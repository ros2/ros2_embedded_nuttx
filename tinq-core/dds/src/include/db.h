/*
 * Copyright (c) 2014 - Qeo LLC
 *
 * The source code form of this Qeo Open Source Project component is subject
 * to the terms of the Clear BSD license.
 *
 * You can redistribute it and/or modify it under the terms of the Clear BSD
 * License (http://directory.fsf.org/wiki/License:ClearBSD). See LICENSE file
 * for more details.
 *
 * The Qeo Open Source Project also includes third party Open Source Software.
 * See LICENSE file for more details.
 */

/* db.h -- Defines the contents and access methods for data buffers. */

#ifndef __db_h_
#define __db_h_

#include "pool.h"
#include "dds/dds_error.h"

#define	DB_OK		DDS_RETCODE_OK
#define	DB_ERR_PARAM	DDS_RETCODE_BAD_PARAMETER
#define	DB_ERR_NOMEM	DDS_RETCODE_OUT_OF_RESOURCES

typedef struct data_buffer_st DB;

#define	MIN_DB_SIZE	64		/* Minimum size of a data buffer. */
#define	DB_HDRSIZE	(sizeof (DB) - MIN_DB_SIZE)	/* Overhead size. */

/* Data buffer: */
struct data_buffer_st {
	DB		*next;		/* Next data buffer. */
#if defined (BIGDATA) || (WORDSIZE == 64)
	size_t		size;		/* Size of the data area. */
	unsigned 	pool;		/* Pool index of block. */
	unsigned 	nrefs;		/* Pool index of block. */
#else
	unsigned short	size;		/* Size of the data area. */
	unsigned char	pool;		/* Pool index of block. */
	unsigned char	nrefs;		/* # of references to this block. */
#endif
	unsigned char	data [MIN_DB_SIZE]; /* Data storage. */
};

/* Data buffer pool: */
typedef struct db_pool_desc_st {
	POOL_LIMITS	msgdata;	/* Message data buffer pool limits. */
	size_t		maxmsgdata;	/* Size of each Message data buffer. */
} DB_POOL;

extern MEM_DESC mdata_pool;	/* Pool for large data buffers for Rx. */


int db_pool_init (unsigned npools,	/* # of data buffer pools. */
		  DB_POOL  *pools);	/* Data buffer pool descriptors. */

/* Initialize the data buffer pools. */

void db_pool_free (void);

/* Free the data buffer pools. */

DB *db_alloc_data (size_t size, int linear);

/* Allocate a data buffer chain to store the given data size.
   We attempt to allocate buffers from the buffer pools if possible.  However,
   if the internal pools are low on memory and it is allowed, we allocate
   directly from the heap.
   If the 'linear' argument is given, the allocator is forced to return a single
   buffer that is large enough to contain all data.  Otherwise, a chain of
   smaller buffers might be returned. */

DB *db_alloc_rx (void);

/* Allocate a data buffer suitable for reception (i.e. large enough). */

void db_free_data (DB *dp);

/* Free an allocated data buffer chain. */


void db_put_data (DB *dbp, size_t ofs, const void *sp, size_t length);

/* Copy a linear buffer {sp, length} to an allocated buffer chain {dbp} at the
   given offset in the buffer chain. */

void db_get_data (void       *dp,
		  const DB   *dbp,
		  const void *data,
		  size_t     ofs,
		  size_t     length);

/* Copy data from a bufffer chain {dbp,data,ofs,length} to a linear data buffer
   {dp,length}. */

void db_pool_dump (size_t sizes []);

/* Display pool data and statistics. */

void db_xpool_stats (void);

/* Display dynamic memory statistics of the data buffer pools. */  


/* Utility structure used to walk over/parse data in a chain of data buffers. */
typedef struct db_walk_st {
	const DB		*dbp;		/* Data buffer pointer. */
	const unsigned char	*data;		/* Data chunk (in buffer). */
	size_t			left;		/* Data chunk size (in buffer). */
	size_t			length;		/* Total data length. */
} DBW;

#ifdef CDR_ONLY
#define DBW_INC(w,n)	(w).data += n; (w).left -= n; (w).length -= n
#else
#define DBW_INC(w,n)	if ((w).left > n) {			\
				(w).data += n; (w).left -= n; 	\
			} else dbw_inc (&w, n);	(w).length -= n
#endif

/* Increment a walker past the current point. */

#define	DBW_END(w)	!(w).length

/* Verify if there is data remaining. */

#ifdef CDR_ONLY
#define	DBW_PTR(w)	(w).data
#else
#define	DBW_PTR(w)	((w).left) ? (w).data : dbw_inc (&w, 0)
#endif

/* Assign the current walker point to a pointer variable. */

#define	DBW_REMAIN(w)	(w).length

/* Return the number of bytes remaining after the pointer. */

const unsigned char *dbw_inc (DBW *p, size_t n);

/* Increment the walk pointer n bytes. */

#endif /* !__db_h_ */

