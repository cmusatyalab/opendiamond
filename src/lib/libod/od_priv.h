#ifndef	_OD_PRIV_H_
#define	_OD_PRIV_H_ 	    1


#ifdef	__cplusplus
extern "C" {
#endif

typedef struct od_srv {
    LIST_ENTRY(od_srv)  ods_id_link;
    CLIENT *            ods_client; 
    uint64_t            ods_id;
    char *              ods_name;
} od_srv_t;


void       ods_init();
od_srv_t * ods_allocate_by_gid(groupid_t *gid);
od_srv_t * ods_lookup_by_oid(obj_id_t *oid);




#ifdef	__cplusplus
}
#endif

#endif	/* !_OD_PRIV_H_ */

