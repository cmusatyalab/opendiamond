/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_ODISK_PRIV_H_
#define	_ODISK_PRIV_H_ 	1


struct odisk_state;


/*
 * XXX we need to clean up this interface so this is not externally 
 * visible.
 */

/*
 * Some macros for using the O_DIRECT call for aligned buffer
 * management.
 */

/* alignment restriction */
#define	OBJ_ALIGN	4096
#define	ALIGN_MASK	(~(OBJ_ALIGN -1))

#define	ALIGN_SIZE(sz)	((sz) + (2 * OBJ_ALIGN))
#define	ALIGN_VAL(base)	(void*)(((uint32_t)(base)+ OBJ_ALIGN - 1) & ALIGN_MASK)
#define	ALIGN_ROUND(sz)	(((sz) + OBJ_ALIGN - 1) & ALIGN_MASK)

/* some maintence functions */
int odisk_write_oids(odisk_state_t * odisk, uint32_t devid);

void obj_load_text_attr(odisk_state_t *odisk, char *file_name, 
	obj_data_t *new_obj);


#endif	/* !_ODISK_PRIV_H_ */

