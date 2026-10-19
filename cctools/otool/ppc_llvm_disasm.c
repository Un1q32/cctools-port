#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include <mach-o/ppc/reloc.h>
#include "stuff/bytesex.h"
#include "stuff/symbol.h"
#include "stuff/llvm.h"
#include "otool.h"
#include "dyld_bind_info.h"
#include "ofile_print.h"
#include "ppc_llvm_disasm.h"

/*
 * The classic otool PowerPC disassembler was removed.  PowerPC output now
 * comes from LLVM's public disassembler API, just like the x86 and ARM paths.
 */
static struct ppc_disassemble_info {
    struct relocation_info *relocs;
    uint32_t nrelocs;
    struct nlist *symbols;
    struct nlist_64 *symbols64;
    uint32_t nsymbols;
    struct symbol *sorted_symbols;
    uint32_t nsorted_symbols;
    char *strings;
    uint32_t strings_size;
    uint32_t *indirect_symbols;
    uint32_t nindirect_symbols;
    struct load_command *load_commands;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    enum byte_sex object_byte_sex;
    uint64_t sect_addr;
    enum bool verbose;
} dis_info;

/*
 * ppc_branch_reloc_symbol() returns the symbol name for a PowerPC branch
 * relocation at the specified section offset.  The relocation type is
 * returned indirectly in reloc_type and the addend for a PPC_RELOC_JBSR
 * relocation is returned in offset.
 */
static
const char *
ppc_branch_reloc_symbol(
uint32_t sect_offset,
struct ppc_disassemble_info *info,
uint32_t *reloc_type,
int64_t *offset)
{
    uint32_t i, n_strx, pair_value;
    struct relocation_info *reloc;
    struct scattered_relocation_info *sreloc;

	*reloc_type = 0;
	*offset = 0;
	if(info->relocs == NULL)
	    return(NULL);
	for(i = 0; i < info->nrelocs; i++){
	    reloc = info->relocs + i;
	    if(reloc->r_address & R_SCATTERED)
		continue;
	    if((uint32_t)reloc->r_address != sect_offset)
		continue;
	    if(reloc->r_extern == 0)
		continue;
	    if(reloc->r_type != PPC_RELOC_BR14 &&
	       reloc->r_type != PPC_RELOC_BR24 &&
	       reloc->r_type != PPC_RELOC_JBSR)
		continue;
	    if(reloc->r_symbolnum >= info->nsymbols)
		return(NULL);
	    if(info->symbols64 != NULL)
		n_strx = info->symbols64[reloc->r_symbolnum].n_un.n_strx;
	    else
		n_strx = info->symbols[reloc->r_symbolnum].n_un.n_strx;
	    if(n_strx == 0 || n_strx >= info->strings_size)
		return(NULL);
	    *reloc_type = reloc->r_type;
	    /*
	     * A PPC_RELOC_JBSR relocation is followed by a PPC_RELOC_PAIR
	     * relocation whose address is the addend of the branch.
	     */
	    if(reloc->r_type == PPC_RELOC_JBSR && i + 1 < info->nrelocs){
		reloc = info->relocs + i + 1;
		if(reloc->r_address & R_SCATTERED){
		    sreloc = (struct scattered_relocation_info *)reloc;
		    pair_value = sreloc->r_address;
		}
		else
		    pair_value = reloc->r_address;
		*offset = pair_value;
	    }
	    return(info->strings + n_strx);
	}
	return(NULL);
}

/*
 * ppc_branch_target() returns the target of a direct PowerPC branch
 * instruction.  It returns FALSE for instructions which do not have a direct
 * branch target (such as bclr and bcctr).
 */
static
int
ppc_branch_target(
uint32_t opcode,
uint64_t addr,
uint64_t *target)
{
    int32_t disp;

	if((opcode & 0xfc000000) == 0x48000000){
	    /* I-form: b and bl */
	    disp = (int32_t)(opcode & 0x03fffffc);
	    if(disp & 0x02000000)
		disp |= 0xfc000000;
	}
	else if((opcode & 0xfc000000) == 0x40000000){
	    /* B-form: bc and bcl */
	    disp = (int32_t)(opcode & 0x0000fffc);
	    if(disp & 0x00008000)
		disp |= 0xffff0000;
	}
	else
	    return(FALSE);

	if((opcode & 0x00000002) != 0)
	    *target = 0;
	else
	    *target = addr;
	*target += (int64_t)disp;
	return(TRUE);
}

/*
 * ppc_symbolicate_branch() replaces the numeric target LLVM printed for a
 * direct branch with a symbol name.  LLVM's PowerPC disassembler does not
 * call the LLVM symbolizer callback, so this has to be done here.  External
 * branch relocations in object files are resolved through the relocation
 * entry.  Otherwise the target address is looked up in the symbol table and
 * in the indirect symbol table (for symbol stubs).
 */
static
void
ppc_symbolicate_branch(
char *dst,
size_t dst_size,
uint32_t opcode,
uint64_t addr,
uint32_t sect_offset,
struct ppc_disassemble_info *info)
{
    uint64_t target;
    uint32_t reloc_type;
    int64_t offset;
    const char *symbol, *stub;
    char *space;
    size_t prefix;

	if(ppc_branch_target(opcode, addr, &target) == FALSE)
	    return;

	symbol = ppc_branch_reloc_symbol(sect_offset, info, &reloc_type,
					 &offset);
	if(symbol != NULL){
	    /*
	     * For a BR14 or BR24 relocation the displacement is the addend of
	     * the branch (it is zero for an unmodified external symbol).
	     */
	    if(reloc_type == PPC_RELOC_BR14 || reloc_type == PPC_RELOC_BR24)
		offset = (int64_t)target;
	}
	else{
	    symbol = guess_symbol(target, info->sorted_symbols,
				  info->nsorted_symbols, TRUE);
	    offset = 0;
	}

	stub = NULL;
	if(symbol == NULL){
	    stub = guess_indirect_symbol(target, info->ncmds,
		info->sizeofcmds, info->load_commands, info->object_byte_sex,
		info->indirect_symbols, info->nindirect_symbols, info->symbols,
		info->symbols64, info->nsymbols, info->strings,
		info->strings_size);
	}

	/*
	 * Replace the trailing operand that LLVM printed for the branch.  The
	 * rest of the instruction, including any condition and prediction
	 * suffix, is left exactly as LLVM printed it.
	 */
	space = strrchr(dst, ' ');
	if(space == NULL)
	    return;
	prefix = (size_t)(space - dst) + 1;
	if(symbol != NULL){
	    if(offset != 0)
		snprintf(dst + prefix, dst_size - prefix, "%s+0x%x", symbol,
			 (unsigned int)offset);
	    else
		snprintf(dst + prefix, dst_size - prefix, "%s", symbol);
	}
	else if(stub != NULL){
	    snprintf(dst + prefix, dst_size - prefix,
		     "0x%llx\t; symbol stub for: %s",
		     (unsigned long long)target, stub);
	}
}

/*
 * SymbolLookUp() is passed to LLVMCreateDisasm().  Branch targets are resolved
 * from the symbol table, or from the Mach-O relocation for unlinked objects.
 * LLVM's PowerPC disassembler does not currently use this callback, but it is
 * still provided for versions of LLVM which do.
 */
static
const char *
SymbolLookUp(
void *DisInfo,
uint64_t SymbolValue,
uint64_t *ReferenceType,
uint64_t ReferencePC,
const char **ReferenceName)
{
    struct ppc_disassemble_info *info;
    const char *symbol_name, *reloc_name, *indirect_name;
    uint32_t reloc_type;
    int64_t offset;

	info = (struct ppc_disassemble_info *)DisInfo;
	symbol_name = guess_symbol(SymbolValue, info->sorted_symbols,
				   info->nsorted_symbols, info->verbose);

	*ReferenceName = NULL;
	if(*ReferenceType == LLVMDisassembler_ReferenceType_In_Branch){
	    reloc_name = ppc_branch_reloc_symbol(
		(uint32_t)(ReferencePC - info->sect_addr), info, &reloc_type,
		&offset);
	    if(reloc_name != NULL){
		*ReferenceType = LLVMDisassembler_ReferenceType_InOut_None;
		return(reloc_name);
	    }
	    indirect_name = guess_indirect_symbol(SymbolValue, info->ncmds,
		info->sizeofcmds, info->load_commands, info->object_byte_sex,
		info->indirect_symbols, info->nindirect_symbols, info->symbols,
		info->symbols64, info->nsymbols, info->strings,
		info->strings_size);
	    if(indirect_name != NULL){
		*ReferenceName = indirect_name;
		*ReferenceType =
		    LLVMDisassembler_ReferenceType_Out_SymbolStub;
	    }
	    else
		*ReferenceType = LLVMDisassembler_ReferenceType_InOut_None;
	}
	else
	    *ReferenceType = LLVMDisassembler_ReferenceType_InOut_None;

	return(symbol_name);
}

static
const char *
ppc_cpu_name(
cpu_subtype_t cpusubtype)
{
	switch(cpusubtype & ~CPU_SUBTYPE_MASK){
	case CPU_SUBTYPE_POWERPC_601:
	    return("601");
	case CPU_SUBTYPE_POWERPC_602:
	    return("602");
	case CPU_SUBTYPE_POWERPC_603:
	    return("603");
	case CPU_SUBTYPE_POWERPC_603e:
	    return("603e");
	case CPU_SUBTYPE_POWERPC_603ev:
	    return("603ev");
	case CPU_SUBTYPE_POWERPC_604:
	    return("604");
	case CPU_SUBTYPE_POWERPC_604e:
	    return("604e");
	case CPU_SUBTYPE_POWERPC_620:
	    return("620");
	case CPU_SUBTYPE_POWERPC_750:
	    return("750");
	case CPU_SUBTYPE_POWERPC_7400:
	    return("7400");
	case CPU_SUBTYPE_POWERPC_7450:
	    return("7450");
	case CPU_SUBTYPE_POWERPC_970:
	    return("970");
	default:
	    return("");
	}
}

LLVMDisasmContextRef
create_ppc_llvm_disassembler(
cpu_type_t cputype,
cpu_subtype_t cpusubtype)
{
    const char *triple, *cpu;

	triple = cputype == CPU_TYPE_POWERPC64 ?
		 "powerpc64-apple-darwin" : "powerpc-apple-darwin";
	cpu = mcpu;
	if(*cpu == '\0')
	    cpu = ppc_cpu_name(cpusubtype);

	return(llvm_create_disasm(triple, cpu, &dis_info, 1, NULL,
				  SymbolLookUp));
}

void
delete_ppc_llvm_disassembler(
LLVMDisasmContextRef dc)
{
	llvm_disasm_dispose(dc);
}

uint32_t
ppc_llvm_disassemble(
char *sect,
uint32_t left,
uint64_t addr,
uint64_t sect_addr,
enum byte_sex object_byte_sex,
struct relocation_info *relocs,
uint32_t nrelocs,
struct nlist *symbols,
struct nlist_64 *symbols64,
uint32_t nsymbols,
struct symbol *sorted_symbols,
uint32_t nsorted_symbols,
char *strings,
uint32_t strings_size,
uint32_t *indirect_symbols,
uint32_t nindirect_symbols,
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
cpu_type_t cputype,
enum bool verbose,
LLVMDisasmContextRef dc)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    uint32_t opcode;
    char dst[4096];

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	if(left < sizeof(uint32_t)){
	    if(left != 0){
		memset(&opcode, 0, sizeof(opcode));
		memcpy(&opcode, sect, left);
		if(swapped)
		    opcode = SWAP_INT(opcode);
		printf("\t.long\t0x%08x\n", (unsigned int)opcode);
	    }
	    printf("(end of section)\n");
	    return(left);
	}

	memcpy(&opcode, sect, sizeof(opcode));
	if(swapped)
	    opcode = SWAP_INT(opcode);

	if(!Xflag && jflag && !no_show_raw_insn)
	    printf("\t%08x", opcode);

	dis_info.relocs = relocs;
	dis_info.nrelocs = nrelocs;
	dis_info.symbols = symbols;
	dis_info.symbols64 = symbols64;
	dis_info.nsymbols = nsymbols;
	dis_info.sorted_symbols = sorted_symbols;
	dis_info.nsorted_symbols = nsorted_symbols;
	dis_info.strings = strings;
	dis_info.strings_size = strings_size;
	dis_info.indirect_symbols = indirect_symbols;
	dis_info.nindirect_symbols = nindirect_symbols;
	dis_info.load_commands = load_commands;
	dis_info.ncmds = ncmds;
	dis_info.sizeofcmds = sizeofcmds;
	dis_info.object_byte_sex = object_byte_sex;
	dis_info.sect_addr = sect_addr;
	dis_info.verbose = verbose;

	dst[sizeof(dst) - 1] = '\0';
	if(llvm_disasm_instruction(dc, (uint8_t *)sect, sizeof(uint32_t), addr,
				   dst, sizeof(dst)) != 0){
	    ppc_symbolicate_branch(dst, sizeof(dst), opcode, addr,
				   (uint32_t)(addr - sect_addr), &dis_info);
	    printf("%s\n", dst);
	}
	else
	    printf("\t.long\t0x%08x\n", opcode);

	return(sizeof(uint32_t));
}
