%module OpenDiamond
%include "arrays_java.i"
%include "typemaps.i"
%include "carrays.i"
%include "various.i"

%javaconst(1);

%{
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_dconfig.h"
#include "lib_filter.h"
#include "lib_searchlet.h"

#define MAX_DEV_GROUPS          64

typedef struct device_handle {
        struct device_handle *          next;
        uint32_t                        dev_id;
        char *                          dev_name;
        groupid_t                       dev_groups[MAX_DEV_GROUPS];
        int                             num_groups;
        unsigned int                    flags;
        void *                          dev_handle;
        int                             ver_no;
        time_t                          start_time;
        int                             remain_old;
        int                             remain_mid;
        int                             remain_new;
        float                           done;
        float                           delta;
        float                           prate;
        int                             obj_total;
        float                           cur_credits;    /* credits for current iteration */
        int                             credit_incr;    /* incremental credits to add */
        int                             serviced;       /* times data removed */
        struct                          search_context *        sc;
} device_handle_t;


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

char *deref_char_cookie(char **c) {
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

unsigned char *deref_data_cookie(unsigned char **c) {
  if (c == NULL) {
    return NULL;
  } else {
    return *c;
  }
}

void delete_data_cookie(unsigned char **c) {
  free(c);
}

int get_dev_stats_size(int num_filters) {
  return DEV_STATS_SIZE(num_filters);
}

dev_stats_t *create_dev_stats(int bytes) {
  return malloc(bytes);
}

void delete_dev_stats(dev_stats_t *ds) {
  free(ds);
}

void get_ipv4addr_from_dev_handle(ls_dev_handle_t dev, signed char addr[]) {
  device_handle_t *dhandle = (device_handle_t *) dev;
  int a = dhandle->dev_id;
  *((int *) addr) = a;
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

typedef unsigned int uint32_t;

%array_class(groupid_t, groupidArray);
%array_class(uint32_t, uintArray);

int nlkup_lookup_collection(char *name, int *INOUT, groupidArray *gids);
int glkup_gid_hosts(groupid_t gid, int *INOUT, uintArray *hostids);

ls_search_handle_t ls_init_search(void);
int ls_terminate_search(ls_search_handle_t handle); // stops search
int ls_set_searchlist(ls_search_handle_t handle, int num_groups,
                      groupidArray *glist);
int ls_set_searchlet(ls_search_handle_t handle, device_isa_t isa_type,
                     char *filter_file_name, char *filter_spec_name);
int ls_add_filter_file(ls_search_handle_t handle, device_isa_t isa_type,
                     char *filter_file_name);
int ls_start_search(ls_search_handle_t handle);

int ls_next_object(ls_search_handle_t handle,
                   ls_obj_handle_t *obj_handle,
                   int flags);
int ls_release_object(ls_search_handle_t handle,
                      ls_obj_handle_t obj_handle);

%array_class(ls_dev_handle_t, devHandleArray);
int ls_get_dev_list(ls_search_handle_t handle, devHandleArray *handle_list,
		    int *INOUT);
int ls_get_dev_stats(ls_search_handle_t handle,
                     ls_dev_handle_t dev_handle,
                     dev_stats_t *dev_stats, int *INOUT);


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
char *deref_char_cookie(char **c);
void delete_char_cookie(char **c);
unsigned char **create_data_cookie(void);
byteArray *deref_data_cookie(unsigned char **c);
void delete_data_cookie(unsigned char **c);
int get_dev_stats_size(int num_filters);
dev_stats_t *create_dev_stats(int bytes);
void delete_dev_stats(dev_stats_t *ds);
void get_ipv4addr_from_dev_handle(ls_dev_handle_t dev, signed char addr[]);
