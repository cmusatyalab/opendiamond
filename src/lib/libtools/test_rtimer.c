
#include <stdio.h>
#include <math.h>
#include "rtimer.h"

int
main(int argc, char **argv) {

  rtimer_t rt;
  int i;
  double x;

  while(1) {
    rt_start(&rt);
    x = 1.14;
    for(i=0; i<1000000; i++) {
      x *= 1.2 + sqrt(i);
    }
    rt_stop(&rt);
    printf("%f\n", rt_seconds(&rt));
  }

  return 0;
}
