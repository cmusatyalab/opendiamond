#include <stdio.h>
#include <rpc/rpc.h>     /* always needed  */
#include <assert.h>     /* always needed  */
#include <stdio.h>     /* always needed  */
#include <ctype.h>	// For isxdigit
#include "lib_od.h"

// Modified by rahuls 2003.05.13 to be take a file and gid
// from the command-line and insert it into the object database
//
// Program now prints (to stdout) the OID of the object that
// was inserted, in xx:xx:xx:xx:xx:xx:xx:xx form.
// This program is used by the perl script "populate-disk.pl"

#define BLOCK_SIZE      4096

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


//
// REWRITE GENERICALLY AS A ROUTINE TO PARSE/PRINT UIN64s
// TODO TODO TODO
// [replace the three functions below thusly]

// Parses a uint64_t string of the form xx:xx:xx:xx:xx:xx:xx:xx
// where "xx" is a hex byte.
//
uint64_t parse_uint64_string(const char* s) {
  int i, o;
  unsigned int x;	// Will actually hold an unsigned char
  uint64_t u = 0u;

  /*
  sscanf(s, "%llx", &u);
  printf("parsed gid is 0x%llx\n", u);
  return u;
  */

  assert(s);
  fprintf(stderr, "parse_uint64_string s = %s\n", s);
  for (i=0; i<8; i++) {
    o = 3*i;
    assert(isxdigit(s[o]) && isxdigit(s[o+1]));
    assert( (s[o+2] == ':') || (s[o+2] == '\0') );
    sscanf(s+o, "%2x", &x);
    u <<= 8;
    u += x;
  }
  // printf("parsed uint64_t is 0x%llx\n", u);
  return u;
}

// Prints a uint64_t as xx:xx:xx:xx:xx:xx:xx:xx
// to stdout.  Really, I should stick it into a char buf
// but then someone has to deal with allocating space etc.
// TODO
void print_uint64(uint64_t u) {
  int i;
  for (i=0; i<8; i++) {
    unsigned int x = u >> 56;
    if (i>0) { printf(":"); }	// separator
    printf("%02x", x);
    u <<= 8;
  }
}

inline groupid_t parse_gid_string(const char* s) {
  return parse_uint64_string(s);
}

inline void print_gid(groupid_t gid) {
  print_uint64(gid);
}

obj_id_t parse_oid_string(const char* s) {
  obj_id_t oid;
  oid.dev_id	= parse_uint64_string(s);
  assert(s[23] == ':');
  oid.local_id	= parse_uint64_string(s+24);
  return oid;
}

inline void print_oid(obj_id_t oid) {
  print_uint64(oid.dev_id);
  printf(":");
  print_uint64(oid.local_id);
}


int
main(int argc, char **argv)
{
#if 0
  obj_id_t uu;
  uu = parse_oid_string(argv[1]);
  printf("oid = "); print_oid(uu); printf("\n");
  return 0;
#endif // 0

    groupid_t   gid;
//    groupid_t   newgid;
    int         err;
    obj_id_t    oid;
    char*	file;
    int		i;

    // argc must be odd, and >= 3
    if ((argc < 3) || (argc%2 == 0)) {
      fprintf(stderr,
	  "Usage: %s file gid [attr1 val1] [attr2 val2]...\n", argv[0]);
      fprintf(stderr, " file is a legal path to a real file\n");
      fprintf(stderr, " gid is in the form 9c:a1:7d:a9:f2:60:61:92\n");
      fprintf(stderr, " [attrn valn] are attribute value pairs of strings\n");
      fprintf(stderr, "%s prints the OID of new object to stdout\n", argv[0]);
      exit(1);
    }
    file = argv[1];
    gid = parse_gid_string(argv[2]);
    printf("parsed gid as 0x%llx\n", gid);

    err = od_init();
    assert(err == 0);
    err = od_create(&oid, &gid);
    assert(err == 0);

    printf("OID = "); print_oid(oid); printf("\n");

    copy_file(file, &oid);

    /*
    copy_file("dummy_file", &oid);
    fetch_file("and_back", &oid);

    newgid = 0x12345678;
    err = od_add_gid(&oid, &newgid);
    assert(err == 0);
    */

    // We assume remaining arguments are all attr/val pairs
    //
    for (i=3; i<argc; ) {
      const char* key = argv[i++];
      const char* val = argv[i++];
      int valsize = strlen(val);
      err = od_set_attr(&oid, key, valsize, (char *)val);
      assert(err == 0);
    }

    /*
    x = 1;
    err = od_set_attr(&oid, "foo", sizeof(x), (char *)&x);
    assert(err == 0);
    */

    exit(0);
}
