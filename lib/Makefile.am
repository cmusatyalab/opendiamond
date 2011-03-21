SUBDIRS = libtools libdconfig liblog libscope libodisk libfilter \
	libfilterexec transport

noinst_LTLIBRARIES = libopendiamond.la

libopendiamond_la_SOURCES =

# because we are merging all the libraries below into the massive library here,
# don't LIBADD .la files in subdirectories (or multiple definitions will occur)
libopendiamond_la_LIBADD = libdconfig/libdconfig.la \
	libfilter/libfilter.la \
	libfilterexec/libfilterexec.la liblog/liblog.la \
	libodisk/libodisk.la \
	libtools/libtools.la transport/libtransport.la \
	libscope/libscope.la
