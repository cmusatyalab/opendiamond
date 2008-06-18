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

#ifndef _LIB_SCOPE_H_
#define	_LIB_SCOPE_H_

#include <diamond_features.h>

/*!  
 * \defgroup scope  Metadata Scoping API
 * The metadata scoping API is a programming interface between the core
 * OpenDiamond system and the Diamond applications running on the host. All 
 * metadata scoping operations with OpenDiamond are performed through this 
 * interface.  The programming interface is defined in lib_scope.h.
 *
 * */


#ifdef __cplusplus
extern "C"
{
#endif

/*!
 * \file lib_scope.h
 * \ingroup scope
 * This defines the API that applications use to set the scope of
 * future searches in the OpenDiamond system.
 */

diamond_public
int ls_define_scope(void);


#ifdef __cplusplus
}
#endif


#endif
