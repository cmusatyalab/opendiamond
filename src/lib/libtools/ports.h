/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _PORTS_H_
#define _PORTS_H_

#include <stdint.h>

uint16_t
diamond_get_control_port(void);

uint16_t
diamond_get_data_port(void);

uint16_t
diamond_get_log_port(void);

#endif                           /* _PORTS_H_ */
