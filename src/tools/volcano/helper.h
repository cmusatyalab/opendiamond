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

int is_file(char *path);
int is_dir(char *path);
int binary_to_hex_string(int len, char *dest, int dest_len, 
			 unsigned char *data, int data_len);
char *strip_username(char *src);
char *parse_extension(char *src);
int insert_gid_colons(char *dest, char *src);
