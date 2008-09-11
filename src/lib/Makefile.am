SUBDIRS = libtools libdconfig libdctl liblog libodisk libfilter \
	libfilterexec transport libsearchlet libscope

lib_LTLIBRARIES = libopendiamond.la

libopendiamond_la_LDFLAGS = -version-info 3:1:0

libopendiamond_la_SOURCES =

# because we are merging all the libraries below into the massive library here,
# don't LIBADD .la files in subdirectories (or multiple definitions will occur)
libopendiamond_la_LIBADD = libdconfig/libdconfig.la \
	libdctl/libdctl.la libfilter/libfilter.la \
	libfilterexec/libfilterexec.la liblog/liblog.la \
	libodisk/libodisk.la libsearchlet/libsearchlet.la \
	libtools/libtools.la transport/libtransport.la \
	libscope/libscope.la
