bin_PROGRAMS =  \
	dyldinfo \
	ObjectDump \
	unwinddump \
	machocheck

AM_CXXFLAGS = \
	-D__DARWIN_UNIX03 \
	$(WARNINGS) \
	-Wno-switch \
	$(ENDIAN_FLAG) \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/foreign \
	-I$(top_srcdir)/ld64/src \
	-I$(top_srcdir)/ld64/src/3rd \
	-I$(top_srcdir)/ld64/src/abstraction \
	-I$(top_srcdir)/ld64/src/ld \
	-I$(top_srcdir)/ld64/src/ld/parsers

AM_CFLAGS = \
	-D__DARWIN_UNIX03 \
	$(WARNINGS) \
	$(ENDIAN_FLAG) \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/foreign \
	-I$(top_srcdir)/ld64/src \
	-I$(top_srcdir)/ld64/src/3rd \
	-I$(top_srcdir)/ld64/src/abstraction \
	-I$(top_srcdir)/ld64/src/ld \
	-I$(top_srcdir)/ld64/src/ld/parsers

unwinddump_SOURCES = unwinddump.cpp
machocheck_SOURCES = machochecker.cpp
ObjectDump_SOURCES = \
	ObjectDump.cpp \
	$(top_srcdir)/ld64/src/ld/debugline.c
ObjectDump_LDADD = \
	$(top_srcdir)/ld64/src/3rd/libhelper.la

dyldinfo_SOURCES = dyldinfo.cpp
dyldinfo_LDADD = \
	$(top_srcdir)/ld64/src/3rd/libhelper.la
