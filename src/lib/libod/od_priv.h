/*
 * 	OpenDiamond 2.0
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_OD_PRIV_H_
#define	_OD_PRIV_H_ 	    1


#ifdef	__cplusplus
extern "C"
{
#endif

	typedef struct od_srv {
		LIST_ENTRY(od_srv)  ods_id_link;
		CLIENT *            ods_client;
		uint64_t            ods_id;
		char *              ods_name;
	}
	od_srv_t;


	void       ods_init();
	od_srv_t * ods_allocate_by_gid(groupid_t *gid);
	od_srv_t * ods_lookup_by_oid(obj_id_t *oid);
	od_srv_t * ods_lookup_by_devid(uint64_t id);




#ifdef	__cplusplus
}
#endif

#endif	/* !_OD_PRIV_H_ */

