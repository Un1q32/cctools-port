bin_PROGRAMS = ld
ld_LDADD =  \
    $(top_srcdir)/ld64/src/3rd/libhelper.la  \
    $(DL_LIB) \
    $(UUID_LIB) \
    $(LTO_RPATH) \
    $(LTO_LIB)

ld_LDFLAGS = $(PTHREAD_FLAGS) $(EXECINFO_LIB)

ld_CXXFLAGS =  \
	-D__DARWIN_UNIX03 \
	$(WARNINGS) \
	$(LTO_DEF) \
	$(ENDIAN_FLAG) \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/foreign \
	-I$(top_srcdir)/ld64/src \
	-I$(top_srcdir)/ld64/src/abstraction \
	-I$(top_srcdir)/ld64/src/3rd \
	-I$(top_srcdir)/ld64/src/3rd/BlocksRuntime \
	-I$(top_srcdir)/ld64/src/3rd/include \
	-I$(top_srcdir)/ld64/src/ld \
	-I$(top_srcdir)/ld64/src/ld/parsers

ld_CFLAGS =  \
	-D__DARWIN_UNIX03 \
	$(WARNINGS) \
	$(LTO_DEF) \
	$(ENDIAN_FLAG) \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/foreign \
	-I$(top_srcdir)/ld64/src \
	-I$(top_srcdir)/ld64/src/abstraction \
	-I$(top_srcdir)/ld64/src/3rd \
	-I$(top_srcdir)/ld64/src/3rd/BlocksRuntime \
	-I$(top_srcdir)/ld64/src/3rd/include \
	-I$(top_srcdir)/ld64/src/ld \
	-I$(top_srcdir)/ld64/src/ld/parsers

ld_SOURCES =  \
	debugline.c  \
	ld.cpp  \
	Options.cpp
