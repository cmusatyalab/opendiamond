AM_LFLAGS = -olex.yy.c

noinst_LTLIBRARIES = libodisk.la

libodisk_la_SOURCES = odisk.c read_text_attr.l obj_attr.c odisk_priv.h obj_attr.h lib_odisk.h

CLEANFILES = read_text_attr.c

AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS)
