/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 5
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2010 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef OPENDIAMOND_SRC_FILTER_RUNNER_FILTER_RUNNER_H_
#define OPENDIAMOND_SRC_FILTER_RUNNER_FILTER_RUNNER_H_

extern const char *_filter_name;
extern FILE *_in;
extern FILE *_out;


void start_output(void);
void end_output(void);


#endif
