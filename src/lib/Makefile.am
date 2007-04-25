SUBDIRS = libtools libdconfig libdctl liblog libodisk libfilter \
	libfilterexec libod libauth transport libsearchlet

lib_LTLIBRARIES = libopendiamond.la

libopendiamond_la_SOURCES =

# because we are merging all the libraries below into the massive library here,
# don't LIBADD .la files in subdirectories (or multiple definitions will occur)
libopendiamond_la_LIBADD = libauth/libauth.la libdconfig/libdconfig.la \
	libdctl/libdctl.la libfilter/libfilter.la \
	libfilterexec/libfilterexec.la liblog/liblog.la libod/libod.la \
	libodisk/libodisk.la libsearchlet/libsearchlet.la \
	libtools/libtools.la transport/libtransport.la
