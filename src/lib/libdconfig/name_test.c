#include    <sys/types.h>
#include    <unistd.h>
#include    <stdio.h>
#include    <stdint.h>
#include    <assert.h>
#include    "lib_od.h"
#include    "lib_dconfig.h"


#define MAX_GIDS    64

int
enum_names()
{
    void *cookie;
    char *name;
    int         err;

    err = nlkup_first_entry(&name, &cookie);
    assert(err == 0);
    printf("name:  %s \n", name);

    while (1) { 
        err = nlkup_next_entry(&name, &cookie);
        if (err) {
            break;
        }
        printf("name:  %s \n", name);
    }

    return(0);
}

void
dump_gid_list()
{
    void *cookie;
    char *name;
    int         err;
    int         num, i;
    groupid_t   gid_list[MAX_GIDS];
    num = MAX_GIDS;


    err = nlkup_first_entry(&name, &cookie);
    assert(err == 0);

    err = nlkup_lookup_collection(name, &num, gid_list);
    assert(err == 0);
    
    fprintf(stdout, "%s: ", name);
    for (i=0; i < num; i++) {
       fprintf(stdout, "%016llx ", gid_list[i]);
    }
    fprintf(stdout, "\n");


    while (1) { 
        err = nlkup_next_entry(&name, &cookie);
        if (err) {
            break;
        }

        err = nlkup_lookup_collection(name, &num, gid_list);
        assert(err == 0);
        fprintf(stdout, "%s: ", name);
        for (i=0; i < num; i++) {
            fprintf(stdout, "%016llx ", gid_list[i]);
        }
        fprintf(stdout, "\n");
    }

}


int
main(int argc, char **argv)
{
    groupid_t   gid_list[MAX_GIDS];
    int         num, err, i;

    err = enum_names();
    dump_gid_list();

    num = MAX_GIDS;
    err = nlkup_lookup_collection("foo", &num, gid_list);
    if (err) {
        fprintf(stdout, "failed lookup on <foo> \n");
    } else {
        fprintf(stdout, "foo ");
        for (i=0; i < num; i++) {
            printf("%016llx ", gid_list[i]);
        }
        fprintf(stdout, "\n");
    }
#ifdef  XXX
    gid_list[0] = 1;
    err = lookup_add_name("foo1", 1, gid_list);
    assert(err == 0);
#endif
    

    exit(0);
}
