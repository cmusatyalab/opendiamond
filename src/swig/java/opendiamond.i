%module OpenDiamond
%include "typemaps.i"
%include "various.i"
%include "cpointer.i"

%{
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_dconfig.h"
#include "lib_filter.h"
#include "lib_searchlet.h"



void **create_void_cookie(void) {
  return (void**) malloc(sizeof(void *));
}

void delete_void_cookie(void **c) {
  free(c);
}

char **create_char_cookie(void) {
  return (char**) malloc(sizeof(char *));
}

const char *deref_char_cookie(char **c) {
  if (c == NULL) {
    return NULL;
  } else {
    return *c;
  }
}

void delete_char_cookie(char **c) {
  free(c);
}
%}

%pragma(java) jniclasscode=%{
  static {
    try {
        System.loadLibrary("OpenDiamond");
    } catch (UnsatisfiedLinkError e) {
      System.err.println("Native code library failed to load. \n" + e);
    }
  }
%}



%include "diamond_consts.h"
%include "diamond_types.h"
%include "lib_dconfig.h"
%include "lib_filter.h"
%include "lib_searchlet.h"

void **create_void_cookie(void);
void delete_void_cookie(void **c);
char **create_char_cookie(void);
const char *deref_char_cookie(char **c);
void delete_char_cookie(char **c);
