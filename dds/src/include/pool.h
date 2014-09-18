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

/* pool.h -- Exports various memory allocation services, such as:

		- Basic memory allocation macros.
		- Support for unaligned memory accesses (CPU-independent).
		- Pool services for user-defined memory lists.
		- Memory descriptor sets.
*/

#ifndef __pool_h_
#define	__pool_h_

#include <string.h>
#include "thread.h"
#include "sys.h"

#define	POOL_OK		0
#define	POOL_ERR_NOMEM	1
#define	POOL_ERR_NFOUND	2

#ifndef STMT_BEG
#define STMT_BEG	do {
#define	STMT_END	} while(0)
#endif

void pool_init_stats (void);
void pool_final_stats (void);

typedef struct pool_alloc_fcts {
	void *(*alloc_) (size_t size);
	void *(*realloc_) (void *ptr, size_t size);
	void (*free_) (void *ptr);
} PoolAllocFcts;

extern PoolAllocFcts mm_fcts;


void pool_pre_init (void);

/* Preinitialize pools. */

void pool_post_final (void);

/* Finalize pools. */


/* 1. Standard C-like memory access macros: 
   ---------------------------------------- */

#ifdef STD_ALLOC

#define	xmalloc	 mm_fcts.alloc_
#define	xrealloc mm_fcts.realloc_
#define	xfree	 mm_fcts.free_

#else /* !STD_ALLOC */

void *xmalloc (size_t size);

/* Try to allocate a memory block of the requested size.  If successful, a
   pointer to the block is returned.  If not enough memory available, the
   function returns NULL.
   Note: this function is similar to the standard malloc() library function,
         but it does some extra bookkeeping for debugging purposes. */

void *xrealloc (void *ptr, size_t size);

/* Change the size of the memory block pointed to by ptr to the given size.
   If successful, a pointer to the new block is returned with the old content
   copied to it.  If not enough memory is available, NULL is returned.
   This function is safe in the sense that if a realloc fails, the old data, as
   pointed to by ptr is still valid.
   Note:  this function is similar to the standard realloc() library function,
          but it does some extra bookkeeping for debugging purposes. */

void xfree (void *ptr);

/* Free the memory block pointed to by ptr.
   Note:  this function is similar to the standard free() library function,
         but it does some extra bookkeeping for debugging purposes. */

void pool_get_malloc_count (unsigned *blocks, size_t *bytes);

/* Get the current xmalloc statistics for the # of blocks/bytes allocated. */

#endif /* !STD_ALLOC */



/* 2. Various macros for unaligned memory accesses: 
   ------------------------------------------------ */

#if defined (_MCC960) || defined(_CCC960) || defined (i386)
#define	FLEX_ALIGN
#endif

#ifdef FLEX_ALIGN
#define shortcpy(dst,src)	*((short *)(dst)) = *((short *)(src))
#define longcpy(dst,src)	*((long *)(dst)) = *((long *)(src))
#define paracpy(dst,src)	STMT_BEG \
				 *((long *)(dst)) = *((long *)(src)); \
				 *((long *)((dst)+4)) = *((long *)((src)+4)); \
				 *((long *)((dst)+8)) = *((long *)((src)+8)); \
				 *((long *)((dst)+12)) = *((long *)((src)+12)); STMT_END
#define maccpy(dm,sm)		STMT_BEG \
				 *((long *)(dm)) = *((long *)(sm)); \
				 *(((short *)(dm))+2) = *(((short *)(sm))+2); STMT_END
#define maccmp(m1,m2)		(*((long *)(((char *)(m1))+2)) == *((long *)(((char *)(m2))+2)) && \
				 *((short *)(m1)) == *((short *)(m2)))
#else
#define shortcpy(dst,src)	STMT_BEG \
				 *((char *)(dst))=*((char *)(src));\
				 *((char *)(dst)+1)=*((char *)(src)+1); STMT_END
#define longcpy(dst,src)	STMT_BEG { unsigned _i; char *_sp, *_dp; \
				 for (_sp=(char*)(src),_dp=(char*)(dst),_i=0;_i<4;_i++) \
				  *_dp++=*_sp++; } STMT_END
#define paracpy(dst,src)	memcpy((char *)(dst),(char *)(src),16)
#define maccmp(m1,m2)		((((unsigned)(m1)&1)!= 0||((unsigned)(m2)&1)!=0)? \
				 !memcmp (m1, m2, 6) : \
				 (*(((short *)(m1))+2) == *(((short *)(m2))+2) && \
				 *(((short *)(m1))+1) == *(((short *)(m2))+1) && \
				 *((short *)(m1)) == *((short *)(m2))))
#define maccpy(dm,sm)		STMT_BEG \
				 if(((unsigned)(dm)&1)!= 0||((unsigned)(sm)&1)!=0) \
				  { unsigned _i; char *_sp, *_dp; \
				    for (_sp=(char*)(sm),_dp=(char*)(dm),_i=0;_i<6;_i++) \
				     *_dp++=*_sp++; \
				  } else { \
				    *((short *)(dm)) = *((short *)(sm)); \
				    *(((short *)(dm))+1) = *(((short *)(sm))+1); \
				    *(((short *)(dm))+2) = *(((short *)(sm))+2); \
				  } STMT_END
#endif


/* 3. Pool services:
   ----------------- */

typedef void *Pool_t;

int pool_init (unsigned max_pools,	/* Max. number of pools. */
	       size_t   gbl_length,	/* Length of global pool. */
	       size_t   gbl_bsize);	/* Block size of global pool. */

/* Initialize the pool handler.  A single global pool is created that can be
   referenced with a pool id of NULL.  Other pools can be created by the user. */

int pool_create (const char *name,	/* Pool name. */
		 size_t     length,	/* Number of blocks in pool. */
		 size_t     bsize,	/* Block size. */
		 Pool_t     *poolid);	/* Pool id. */

/* Create a new pool with the specified name and the given number of blocks. */

unsigned pool_bsize (Pool_t pool_id);

/* Return the block size of a pool element. */

int pool_ident (const char *name, Pool_t *poolid);

/* Find the pool id of the given pool name. */

#define pool_gbl_alloc()	pool_alloc(NULL)

/* Allocate a block from the global pool. */

#define pool_gbl_free(p)	pool_free(p)

/* Free a block back to the global pool. */

void *pool_alloc (Pool_t pool_id);

/* Allocate a block from the specified pool. */

void pool_free (void *ptr);

/* Free a block to its pool. */

void pool_check (Pool_t pool_id);

/* Do a sanity check on the given pool. */

unsigned pool_current_block_count (Pool_t pool_id);

/* Returns the number of free blocks in the pool. */

unsigned pool_initial_block_count (Pool_t pool_id);

/* Returns the initial number of free blocks in the pool (at creation). */

void pool_walk (Pool_t pool_id,
		void (*bfct) (void *ptr, size_t size));

/* Walk over every as yet unallocated pool block of a pool and call the block
   function with a pointer to the pool block and the block size.
   This function can be used for once-only pool block initializations after a
   pool has been created. */


/* 4. Memory descriptor set services:
   ---------------------------------- */

/* A memory descriptor set is a 2-dimensional collection of memory blocks that
   is allocated completely in a single operation.  Individual elements of the
   set can either be single memory chunks or simple pools of fixed size blocks.
   The set is specified as an array of Memory Descriptor structures.
   Once the set is allocated, blocks can be taken from it or added back to it
   on request of the user.
   The full set can be released to free memory in a single operation.
   Pools based on the memory descriptor can be either fixed size, or may grow
   in size up to a maximum value. For the former, the MDS_ENTRY/ALLOC/CALLOC/
   FREEand MDS_NOMEM macros (see further) can be used.  To allow dynamic sizing,
   the mds_pool_alloc/free functions must be used. */

typedef struct mem_desc_st {
	const char	*md_name;	/* Pool name. */
	void		*md_addr;	/* Pointer to memory block. */
	size_t		md_size;	/* Total memory block size. */
	size_t		md_esize;	/* Element size. */
	void		*md_pool;	/* Reserved elements pool. */
	unsigned	md_count;	/* # of elements currently in pool. */
	unsigned	md_mcount;	/* Min. # of elements ever in pool. */
	void		*md_xpool;	/* Extra allocated elements pool. */
	unsigned	md_xmax;	/* Max. # of extra allocatable bufs. */
	unsigned	md_xgrow;	/* Max. # of stored extra bufs. */
	unsigned	md_mxcount;	/* Maximum # of extra allocated bufs. */
	unsigned	md_xcount;	/* Current # of extra allocated bufs. */
	unsigned	md_gcount;	/* Current # of buffers in grow area. */
	unsigned	md_xalloc;	/* # of extra allocations. */
	unsigned	md_nomem;	/* # of out-of-memory conditions. */
	lock_t		md_lock;	/* Concurrency lock. */
#ifdef CTRACE_USED
	unsigned	md_trace_id;	/* Trace identifier. */
#endif
} MEM_DESC_ST, *MEM_DESC;

size_t mds_alloc (MEM_DESC mds, const char **names, size_t length);

/* Allocate the full memory descriptor set in a single operation.
   The mds parameter points to an array of MEM_DESC_ST structs, length specifies
   the number of descriptor structures.
   The individual memory descriptors must be filled in correctly as follows:

	md_size:	Total size of a full pool or single memory block.
	md_esize:	Element size: 0 if a single memory block, else size of
			pool element.  The number of pool elements in a pool =
			md_size/md_esize.

   When the function returns successfully (returns total size), the other fields
   will be successfully filled as follows:

	md_addr:	Points to full memory block (= first pool element).
	md_pool:	Pools only: initially set to md_addr but changes once
			blocks are allocated/released.
	md_count:	Pools only: initially keeps the number of pool elements,
			but changes once blocks are allocated/released, keeping
			count of the number of available elements. */

void mds_reset (MEM_DESC mds, size_t length);

/* Reset an active memory descriptor set so that the set is equal to a newly
   created descriptor set. */

void mds_free (MEM_DESC mds, size_t length);

/* Free a memory descriptor set.
   Note: the arguments must be identical to those used in the mds_alloc()
         function. */


/* The macros MDS_BLOCK_TYPE() and MDS_POOL_TYPE() can be used to specify either
   a single block at the specified index or a pool of elements.  These macros
   should be used before the call to mds_alloc() ! */

#define MDS_BLOCK_TYPE(mds,idx,s)	STMT_BEG	\
	mds [idx].md_size = (s);			\
	mds [idx].md_esize = 0;				\
STMT_END

/* Set an MDS entry (mds [idx]) to a single block of the specified size (s). */

#define MDS_POOL_TYPE(mds,idx,l,s)	STMT_BEG	\
	mds [idx].md_size = (l).reserved * (s);		\
	mds [idx].md_esize = (s);			\
	mds [idx].md_xmax = (l).extra;			\
	mds [idx].md_xgrow = (l).grow;			\
STMT_END

/* Set an MDS entry (mds [idx]) to a pool of (n) elements of the specified size
   (s). */


/* Once mds_alloc() was successful, the following macros can be used to access
   individual elements from the descriptor set:

   To get a single block, use the MDS_BLOCK() macro.
   To allocate pool blocks, use the following algorithm:

		if ((p = MDS_ENTRY (mds,idx)) != NULL) {
			MDS_ALLOC (mds, idx, p);
			...
		}

   To release pool blocks, use the MDS_FREE() macro.

   Note: These macros should only be used in single-threaded environments and do
         *not* support the dynamic pool extension mechanisms.
	 If concurrent access and pool extension features are required, the
	 mds_pool_alloc/free() functions should be used instead! */

#define MDS_BLOCK(mds,idx)	mds [idx].md_addr

/* Get a pointer to an MDS block. */

#define MDS_ENTRY(mds,idx)	mds [idx].md_pool

/* Get a pointer to the first element of an MDS pool (or NULL if empty). */

#define MDS_NEXT(px)		*((void **)px)

/* Get a pointer to the next pool block. */

#define MDS_ALLOC(mds,idx,px)	STMT_BEG		\
	mds [idx].md_pool = MDS_NEXT (px);		\
	MDS_NEXT (px) = NULL;				\
	mds [idx].md_count--;				\
	if (mds [idx].md_count < mds [idx].md_mcount)	\
		mds [idx].md_mcount--;			\
STMT_END

/* Allocate an MDS pool element. */

#define MDS_FREE(mds,idx,px)	STMT_BEG		\
	MDS_NEXT (px) = mds [idx].md_pool;		\
	mds [idx].md_pool = px;				\
	mds [idx].md_count++;				\
STMT_END

/* Release an MDS pool element. */

#define MDS_NOMEM(mds,idx)	mds [idx].md_nomem++

/* Increment the out-of-memory counter of an MDS pool. */

void *mds_block (MEM_DESC mp);

/* Get the monolithic block that was allocated in mds_alloc(). */

void *mds_pool_alloc (MEM_DESC mp);

/* Allocate an MDS pool element, either from the pre-created pool, or by extra
   allocations when the pool is depleted, up to the maximum allowed.
   There is no need to use the MDS_ENTRY or MDS_NOMEM() macros, since this is
   done automatically. */

void mds_pool_free (MEM_DESC mp, void *ptr);

/* Free an MDS pool element that was formerly allocated via mds_pool_alloc(). */

int mds_pool_contains (MEM_DESC mp, void *ptr);

/* Verify whether an MDS pool contains the given block. */

typedef struct pool_limits_st {
	unsigned	reserved;	/* Reserved. # of items. */
	unsigned	extra;		/* Extra allocatable # of items. */
	unsigned	grow;		/* Extra pool size. */
} POOL_LIMITS;

#define	pool_grow_amount(res,extra,gp)	(gp>=100 || (extra == ~0U && gp)) ? \
						extra : (extra * gp) / 100

/* Calculate the number of extra allocated buffers to keep as a percentage of
   the that number specified with gp. */

#define	pool_limits_set(l,min,max,gp) (l).reserved=(min);			\
	(l).extra=((max)==~0U) ? ~0U : ((max)<(min)) ? ~0U : (max)-(min);	\
	(l).grow=pool_grow_amount ((l).reserved,(l).extra,gp)

/* Set pool constraints as min: (reserved # of elements), max (maximum # of
   pool elements) and grow (% of elements to keep after dynamic allocations). */

typedef enum {
	PDT_NORMAL,	/* Fits in 80-chars, 1 line per pool. */
	PDT_LONG,	/* Displays extended pool info, 1 line per pool. */
	PDT_SUMMARY	/* Display summary only. */
} POOL_DISPLAY;

void print_pool_format (POOL_DISPLAY type);

/* Specify the pool display format. */

void print_pool_hdr (int log);

/* Start display of a memory pool, either as logged info (log != 0) or as a
   response to a pool display command (log == 0). */

/* Indices in sizes array for memory pool results: */
#define	PPT_TOTAL	0	/* Total available. */
#define	PPT_MUSE	1	/* Maximum used. */
#define	PPT_CUSE	2	/* Currently used. */
#define	PPT_MXUSE	3	/* Maximum extra allocated. */
#define	PPT_XUSE	4	/* Extra allocated. */
#define	PPT_XCNT	5	/* # of allocations. */

#define PPT_SIZES	6	/* # of counters. */
 
void print_pool_table (const MEM_DESC_ST *pdp,	/* Pool descriptors. */
		       unsigned          n,	/* Number of elements in pd. */
		       size_t            sizes [PPT_SIZES]);/* Sizes. */

/* Display a pool table.  Set *tsize = *msize = *usize = 0 for the first pool
   display. */

void print_pool_end (size_t sizes [PPT_SIZES]);

/* End pool display, printing totals. */

void print_alloc_stats (void);

/* Display allocation statistics. */

#endif /* !_pool_h_ */

