
#include <stdio.h>
#include "rstat.h"

int
main(int argc, char **argv)
{
    u_int64_t       freq,
                    mem;

    r_cpu_freq(&freq);
    r_freemem(&mem);
    printf("cpu frequency = %llu\n", freq);
    printf("memory = %llu KB\n", mem / 1000);

    return 0;
}
