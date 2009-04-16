/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _DCONFIG_PRIV_H_
#define _DCONFIG_PRIV_H_

typedef enum {
	DATA_TYPE_OBJECT = 1,
	DATA_TYPE_NATIVE
} data_type_t;


FILE * dconfig_open_config_file(const char *conf_file);
char * dconf_get_logdir(void);
char * dconf_get_cachedir(void);
char * dconf_get_spec_cachedir(void);
char * dconf_get_binary_cachedir(void);
data_type_t	dconf_get_datatype(void);



diamond_public
char * dconf_get_blob_cachedir(void);

diamond_public
char * dconf_get_filter_cachedir(void);



#ifdef __cplusplus
}
#endif
#endif                          /* !_DCONFIG_PRIV_H_ */

