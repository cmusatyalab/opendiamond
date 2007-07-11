noinst_LTLIBRARIES = libauth.la

libauth_la_SOURCES = lib_auth.c lib_auth_priv.h

pkginclude_HEADERS = lib_auth.h

noinst_PROGRAMS = actest astest

actest_SOURCES = actest.c
actest_LDADD = libauth.la ../liblog/liblog.la \
	../libdctl/libdctl.la ../libdconfig/libdconfig.la \
	${GLIB2_LIBS}

astest_SOURCES = astest.c
astest_LDADD = libauth.la ../liblog/liblog.la \
	../libdctl/libdctl.la ../libdconfig/libdconfig.la \
	${GLIB2_LIBS}

