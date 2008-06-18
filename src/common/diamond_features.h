/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */


#ifndef DIAMOND_FEATURES_H
#define DIAMOND_FEATURES_H


// for exporting from shared libraries or DLLs
#if defined WIN32
#  ifdef BUILDING_DLL
#    define diamond_public __declspec(dllexport)
#  else
#    define diamond_public __declspec(dllimport)
#  endif
#elif __GNUC__ > 3
# define diamond_public __attribute((visibility("default")))
#else
# define diamond_public
#endif



#endif
