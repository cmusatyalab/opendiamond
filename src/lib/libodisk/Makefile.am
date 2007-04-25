AM_LFLAGS = -olex.yy.c

noinst_LTLIBRARIES = libodisk.la

libodisk_la_SOURCES = odisk.c read_text_attr.l obj_attr.c odisk_priv.h

pkginclude_HEADERS = lib_odisk.h obj_attr.h


noinst_PROGRAMS = odisk_test attr_test odisk_idx del_obj odisk_setoid

odisk_test_SOURCES = odisk_test.c
odisk_test_LDADD = libodisk.la ../libtools/libtools.la \
	../libdconfig/libdconfig.la \
	../libdctl/libdctl.la ../liblog/liblog.la \
	../libtools/libtools.la

attr_test_SOURCES = attr_test.c
attr_test_LDADD = libodisk.la ../libtools/libtools.la \
	../libdconfig/libdconfig.la \
	../libdctl/libdctl.la ../liblog/liblog.la \
	../libtools/libtools.la

odisk_idx_SOURCES = odisk_idx.c
odisk_idx_LDADD = libodisk.la ../libtools/libtools.la \
	../libdconfig/libdconfig.la \
	../libdctl/libdctl.la ../liblog/liblog.la \
	../libtools/libtools.la

del_obj_SOURCES = del_obj.c
del_obj_LDADD = libodisk.la ../libtools/libtools.la \
	../libdconfig/libdconfig.la \
	../libdctl/libdctl.la ../liblog/liblog.la \
	../libtools/libtools.la

odisk_setoid_SOURCES = odisk_setoid.c
odisk_setoid_LDADD = libodisk.la ../libtools/libtools.la \
	../libdconfig/libdconfig.la \
	../libdctl/libdctl.la ../liblog/liblog.la \
	../libtools/libtools.la
