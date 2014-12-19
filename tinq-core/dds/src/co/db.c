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

/* db.c -- Implements the contents and access methods for data buffers. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "thread.h"
#include "vgdefs.h"
#include "log.h"
#include "pool.h"
#include "error.h"
#include "db.h"

#define	MAX_DB_POOLS	8	/* Max. # of Data buffer pools.
				   Not all of these are needed, but at least 2
				   seems reasonable. */
/*#define CHECK_ALLOC		** Define this to verify allocated buffer
				   integrity. */
/*#define CLEAR_ON_FREE		** Define to reset data field on release. */

#ifdef TRC_DB_ALLOCS
#define TRC_DB(cp,s,alloc)	log_printf (DB_ID, 0, "%s(%p) %s\r\n", (alloc) ? "alloc_db" : "free_db", cp, s);
#else
#define	TRC_DB(cp,s,alloc)
#endif

enum mem_block_en {
	MB_DATA_BUF_0,		/* Data buffer (largest size). */
	MB_DATA_BUF_1,		/* idem (smaller than #0). */
	MB_DATA_BUF_2,		/* idem (smaller than #1). */
	MB_DATA_BUF_3,		/* idem (smaller than #2). */
	MB_DATA_BUF_4,		/* idem (smaller than #3). */
	MB_DATA_BUF_5,		/* idem (smaller than #4). */
	MB_DATA_BUF_6,		/* idem (smaller than #5). */
	MB_DATA_BUF_7,		/* idem (smallest size). */

	MB_END
};

static const char *mem_names [] = {
	"DATA_BUF(0)",
	"DATA_BUF(1)",
	"DATA_BUF(2)",
	"DATA_BUF(3)",
	"DATA_BUF(4)",
	"DATA_BUF(5)",
	"DATA_BUF(6)",
	"DATA_BUF(7)"
};

static MEM_DESC_ST	mem_blocks [MB_END];	/* Memory used by driver. */
static size_t		mem_size;		/* Total memory allocated. */
static unsigned		num_db_pools;		/* # of data buffer pools. */
MEM_DESC 		mdata_pool = mem_blocks; /* Pool for Rx data buffers. */
static uint32_t		cur_balloc_blocks;	/* Cur. # of alloced blocks. */
static uint32_t		max_balloc_blocks;	/* Max. # of alloced blocks. */
static uint32_t		cur_lalloc_blocks;	/* Cur. # of large allocs. */
static uint32_t		max_lalloc_blocks;	/* Max. # of large allocs. */
STATIC_ULLONG		(cur_balloc_size);	/* Cur. # of alloced bytes. */
STATIC_ULLONG		(max_balloc_size);	/* Max. # of alloced bytes. */
static uint32_t		num_ballocs;		/* Total # of allocs. */
static uint32_t		num_lallocs;		/* Total # of large allocs. */
static uint32_t		num_bfrees;		/* Total # of frees. */
static uint32_t		num_lfrees;		/* Total # of large frees. */
static lock_t 		db_lock;		/* Lock for DB pool access. */
static lock_t 		stats_lock;		/* Lock for statistic updates. */

/* db_pool_init -- Initialize the data buffer pools. */

int db_pool_init (unsigned npools,	/* # of data buffer pools. */
		  DB_POOL  *pools)	/* Data buffer pool descriptors. */
{
	DB_POOL		*pdp;
#ifndef FORCE_MALLOC
	DB		*dbp;
	MEM_DESC	mdp;
#endif
	unsigned	i, size;

	if (mem_blocks [0].md_addr) {	/* Was already initialized -- reset. */
		mds_reset (mem_blocks, MB_END);
		return (DB_OK);
	}

	if (!npools || npools > MAX_DB_POOLS)
		return (DB_ERR_PARAM);

	size = 0xfff0;
	for (i = 0, pdp = pools; i < npools; i++, pdp++) {
		if (!pdp->maxmsgdata || !pdp->msgdata.reserved || pdp->maxmsgdata >= size)
			return (DB_ERR_PARAM);

		size = pdp->maxmsgdata + DB_HDRSIZE;
		MDS_POOL_TYPE (mem_blocks, MB_DATA_BUF_0 + i, pdp->msgdata, size);
	}

	/* Allocate all pools in one go. */
	mem_size = mds_alloc (mem_blocks, mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!mem_size) {
		warn_printf ("db_pool_init: not enough memory available!\r\n");
		return (DB_ERR_NOMEM);
	}
	num_db_pools = npools;
	log_printf (DB_ID, 0, "db_pool_init: %lu bytes allocated for pools.\r\n",
							(unsigned long) mem_size);

	/* Setup data buffer pools. */
	for (i = 0, pdp = pools, mdp = &mem_blocks [MB_DATA_BUF_0];
	     i < npools;
	     i++, pdp++, mdp++) {
		VG_DEFINED (mdp, sizeof (MEM_DESC_ST));
		VG_DEFINED (mdp->md_addr, mdp->md_size);
		for (dbp = (DB *) mdp->md_addr;
		     dbp;
		     dbp = (DB *) MDS_NEXT (dbp)) {
			dbp->size = pdp->maxmsgdata;
			dbp->nrefs = 0;
			dbp->pool = i;
		}
		VG_NOACCESS (mdp->md_addr, mdp->md_size);
		VG_NOACCESS (mdp, sizeof (MEM_DESC_ST));
	}
#endif

	lock_init_nr (db_lock, "db");
	lock_init_nr (stats_lock, "stats");
	return (DB_OK);
}

/* db_pool_free -- Free the data buffer pools. */

void db_pool_free (void)
{
	lock_destroy (stats_lock);
	lock_destroy (db_lock);
	mds_free (mem_blocks, MB_END);
}

/* db_alloc_db -- Allocate a single 'best-fit' data buffer for the given #
		  of bytes.  If there is no single buffer available in the
		  pools, either because all pools of the size are empty and it
		  is allowed to allocate more from the heap or none of the
		  pools have the appropriate size and 'linear' is requested,
		  a buffer is allocated directly from the heap. */

static DB *db_alloc_db (size_t size, int linear)
{
	DB	*dbp;

#ifdef FORCE_MALLOC
	ARG_NOT_USED (linear)
	lock_take (db_lock);
#else
	MEM_DESC	pp, nepp = NULL, bfpp = NULL, xpp = NULL;
	unsigned	i, p, bfp = 0;

	VG_DEFINED (mem_blocks, sizeof (mem_blocks));
	lock_take (db_lock);
	for (i = 0, p = num_db_pools - 1, pp = &mem_blocks [p];
	     i < num_db_pools;
	     i++, p--, pp--) {
		if (pp->md_pool)
			nepp = pp;
		if (pp->md_esize - DB_HDRSIZE >= size) {
			if (pp->md_pool)
				xpp = pp;
			else {
				bfpp = pp;
				bfp = p;
			}
			break;
		}
	}
	if (!xpp) {
		if (!linear && nepp)
			xpp = nepp;
		else {
			if (bfpp && bfpp->md_xcount >= bfpp->md_xmax) {
				bfpp->md_nomem++;
				lock_release (db_lock);
				log_printf (DB_ID, 0, "db_alloc_db: data buffer pool empty!\r\n");
				return (NULL);
			}
#endif
			dbp = mm_fcts.alloc_ ((size + 3 + DB_HDRSIZE) & ~3);
			if (!dbp) {
				lock_release (db_lock);
				err_printf ("db_alloc_db: out of memory!\r\n");
				return (NULL);
			}
			if (size > 0xffff) {
				dbp->size = 0;
				lock_take (stats_lock);
				cur_lalloc_blocks++;
				if (cur_lalloc_blocks > max_lalloc_blocks)
					max_lalloc_blocks = cur_lalloc_blocks;
				num_lallocs++;
				lock_release (stats_lock);
			}
			else {
				dbp->size = size;
				lock_take (stats_lock);
				cur_balloc_blocks++;
				if (cur_balloc_blocks > max_balloc_blocks)
					max_balloc_blocks = cur_balloc_blocks;
				ADD_ULLONG (cur_balloc_size, size + DB_HDRSIZE);
				if (GT_ULLONG (cur_balloc_size, max_balloc_size))
					ASS_ULLONG (max_balloc_size, cur_balloc_size);
				num_ballocs++;
				lock_release (stats_lock);
			}
			dbp->nrefs = 0;

#ifdef FORCE_MALLOC
			lock_release (db_lock);
			dbp->pool = 0xff;
			return (dbp);
#else
			if (!bfpp)
				dbp->pool = 0xff;
			else {
				bfpp->md_xalloc++;
				bfpp->md_xcount++;
				if (bfpp->md_xcount > bfpp->md_mxcount)
					bfpp->md_mxcount = bfpp->md_xcount;
				dbp->pool = bfp;
			}
			lock_release (db_lock);
			TRC_DB (dbp, "db_alloc_db!", 1)
			VG_NOACCESS (mem_blocks, sizeof (mem_blocks));
			return (dbp);
		}
	}
	dbp = xpp->md_pool;
	VG_DEFINED (dbp, sizeof (void *));
	MDS_ALLOC (xpp, 0, dbp);
	lock_release (db_lock);
	TRC_DB (dbp, "db_alloc_db", 1)
	VG_POOL_ALLOC (xpp, dbp, xpp->md_esize);
	VG_DEFINED (dbp, offsetof (struct data_buffer_st, nrefs));
	dbp->nrefs = 0;
	VG_NOACCESS (mem_blocks, sizeof (mem_blocks));
#endif
	return (dbp);
}

/* db_alloc_rx -- Allocate a data buffer suitable for reception (i.e. large
		  enough). */

DB *db_alloc_rx (void)
{
	DB	*dbp;

	VG_DEFINED (mdata_pool, sizeof (MEM_DESC_ST));
	lock_take (db_lock);
#ifndef FORCE_MALLOC
	if ((dbp = mdata_pool->md_pool) != NULL) {
		VG_DEFINED (dbp, offsetof (struct data_buffer_st, nrefs));
		mdata_pool->md_pool = MDS_NEXT (dbp);
		mdata_pool->md_count--;
		if (mdata_pool->md_count < mdata_pool->md_mcount)
			mdata_pool->md_mcount = mdata_pool->md_count;

		VG_POOL_ALLOC (mdata_pool, dbp, mdata_pool->md_esize);
	}
	else if ((dbp = mdata_pool->md_xpool) != NULL) {
		VG_DEFINED (dbp, offsetof (struct data_buffer_st, nrefs));
		mdata_pool->md_xpool = MDS_NEXT (dbp);
		mdata_pool->md_gcount--;
	}
	else 
#endif
	     if (mdata_pool->md_xcount < mdata_pool->md_xmax) {
		mdata_pool->md_xalloc++;
		mdata_pool->md_xcount++;
		if (mdata_pool->md_xcount > mdata_pool->md_mxcount)
			mdata_pool->md_mxcount = mdata_pool->md_xcount;
		dbp = mm_fcts.alloc_ (mdata_pool->md_esize);
		if (!dbp) {
			lock_release (db_lock);
			return (NULL);
		}
		dbp->size = mdata_pool->md_esize - DB_HDRSIZE;
		dbp->pool = 0;
		lock_take (stats_lock);
		cur_balloc_blocks++;
		if (cur_balloc_blocks > max_balloc_blocks)
			max_balloc_blocks = cur_balloc_blocks;
		ADD_ULLONG (cur_balloc_size, mdata_pool->md_esize);
		if (GT_ULLONG (cur_balloc_size, max_balloc_size))
			ASS_ULLONG (max_balloc_size, cur_balloc_size);
		num_ballocs++;
		lock_release (stats_lock);
	}
	else {
		lock_release (db_lock);
		VG_NOACCESS (mdata_pool, sizeof (MEM_DESC_ST));
		return (NULL);
	}
	lock_release (db_lock);
#ifndef FORCE_MALLOC
	VG_UNDEFINED (dbp->data, mdata_pool->md_esize - DB_HDRSIZE);
#endif
	TRC_DB (dbp, "db_alloc_rx", 1)
	dbp->nrefs = 0;
	VG_NOACCESS (mdata_pool, sizeof (MEM_DESC_ST));
	return (dbp);
}

/* db_free_data -- Free a previously allocated data buffer chain. */

void db_free_data (DB *dp)
{
	DB		*next_dp;
	MEM_DESC	mp;

	TRC_DB (dp, "db_free_data!", 0);
	VG_DEFINED (mem_blocks, sizeof (mem_blocks));
	lock_take (db_lock);
	for (; dp; dp = next_dp) {
		next_dp = dp->next;
		rcl_access (dp);
		dp->nrefs--;
		rcl_done (dp);
		if (!dp->nrefs) {
			TRC_DB (dp, "db_free_data*", 0);
#ifdef CLEAR_ON_FREE
			memset (dp->data, 0xa5, dp->size);
#endif
			if (dp->pool == 0xff)
				goto free_block;

			else if (dp->pool < num_db_pools)
				mp = &mem_blocks [MB_DATA_BUF_0 + dp->pool];
			else {
				fatal_printf ("db_free_data: invalid data buffer!");
				return;
			}
			if ((uintptr_t) dp >= (uintptr_t) mp->md_addr &&
			    (uintptr_t) dp < (uintptr_t) mp->md_addr + mp->md_size) {
				MDS_FREE (mem_blocks, MB_DATA_BUF_0 + dp->pool, dp);
				VG_POOL_FREE (mp, dp);
			}
			else {
				mp->md_xcount--;

			    free_block:
				lock_take (stats_lock);
			    	if (!dp->size) {
					cur_lalloc_blocks--;
					num_lfrees++;
				}
				else {
					cur_balloc_blocks--;
					SUB_ULLONG (cur_balloc_size, dp->size + DB_HDRSIZE);
					num_bfrees++;
				}
				mm_fcts.free_ (dp);
				lock_release (stats_lock);
			}
		}
		else
			break;
	}
	lock_release (db_lock);
	VG_NOACCESS (mem_blocks, sizeof (mem_blocks));
}

/* db_alloc_data -- Allocate a data buffer chain to store the given data size.
		    We attempt to allocate buffers from the buffer pools if
		    possible.  However, if the internal pools are low on memory
		    and it is allowed, we allocate directly from the heap.
		    If the 'linear' argument is given, the allocator is forced
		    to return a single buffer that is large enough to contain
		    all data.  Otherwise, a chain of smaller buffers might be
		    returned. */

DB *db_alloc_data (size_t size, int linear)
{
	DB	*dp = NULL, *prev_dp = NULL, *ndp;
#ifdef CHECK_ALLOC
	size_t	tsize = size;
#endif
	for (;;) {
		ndp = db_alloc_db (size, linear);
		if (!ndp) {
			if (prev_dp) {
				prev_dp->next = NULL;
				db_free_data (dp);
			}
			return (NULL);
		}
		/*VG_DEFINED (ndp, DB_HDRSIZE);*/
		ndp->nrefs = 1;
		if (!prev_dp)
			dp = ndp;
		else
			prev_dp->next = ndp;
		if (!ndp->size || size <= ndp->size)
			break;

		prev_dp = ndp;
		size -= ndp->size;
	}
	ndp->next = NULL;
#ifdef CHECK_ALLOC
	for (ndp = dp, size = 0; ndp; ndp = ndp->next)
		size += ndp->size;
	if (size < tsize)
		fatal_printf ("db_alloc_data: returned buffer list is too small to contain data!");
#endif
	return (dp);
}

/* db_put_data -- Copy a linear buffer {sp,length} to an allocated buffer chain
		  at the given destination offset. */

void db_put_data (DB *dbp, size_t ofs, const void *sp, size_t length)
{
	size_t	n;

	while (length) {
		if (!dbp->size)
			n = length;
		else {
			n = dbp->size - ofs;
			if (n > length)
				n = length;
		}
		memcpy (dbp->data + ofs, sp, n);
		ofs = 0;
		length -= n;
		if (length) {
			sp = (unsigned char *) sp + n;
			dbp = dbp->next;
		}
	}
}

/* db_get_data -- Copy data from a buffer chain {dbp,data,ofs,length} to a 
                  linear data buffer {dp,length}. */

void db_get_data (void       *dp,
		  const DB   *dbp,
		  const void *data,
		  size_t     ofs,
		  size_t     length)
{
	unsigned	n, size;

	while (length) {
		size = dbp->size;
		if (!size)
			size = length;
		if (!data)
			data = dbp->data;
		else if (dbp->size)
			size -= (unsigned char *) data - dbp->data;

		if (ofs) {
			if (size > ofs) {
				size -= ofs;
				data = (char *) data + ofs;
				ofs = 0;
			}
			else {
				ofs -= size;
				dbp = dbp->next;
				data = dbp->data;
				continue;
			}
		}
		n = (length > size) ? size : length;
		memcpy (dp, data, n);
		length -= n;
		if (length) {
			dp = (unsigned char *) dp + n;
			dbp = dbp->next;
			if (!dbp) {
				warn_printf ("db_get_data: end of list reached (needed %lu extra bytes)!",
							(unsigned long) length);
				break;
			}
			data = NULL;
		}
	}
}

/* dbw_inc -- Increment the walk pointer n bytes. */

const unsigned char *dbw_inc (DBW *p, size_t n)
{
	while (p->left < n) {
		n -= p->left;
		p->dbp = p->dbp->next;
		if (!p->dbp)
			return (NULL);

		p->data = p->dbp->data;
		p->left = p->dbp->size;
	}
	if (n) {
		p->data += n;
		p->left -= n;
	}
	return (p->data);
}

#ifdef DDS_DEBUG

/* db_pool_dump -- Display pool data and statistics. */

void db_pool_dump (size_t sizes [])
{
	print_pool_table (mem_blocks, (unsigned) MB_END, sizes);
}

void db_xpool_stats (void)
{
	lock_take (stats_lock);
	dbg_printf ("Dynamic pool block stats: <=64K - max/used/msize/size: ");
	dbg_printf ("%u/%u/", max_balloc_blocks, cur_balloc_blocks);
	DBG_PRINT_ULLONG (cur_balloc_size);
	dbg_printf ("/");
	DBG_PRINT_ULLONG (max_balloc_size);
	dbg_printf ("\r\n                           >64K - max/used           : ");
	dbg_printf ("%u/%u\r\n", cur_lalloc_blocks, max_lalloc_blocks);
	lock_release (stats_lock);
}

#endif

