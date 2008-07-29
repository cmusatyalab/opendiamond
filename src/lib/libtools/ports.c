/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "ports.h"


static uint16_t
get_base_port_from_env(void)
{
  uint16_t result = 5872;       // default

  const char *port_string = getenv("DIAMOND_PORT_BASE");
  if (port_string != NULL) {
    // try to parse
    char *endptr;
    long parse_result = strtol(port_string, &endptr, 10);
    if (port_string == endptr || parse_result < 1 || parse_result > 65535) {
      // bad
      printf("cannot understand DIAMOND_PORT_BASE value \"%s\"\n", port_string);
    } else {
      result = parse_result;
    }
  }

  return result;
}

const char *
diamond_get_control_port(void)
{
    static char port[6];
    sprintf(port, "%u", get_base_port_from_env());
    return port;
}

const char *
diamond_get_data_port(void)
{
    static char port[6];
    sprintf(port, "%u", get_base_port_from_env() + 1);
    return port;
}

