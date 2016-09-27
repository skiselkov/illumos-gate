/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */



/*-*************************************
*  Dependencies
***************************************/
#include <sys/kmem.h>
#include "error_private.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "../zstd.h"           /* declaration of ZSTD_isError, ZSTD_getErrorName, ZSTD_getErrorCode, ZSTD_getErrorString, ZSTD_versionNumber */
#include "zbuff.h"          /* declaration of ZBUFF_isError, ZBUFF_getErrorName */


/*-****************************************
*  Version
******************************************/
unsigned ZSTD_versionNumber (void) { return ZSTD_VERSION_NUMBER; }


/*-****************************************
*  ZSTD Error Management
******************************************/
/*! ZSTD_isError() :
*   tells if a return value is an error code */
unsigned ZSTD_isError(size_t code) { return ERR_isError(code); }

/*! ZSTD_getErrorName() :
*   provides error code string from function result (useful for debugging) */
const char* ZSTD_getErrorName(size_t code) { return ERR_getErrorName(code); }

/*! ZSTD_getError() :
*   convert a `size_t` function result into a proper ZSTD_errorCode enum */
ZSTD_ErrorCode ZSTD_getErrorCode(size_t code) { return ERR_getErrorCode(code); }

/*! ZSTD_getErrorString() :
*   provides error code string from enum */
const char* ZSTD_getErrorString(ZSTD_ErrorCode code) { return ERR_getErrorName(code); }


/* **************************************************************
*  ZBUFF Error Management
****************************************************************/
unsigned ZBUFF_isError(size_t errorCode) { return ERR_isError(errorCode); }

const char* ZBUFF_getErrorName(size_t errorCode) { return ERR_getErrorName(errorCode); }



/*=**************************************************************
*  Custom allocator
****************************************************************/
/* default uses stdlib */
/*ARGSUSED*/
void* ZSTD_defaultAllocFunction(void* opaque, size_t size)
{
	return ((void*)0);
}

/*ARGSUSED*/
void ZSTD_defaultFreeFunction(void* opaque, void* address)
{
}

/*ARGSUSED*/
void* ZSTD_malloc(size_t size, ZSTD_customMem customMem)
{
    uint8_t* address = kmem_alloc(size + sizeof (size_t), KM_SLEEP);
    *(size_t*)address = size;
    return address + sizeof (size_t);
}

/*ARGSUSED*/
void ZSTD_free(void* ptr, ZSTD_customMem customMem)
{
    uint8_t *addr = ((uint8_t *)ptr) - sizeof (size_t);
    size_t size = *(size_t *)addr;
    kmem_free(addr, size);
}
