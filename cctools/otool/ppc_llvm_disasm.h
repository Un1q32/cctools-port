#ifndef _OTOOL_PPC_LLVM_DISASM_H_
#define _OTOOL_PPC_LLVM_DISASM_H_

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"
#include "stuff/symbol.h"
#include "llvm-c/Disassembler.h"

extern LLVMDisasmContextRef create_ppc_llvm_disassembler(
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype);

extern void delete_ppc_llvm_disassembler(LLVMDisasmContextRef dc);

extern uint32_t ppc_llvm_disassemble(
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
    LLVMDisasmContextRef dc);

#endif /* _OTOOL_PPC_LLVM_DISASM_H_ */
