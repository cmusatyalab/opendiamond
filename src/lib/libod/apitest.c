#include <stdio.h>
#include <rpc/rpc.h>     /* always needed  */
#include <assert.h>     /* always needed  */
#include <stdio.h>     /* always needed  */
#include "lib_od.h"

#define BLOCK_SIZE      512

void
copy_file(char *fname, obj_id_t *oid)
{
    int     offset, len;
    int     err;
    char *  buf;
    FILE *  src_file;

    buf = (char *)malloc(BLOCK_SIZE);
    assert(buf != NULL);

   
    src_file = fopen(fname, "r");
    assert(src_file != NULL);

    offset = 0;

    while ((len = fread(buf, 1, BLOCK_SIZE, src_file)) != 0) {
        err = od_write(oid, offset, len, buf);
        assert(err == 0);
        offset += len;
    }

    printf("done with copy len %d off %d\n", len, offset);

}

void
fetch_file(char *fname, obj_id_t *oid)
{
    int     offset, len;
    int     err;
    char *  buf;
    FILE *  dst_file;

    buf = (char *)malloc(BLOCK_SIZE);
    assert(buf != NULL);

   
    dst_file = fopen(fname, "w");
    assert(dst_file != NULL);

    offset = 0;

    while ((len = od_read(oid, offset, BLOCK_SIZE, buf)) != 0) {
        err = fwrite(buf, len, 1, dst_file);
        assert(err == 1);
        offset += len;
    }

    assert(len != -1);
    fclose(dst_file);
}


int
main(int argc, char **argv)
{

    groupid_t   gid;
    groupid_t   newgid;
    int         err;
    obj_id_t    oid;
    int         x;


    gid = 1;

    err = od_init();
    assert(err == 0);
    err = od_create(&oid, &gid);
    assert(err == 0);


    copy_file("dummy_file", &oid);
    fetch_file("and_back", &oid);

    newgid = 0x12345678;
    err = od_add_gid(&oid, &newgid);
    assert(err == 0);

    x = 1;
    err = od_set_attr(&oid, "foo", sizeof(x), (char *)&x);
    assert(err == 0);

    exit(0);
}


