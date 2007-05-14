%module OpenDiamond
%include "arrays_java.i"
%include "typemaps.i"
%include "carrays.i"

%javaconst(1);

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

void *deref_void_cookie(void **c) {
  return *c;
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

unsigned char **create_data_cookie(void) {
  return (unsigned char**) malloc(sizeof(unsigned char *));
}

const unsigned char *deref_data_cookie(unsigned char **c) {
  if (c == NULL) {
    return NULL;
  } else {
    return *c;
  }
}

void delete_data_cookie(unsigned char **c) {
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

int nlkup_first_entry(char **name, void **cookie);
int nlkup_next_entry(char **name, void **cookie);

%array_class(groupid_t, groupidArray);

int nlkup_lookup_collection(char *name, int *INOUT, groupidArray *gids);


ls_search_handle_t ls_init_search(void);
int ls_terminate_search(ls_search_handle_t handle);  // destroys everything
int ls_set_searchlist(ls_search_handle_t handle, int num_groups,
                      groupidArray *glist);
int ls_set_searchlet(ls_search_handle_t handle, device_isa_t isa_type,
                     char *filter_file_name, char *filter_spec_name);
int ls_add_filter_file(ls_search_handle_t handle, device_isa_t isa_type,
                     char *filter_file_name);
int ls_start_search(ls_search_handle_t handle);
int ls_abort_search(ls_search_handle_t handle);  // you can start again after

int ls_next_object(ls_search_handle_t handle,
                   ls_obj_handle_t *obj_handle,
                   int flags);
int ls_release_object(ls_search_handle_t handle,
                      ls_obj_handle_t obj_handle);


int ls_get_dev_stats(ls_search_handle_t handle,
                     ls_dev_handle_t  dev_handle,
                     dev_stats_t *dev_stats, int *stat_len);


typedef	void *	lf_obj_handle_t;
typedef unsigned int  size_t;
int lf_next_block(lf_obj_handle_t obj_handle, int num_blocks,
			size_t *OUTPUT, unsigned char **data);
int lf_ref_attr(lf_obj_handle_t ohandle, const char *name,
		size_t *OUTPUT, unsigned char **data);
int lf_first_attr(lf_obj_handle_t ohandle, char **name,
		size_t *OUTPUT, unsigned char **data, void **cookie);
int lf_next_attr(lf_obj_handle_t ohandle, char **name,
		size_t *OUTPUT, unsigned char **data, void **cookie);

%array_class(unsigned char, byteArray);

void **create_void_cookie(void);
void delete_void_cookie(void **c);
void *deref_void_cookie(void **c);
char **create_char_cookie(void);
const char *deref_char_cookie(char **c);
void delete_char_cookie(char **c);
unsigned char **create_data_cookie(void);
const byteArray *deref_data_cookie(unsigned char **c);
void delete_data_cookie(unsigned char **c);
