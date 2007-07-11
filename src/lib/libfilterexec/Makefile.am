AM_LFLAGS = -olex.yy.c

noinst_LTLIBRARIES = libfilterexec.la

libfilterexec_la_LIBADD = $(GLIB2_LIBS)
libfilterexec_la_SOURCES = filter_exec.c filter_spec.l fexec_stats.c \
	fexec_misc.c fexec_opt.c fexec_bypass.c ocache.c cache_filter.c \
	fexec_history.c filter_priv.h fexec_stats.h fexec_opt.h \
	fexec_history.h ocache_priv.h

pkginclude_HEADERS = lib_filterexec.h lib_ocache.h
