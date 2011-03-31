SUBDIRS = libtools libdconfig liblog libscope libodisk libfilter \
	libfilterexec python transport

noinst_LTLIBRARIES = libopendiamond.la

libopendiamond_la_SOURCES =

# because we are merging all the libraries below into the massive library here,
# don't LIBADD .la files in subdirectories (or multiple definitions will occur)
# note that libfilter is built into a separate library and not included here
libopendiamond_la_LIBADD = libdconfig/libdconfig.la \
	libfilterexec/libfilterexec.la liblog/liblog.la \
	libodisk/libodisk.la \
	libtools/libtools.la transport/libtransport.la \
	libscope/libscope.la
