noinst_LTLIBRARIES = libauth.la

libauth_la_SOURCES =
if HAVE_KRB5
libauth_la_SOURCES += lib_auth.c lib_auth_priv.h
else
libauth_la_SOURCES += lib_noauth.c
endif

pkginclude_HEADERS = lib_auth.h
