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

/* cache.c -- Implements the history cache of DDS. */

#include <stdio.h>
#include "log.h"
#include "prof.h"
#include "error.h"
#include "pool.h"
#include "list.h"
#include "skiplist.h"
#include "md5.h"
#include "typecode.h"
#include "guard.h"
#include "debug.h"
#include "dds.h"
#include "dcps.h"
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
#include "sec_access.h"
#endif
#include "cache.h"

#ifndef MIN_XQOS_DELAY
#define	MIN_XQOS_DELAY	40	/* Min. # of ticks before recheck QoS. */
#endif
#define	MAX_CHANGE_REFS	0x7ffU	/* Max. # of references to a change. */

#ifndef MAX_INST_HANDLE
#if defined (BIGDATA) || (WORDSIZE == 64)
#define	MAX_INST_HANDLE 0xffffffffU
#else
#define	MAX_INST_HANDLE	0xffffU	/* Max. instance handle. */
#endif
#endif
#define RX_INST_CLEANUP		/* Cleanup instances when taken. */

#ifdef _WIN32
#define	INLINE
#else
#define	INLINE	inline
#endif

#ifdef XTYPES_USED
#define	PARSE_KEY	/* Display key as formatted data. */
#endif

#ifdef PROFILE
PROF_PID (cache_get)
PROF_PID (cache_get_data)
PROF_PID (cache_get_key)
PROF_PID (cache_done)
PROF_PID (cache_avail)
PROF_PID (cache_register)
PROF_PID (cache_lookup_key)
PROF_PID (cache_lookup_hash)
PROF_PID (cache_release)
PROF_PID (cache_add_inst)
PROF_PID (cache_inst_free)
#endif

#ifndef DDS_HIST_PURGE_NA
#define DDS_HIST_PURGE_NA 0
#endif

/* History cache.
   --------------
   This implementation of the history cache tries to keep the cache footprint
   as small as possible using a number of design principles.

   Data is stored in the cache by means of a Cache Change descriptor.
   This descriptor describes all attributes of a cache change and can be used
   not only for referencing real data, but also to handle instance state
   changes.

   Cache Change data can be referenced in a number of places, such as:
   
   	- A complete linear list of all cache changes in 'presentation' order
	  (sequence number/reception timestamp/source timestamp) in each cache.
        - Optionally, when there are multiple instances required (WITH_KEY),
	  a list of cache changes per instance in 'presentation' order per
	  cache.
        - A list of cache changes that are being processed on upper (DCPS) or
	  lower (RTPS) level for each individual reader and/or writer proxy.

   It is important to realize that we strive for the cache data to be present
   only once in the system, irrespective on the number of references to it, even
   when the data is referenced in multiple caches.  This can happen, for example
   when there is a writer as well as a reader for the same cache data.  In that
   case, two caches will be present, but the cache data itself will be there
   only once.

   To allow all this in an easy manner and in order not to duplicate the
   descriptor itself, all linkages are done using Cache Change Reference
   structures (CCREF) that can be chained together in a dual-linked list
   (CCLIST). A Cache Change Reference contains a pointer to a Cache Change as
   well as some layer-specific extra information.
   For the history cache, an extra pointer is present to link full cache list
   entries and instance entries.
   For RTPS, extra state information is added.

	   Advantages: - Easy to enqueue the complete cache contents to
	                 a new reader.
		       - Small cost: (3 or 4 pointers/cache change).
		       - Cache changes can easily be removed.

   For proper management of usage of cache data, each Cache Change has a
   reference counter that counts the number of active pointers to it.  Only when
   the last user of the data has relinquished access to it is the Cache Change
   and the Change data released.

   As described above, when multiple instances are required, the data that is
   associated with an instance is also queued in proper order on instance level.

   The various instances each have an instance descriptor that holds all the
   instance attributes such as the Hashkey and the concatenated Key fields.
   Instance descriptors are linked together, either in a simple linked list when
   there are only a few entries or by skiplists when many instances are used.
   In the latter case, one skiplist is used for for searching on instance handle
   and the other is used for searching on hash key (16-byte).
   When long keys (>16 bytes) are used, collision entries are chained in the
   Hashkey node.

*/

/* Change list node: */
typedef struct ccref_st CCREF;
struct ccref_st {
	CCREF		*next;		/* Next in list of changes. */
	CCREF		*prev;		/* Previous in list of changes. */
	CCREF		*mirror;	/* Mirror link for multi-instances. */
	Change_t	*change;	/* Cache change data. */
};

#if defined (BIGDATA) || (WORDSIZE == 64)
#define MAX_LIST_CHANGES	0xffffffffU
typedef unsigned CacheHandle_t;
#else
#define MAX_LIST_CHANGES 	0xffffU
typedef unsigned short CacheHandle_t;
#endif

/* List of cache changes: */
typedef struct cclist_st {
	union {
	  struct {
	    CCREF	*head;		/* First cache change. */
	    CCREF	*tail;		/* Last cache change. */
	  }		list;		/* Non-empty list: list pointers. */
	  FTime_t	time;		/* Empty list: timestamp. */
	}		u;
#define l_head		u.list.head
#define l_tail		u.list.tail
#define l_time		u.time
#define l_list		u.list
	CacheHandle_t	l_nchanges;	/* # of changes. */
	CacheHandle_t	l_handle;	/* Handle of instance. */
} CCLIST;

typedef struct instance_st INSTANCE;

#define DNWRITERS	(sizeof (uintptr_t) / sizeof (handle_t))
#define NWRITERS_INC	DNWRITERS * 4

/* To do proper accounting on who is writing an the instance, a list of writers
   is kept up to date on each cache add.  This list is either embedded, i.e.
   i_writers.w *is* the list, or the list is a separately allocated block,
   pointed to by i_writers.p. In either case, the list is can be seen as an
   array of handles that are always kept sorted for efficient lookups using a
   binary search algorithm. */

/* Extra allocated block containing a list of writers.  This is used when the
   actual # of writers is larger than DEF_NWRITERS. */
typedef struct inst_writers_st {
	unsigned short	max_writers;
	handle_t	w [NWRITERS_INC];
} WLBLOCK;

/* List of sample writers. */
typedef union {
	handle_t	w [DNWRITERS];	/* Writers list (embedded). */
	WLBLOCK		*ptr;		/* Writers list (allocated). */
} WLIST;

#define	WLIST_HANDLES(ip) ((ip)->i_nwriters <= DNWRITERS) ? \
				(ip)->i_writers.w : (ip)->i_writers.ptr->w

typedef struct history_cache_st HistoryCache_t;
typedef struct tfilter_st TFilter_t;
typedef struct tf_node_st TFNode_t;

struct tf_node_st {
	TFNode_t	*next;		/* Time-sorted dual-linked list node. */
	TFNode_t	*prev;
	TFNode_t	*i_next;	/* Next node in per-instance list. */
	TFilter_t	*filter;	/* Filter to which node belongs. */
	INSTANCE	*instance;	/* Instance reference. */
	FTime_t		tx_time;	/* Next transmit/flush time. */
	void		*sample;	/* Enqueued sample. */
	int		rel;		/* Reliable. */
};

struct tfilter_st {
	TFNode_t	*head;		/* Time-sorted dual-linked list. */
	TFNode_t	*tail;
	TFilter_t	*next;		/* Next filter context. */
	HistoryCache_t	*cache;		/* Cache to which filter belongs. */
	unsigned	nusers;		/* # of users. */
	void		*proxy;		/* Proxy context. */
	FTime_t		delay;		/* Minimum send delay per instance. */
	TBFSENDFCT	send_fct;	/* Send sample function. */
	TBFDONEFCT	done_fct;	/* Free sample function. */
	Timer_t		timer;		/* Timer to manage node time-outs. */
};

#ifdef EXTRA_STATS

#define	STATS_INC(x)	x++

typedef struct inst_stats_st {
	uint64_t	i_octets;	/* Cache: add bytes. */
	unsigned	i_add;		/* Cache: add data. */
	unsigned	i_get;		/* Cache: sample read. */
	unsigned	c_transfer;	/* Cache: transfer. */
	unsigned	m_new;		/* RTPS: new sample. */
	unsigned	m_remove;	/* RTPS: removed sample. */
	unsigned	m_urgent;	/* RTPS: urgent ack. */
	unsigned	l_notify;	/* DCPS: notify data. */
} INST_STATS;

#else
#define	STATS_INC(x)
#endif

/* Instance changes: */
struct instance_st {
	INSTANCE	*i_next;	/* Next instance. */
	CCLIST		i_list;		/* Cache change list. */
#define i_head		i_list.l_head	/* First element. */
#define i_tail		i_list.l_tail	/* Last element. */
#define i_time		i_list.l_time	/* List timestamp. */
#define i_nchanges	i_list.l_nchanges /* # of changes. */
#define i_handle	i_list.l_handle	/* Instance handle. */
	KeyHash_t	i_hash;		/* Hash of key fields. */
	String_t	*i_key;		/* Key fields. */
	handle_t	i_owner;	/* Current owner. */
#ifdef HBITMAP_USED
	unsigned	i_nwriters;	/* Current # of writers. */
#else
	unsigned short	i_nwriters;	/* Current # of writers. */
#endif
	unsigned	i_ndata;	/* # of alive samples. */
	unsigned	i_kind:2;	/* Instance state. */
	unsigned	i_view:1;	/* New instance? */
	unsigned	i_wait:1;	/* Somebody waiting on instance? */
	unsigned	i_deadlined:1;	/* Deadline exceeded. */
	unsigned	i_inform:1;	/* Inform monitor of existence. */
	unsigned	i_recover:1;	/* Instance can be recovered. */
	unsigned	i_registered:1;	/* Explicitly registered instance. */
	unsigned	i_disp_cnt;	/* Disposed generation count. */
	unsigned	i_no_w_cnt;	/* No Writers generation count. */
	TFNode_t	*i_tbf;		/* Time-based filter nodes. */
	WLIST		i_writers;	/* Writers list. */
#ifdef EXTRA_STATS
	INST_STATS	i_stats;	/* Extra statistics. */
	unsigned	i_register;	/* Cache registers. */
	unsigned	i_dispose;	/* Cache disposes. */
	unsigned	i_unregister;	/* Cache unregisters. */
#endif
};

/* While few instances are used (n < MAX_INST_LLIST), instances are chained
   directly together, and searching is simply by walking over the list, whether
   when searching a key, or when searching an instance handle.
   When the number of instances becomes larger, the cache is converted to a
   dual skiplist for efficient searching:

  	- The first skiplist is used to search instances based on the unique
	  instance handle.  Since it is impossible to have duplicate handles,
	  matching entries is very simple, i.e. no subchaining required.
	- The second skiplist is used to implement a hash table, where the
	  hash of a key (key itself if <= 16, or MD5(key)) is used to lookup
	  an entry and with hash collision resolution by comparing keys in a
	  direct instance chain (as before). */

#define	MAX_INST_LLIST	12	/* Max. length of a simple instance list. */
#define	MIN_INST_LLIST	8	/* Min. length of a set of skiplists. */

typedef struct inst_llist_st {
	INSTANCE	*head;		/* First cache change. */
	INSTANCE	*tail;		/* Last cache change. */
	unsigned	ninstances;	/* # of changes. */
} INST_LLIST;

typedef struct inst_slists_st {
	Skiplist_t	*hashes;	/* Indexed by instance hashkey fields.*/
	Skiplist_t	*handles;	/* Indexed by instance handle. */
	unsigned	ninstances;	/* # of instances. */
} INST_SLISTS;

typedef struct cache_ref_st CacheRef_t;

struct cache_ref_st {
	CacheRef_t	*next;		/* Next reader cache reference. */
	HistoryCache_t	*cache;		/* Reader cache. */
};

/* History cache: */
struct history_cache_st {
	LocalEndpoint_t	*hc_endpoint;	/* Endpoint link. */
	CacheRef_t	*hc_readers;	/* Matching local Reader caches. */
	unsigned	hc_key_size;	/* Fixed key size if != 0. */
	unsigned	hc_multi_inst:1;/* Multi-instance cache. */
	unsigned	hc_writer:1;	/* Writer cache. */
	unsigned	hc_monitor:1;	/* Active monitor. */
	unsigned	hc_inform:1;	/* Active instance notifications. */
	unsigned	hc_skiplists:1;	/* Multi-instance uses skiplists. */
	unsigned	hc_durability:1;/* Durability mode. */
	unsigned	hc_kind:2;	/* No instances: instance state. */
	unsigned	hc_view:1;	/* No instances: view state. */
	unsigned	hc_src_time:1;	/* Use source timestamps. */
	unsigned	hc_inst_order:1;/* Order on instance for access. */
	unsigned	hc_auto_disp:1;	/* If auto-dispose. */
	unsigned	hc_prefix:1;	/* Prepend type prefix. */
	unsigned	hc_must_ack:1;	/* Block on unacked/unread full. */
	unsigned	hc_blocked:1;	/* Reader/writer blocked state. */
	unsigned	hc_recycle:1;	/* Recycled handles - needs checks. */
	unsigned	hc_exclusive:1;	/* Exclusive ownership mode? */
	unsigned	hc_liveliness:1;/* Liveliness enabled. */
	unsigned	hc_deadline:1;	/* Deadline checks enabled. */
	unsigned	hc_deadlined:1;	/* Deadline exceeded. */
	unsigned	hc_lifespan:1;	/* Lifespan checks enabled. */
	unsigned	hc_tfilter:1;	/* Time-based filter enabled. */
	unsigned	hc_purge_nw:1;	/* Autopurge no-writers checks. */
	unsigned	hc_purge_dis:1;	/* Autopurge disposed checks. */
	unsigned	hc_lsc_idle:1;	/* Lifespan checks are inactive. */
	unsigned	hc_dlc_idle:1;	/* Deadline checks are inactive. */
	unsigned	hc_apw_idle:1;	/* Autopurge no-writer checks idle. */
	unsigned	hc_apd_idle:1;	/* Autopurge disposed checks idle. */
	unsigned	hc_sl_walk:2;	/* Currently walking over instances. */
	unsigned	hc_secure_h:1;	/* Use secure hashes. */
	unsigned	hc_ref_type:1;	/* Purge data on dispose/unregister. */
	unsigned	hc_max_depth;	/* Max. # of samples/instance. */
	unsigned	hc_max_inst;	/* Max. # of instances. */
	unsigned	hc_max_samples;	/* Max. # of samples overall. */
	CCLIST		hc_changes;	/* List of all cache changes. */
#define hc_head		hc_changes.l_head
#define hc_tail		hc_changes.l_tail
#define hc_time		hc_changes.l_time
#define hc_list		hc_changes.l_list
#define	hc_nchanges	hc_changes.l_nchanges /* Total # of changes. */
#define hc_last_handle	hc_changes.l_handle /* Last assigned instance handle. */
	unsigned	hc_ndata;	/* Total # of data samples. */
	SequenceNumber_t hc_last_seqnr;	/* Last assigned sequence number. */
	union {
	  union {
	    INST_LLIST	instances_ll;	/* Linked-list of instances. */
	    INST_SLISTS	instances_sl;	/* Skiplists of instances. */
	  }		mi;
#define hc_inst_ll	u.mi.instances_ll
#define	hc_inst_head	u.mi.instances_ll.head
#define	hc_inst_tail	u.mi.instances_ll.tail
#define	hc_ninstances	u.mi.instances_ll.ninstances
#define hc_inst_sl	u.mi.instances_sl
#define hc_hashes	u.mi.instances_sl.hashes
#define	hc_handles	u.mi.instances_sl.handles
	  struct {
	    TFNode_t	*tbf;		/* Time-based filter context. */
	    GenCounters_t ctr;		/* Generation counts. */
	  }		si;
#define	hc_tbf		u.si.tbf		/* Time-based filter node. */
#define	hc_disp_cnt	u.si.ctr.disp_cnt	/* Disposed count. */
#define hc_no_w_cnt	u.si.ctr.no_w_cnt	/* No Writers count. */
	}		u;
	HCRDNOTFCT	hc_notify_fct;	/* Notification callback function. */
	uintptr_t	hc_notify_user;	/* User parameter for notifications. */
	uintptr_t	hc_mon_user;	/* User parameter for monitors. */
	TFilter_t	*hc_filters;	/* Time-based filter contexts. */
	unsigned	hc_unacked;	/* # of unacked changes. */
#ifdef EXTRA_STATS
	INST_STATS	hc_stats;	/* Instance statistics. */
	unsigned	hc_mon_start;	/* RTPS: connected. */
	unsigned	hc_mon_stop;	/* RTPS: disconnected. */
	unsigned	hc_mon_replay;	/* RTPS: replay samples. */
	unsigned	hc_unblock;	/* RTPS: unblocked. */
#endif
};

typedef struct cache_wait_st CacheWait_t;
struct cache_wait_st {
	CacheWait_t	*next;
	CacheWait_t	*prev;
	HistoryCache_t	*cache;
	INSTANCE	*instance;
	Change_t	*change;
	int		wchange;
	cond_t		wcond;
	unsigned	nwaiting;
};

typedef struct cache_wait_list_st CacheWaitList_t;
struct cache_wait_list_st {
	CacheWait_t	*head;
	CacheWait_t	*tail;
};

typedef struct cache_xfer_st CXFER;
struct cache_xfer_st {
	CXFER		 *next;
	CXFER		 *prev;
	Cache_t		 scache;
	HCI		 shci;
	SequenceNumber_t seqnr;
	Change_t	 *change;
	String_t	 *key;
	KeyHash_t	 hash;
};

typedef struct cache_xfer_list_st CacheXferList_t;
struct cache_xfer_list_st {
	CXFER		*head;
	CXFER		*tail;
};

typedef enum {
	XS_IDLE,
	XS_WAITING,
	XS_READY
} XferState_t;

typedef struct cache_xfers_st CacheXfers_t;
struct cache_xfers_st {
	CacheXfers_t	*next;		/* Node linkage. */
	CacheXfers_t	*prev;
	HistoryCache_t	*cache;		/* Destination cache for samples. */
	CacheXferList_t	list;		/* List of waiting samples. */
	XferState_t	state;		/* Transfer state. */
	CacheXfers_t	*ready;		/* List of caches that accept samples.*/
};

typedef struct cache_xfers_list_st CacheXfersList_t;
struct cache_xfers_list_st {
	CacheXfers_t	*head;
	CacheXfers_t	*tail;
};

enum mem_block_en {
	MB_HIST_CACHE,		/* History Cache (shared between DCPS & RTPS).*/
	MB_CHANGE,		/* Cache Change. */
	MB_INSTANCE,		/* Instance list of cache change references. */
	MB_CCREF,		/* Cache Change Reference. */
	MB_CREF,		/* Cache Reference. */
	MB_CWAIT,		/* Cache Wait. */
	MB_CXFER,		/* Cache transfer. */
	MB_XFLIST,		/* Per cache list of transfers. */
	MB_FILTER,		/* Time-based filter context. */
	MB_FINST,		/* Filter instance node. */

	MB_END
};

static const char *mem_names [] = {
	"HIST_CACHE",
	"CHANGE",
	"INSTANCE",
	"CCREF",
	"CREF",
	"CWAIT",
	"CXFER",
	"XFLIST",
	"FILTER",
	"FINST"
};

#ifdef DDS_DEBUG
#ifdef CACHE_TRC_CHANGES
#define TRC_CHANGE(cp,s,alloc)	log_printf (CACHE_ID, 0, "%s(%p) %s\r\n", (alloc) ? "alloc" : "free", cp, s);
#else
#define TRC_CHANGE(cp,s,alloc)
#endif
#else
#define TRC_CHANGE(cp,s,alloc)
#endif

static HCREQIFCT	mon_new_fct;	/* New change monitor callback. */
static HCREQFCT		mon_remove_fct;	/* Remove change monitor callback. */
static HCREQFCT		mon_urgent_fct;	/* Request for immediate ack. */
static HCREQFCT		mon_unblock_fct;/* Unblocked indication callback. */
static HCALIVEFCT	mon_alive_fct;	/* Liveliness send function. */
static HCFINSTFCT	mon_iflush_fct;	/* Free HCI function. */
static lock_t		waiters_lock;	/* Lock on waiters list. */
static CacheWaitList_t	waiters_list;	/* Threads blocked on caches. */
static lock_t		xfers_lock;	/* Lock on transfers list. */
static CacheXfersList_t	xfers_list;	/* Transfers blocked on caches. */
static CacheXfersList_t	xfers_ready;	/* Ready transfers. */
static size_t		mem_size;	/* Total memory allocated. */
static MEM_DESC_ST	mem_blocks [MB_END]; /* Memory for cache subsystem. */


/* hc_pool_init -- Initialize the memory structures required for correct
		   operation. */

int hc_pool_init (const CACHE_CONFIG *limits)
{
	if (mem_blocks [0].md_addr) {	/* Was already initialized -- reset. */
		mds_reset (mem_blocks, MB_END);
		return (DDS_RETCODE_OK);
	}

	MDS_POOL_TYPE (mem_blocks, MB_HIST_CACHE, limits->cache, sizeof (HistoryCache_t));
	MDS_POOL_TYPE (mem_blocks, MB_INSTANCE, limits->instance, sizeof (INSTANCE));
	MDS_POOL_TYPE (mem_blocks, MB_CHANGE, limits->change, sizeof (Change_t));
	MDS_POOL_TYPE (mem_blocks, MB_CCREF, limits->ccrefs, sizeof (CCREF));
	MDS_POOL_TYPE (mem_blocks, MB_CREF, limits->crefs, sizeof (CacheRef_t));
	MDS_POOL_TYPE (mem_blocks, MB_CWAIT, limits->cwaits, sizeof (CacheWait_t));
	MDS_POOL_TYPE (mem_blocks, MB_CXFER, limits->cxfers, sizeof (CXFER));
	MDS_POOL_TYPE (mem_blocks, MB_XFLIST, limits->xflists, sizeof (CacheXfers_t));
	MDS_POOL_TYPE (mem_blocks, MB_FILTER, limits->filters, sizeof (TFilter_t));
	MDS_POOL_TYPE (mem_blocks, MB_FINST, limits->finsts, sizeof (TFNode_t));

	/* All pools defined: allocate one big chunk of data that will be split in
	   separate pools. */
	mem_size = mds_alloc (mem_blocks, mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!mem_size) {
		warn_printf ("hc_pool_init: not enough memory available!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	log_printf (CACHE_ID, 0, "hc_pool_init: %lu bytes allocated for pools.\r\n", (unsigned long) mem_size);
#endif
	lock_init_nr (waiters_lock, "CWait");
	LIST_INIT (waiters_list);
	lock_init_nr (xfers_lock, "CXfer");
	LIST_INIT (xfers_list);
	xfers_ready.head = xfers_ready.tail = NULL;

	PROF_INIT ("H:Get", cache_get);
	PROF_INIT ("H:GData", cache_get_data);
	PROF_INIT ("H:GKey", cache_get_key);
	PROF_INIT ("H:Done", cache_done);
	PROF_INIT ("H:Avail", cache_avail);
	PROF_INIT ("H:IReg", cache_register);
	PROF_INIT ("H:LKey", cache_lookup_key);
	PROF_INIT ("H:LHash", cache_lookup_hash);
	PROF_INIT ("H:IRel", cache_release);
	PROF_INIT ("H:IAdd", cache_add_inst);
	PROF_INIT ("H:IFree", cache_inst_free);

	return (DDS_RETCODE_OK);
}

/* hc_pool_free -- Free the pools used for the history cache. */

void hc_pool_free (void)
{
	mds_free (mem_blocks, MB_END);
	lock_destroy (xfers_lock);
	lock_destroy (waiters_lock);
}

/* duration2ticks -- Convert a duration parameter to a # of timer ticks. */

Ticks_t duration2ticks (const Duration_t *dp)
{
	Ticks_t		nticks;

	if (!dp || (dp->secs == 0 && dp->nanos == 0))
		return (0);

	else if (dp->secs == ~0U)
		return (~0);

	else {
		nticks = dp->secs * TICKS_PER_SEC + dp->nanos / (TMR_UNIT_MS * 1000 * 1000);
		return (nticks ? nticks : 1);
	}
}

/* hc_change_new -- Allocate a new change record. */

Change_t *hc_change_new (void)
{
	Change_t	*cp;

	cp = mds_pool_alloc (&mem_blocks [MB_CHANGE]);
	if (!cp)
		return (NULL);

	memset (cp, 0, sizeof (Change_t));
	cp->c_nrefs = 1;
	TRC_CHANGE (cp, "hc_change_new", 1);
	return (cp);
}

/* hc_change_dispose -- Dispose of a change record. */

void hc_change_dispose (Change_t *cp)
{
	TRC_CHANGE (cp, "hc_change_dispose", 0);

	if (cp->c_db)
		db_free_data (cp->c_db);
	else if (cp->c_data && cp->c_data != cp->c_xdata)
		xfree (cp->c_data);
	mds_pool_free (&mem_blocks [MB_CHANGE], cp);
}

/* hc_change_clone -- Clone an existing change record. */

Change_t *hc_change_clone (Change_t *cp)
{
	Change_t	*ncp;

	ncp = mds_pool_alloc (&mem_blocks [MB_CHANGE]);
	if (!ncp)
		return (NULL);

	*ncp = *cp;
	ncp->c_nrefs = 1;
	ncp->c_wack = 0;
	if (ncp->c_db) {
		rcl_access (ncp->c_db);
		ncp->c_db->nrefs++;
		rcl_done (ncp->c_db);
	}
	else if (cp->c_data == cp->c_xdata)
		ncp->c_data = ncp->c_xdata;

	return (ncp);
}

/* ccref_add -- Add a new reference to a Change record to a list of changes.
	        If ordered is set, the list will be sorted according to the
		timestamp of each change. */

static CCREF *ccref_add (CCLIST *list, Change_t *cp, int ordered)
{
	CCREF	*rp, *lrp, *prev_lrp;

	if ((rp = mds_pool_alloc (&mem_blocks [MB_CCREF])) == NULL) {
		log_printf (CACHE_ID, 0, "ccref_add (): out of memory!\r\n");
		return (NULL);
	}
	rp->change = cp;
	if (++cp->c_nrefs > MAX_CHANGE_REFS)
		fatal_printf ("ccref_add: maximum # of change clients exceeded!");

	if (!list->l_nchanges++) {
		LIST_IS_SINGLETON (list->l_list, *rp);
	}
	else if (!ordered ||
	         !FTIME_LT (cp->c_time, list->l_tail->change->c_time)) {
		LIST_ADD_TAIL (list->l_list, *rp);
	}
	else {
		for (lrp = list->l_head, prev_lrp = NULL;
		     lrp && !FTIME_LT (cp->c_time, lrp->change->c_time);
		     prev_lrp = lrp, lrp = LIST_NEXT (list->l_list, *lrp))
			break;

                if (prev_lrp == NULL) 
                        LIST_ADD_HEAD (list->l_list, *rp);
                else
                        LIST_INSERT (*rp, *prev_lrp);
	}
	return (rp);
}

/* ccref_delete -- Free a Change Reference that is no longer in a list back
		   to the free pool.  Returns the Change pointer. */

static INLINE void ccref_delete (CCREF *rp)
{
	TRC_CHANGE (rp->change, "ccref_delete", 0);
	hc_change_free (rp->change);
	mds_pool_free (&mem_blocks [MB_CCREF], rp);
}

/* ccref_remove_ref -- Remove a Change from a list of change references where
		       the Change reference is known and return the reference.*/

static INLINE CCREF *ccref_remove_ref (CCLIST *list, CCREF *rp)
{
	if (!--list->l_nchanges)
		if (rp->change)
			list->l_time = rp->change->c_time;
		else {
			FTIME_CLR (list->l_time);
		}
	else
		LIST_REMOVE (list->l_list, *rp);
	return (rp);
}

/* ccref_remove_change -- Remove a change from a list of change references.
			  If the reference to the change is found, the reference
			  is removed from the list and returned. */

static CCREF *ccref_remove_change (CCLIST *list, Change_t *cp)
{
	CCREF		*rp;
	int		found = 0;

	if (list->l_nchanges)
		LIST_FOREACH (list->l_list, rp)
			if (rp->change == cp) {
				found = 1;
				break;
			}

	if (!found) /* Asynchronously deleted via hc_acknowledge. */
		return (NULL);

	return (ccref_remove_ref (list, rp));
}

/* ccref_list_delete -- Delete a complete list of change references. */

static void ccref_list_delete (CCLIST *list)
{
	CCREF		*rp, *next_rp;

	if (list->l_nchanges) {
		for (rp = LIST_HEAD (list->l_list); rp; rp = next_rp) {
			next_rp = LIST_NEXT (list->l_list, *rp);
			if (!next_rp) {
				if (rp->change)
					list->l_time = rp->change->c_time;
				else {
					FTIME_CLR (list->l_time);
				}
				list->l_nchanges = 0;
			}
			ccref_delete (rp);
		}
	}
}

/* hash_cmp_fct -- Compare function for KeyHash fields. */

static int hash_cmp_fct (const void *np, const void *data)
{
	INSTANCE	**lpp = (INSTANCE **) np;

	return (memcmp (&(*lpp)->i_hash, data, sizeof (KeyHash_t)));
}

/* handle_cmp_fct -- Compare function for instance handle fields. */

static int handle_cmp_fct (const void *np, const void *data)
{
	INSTANCE	**lpp = (INSTANCE **) np;

#if defined (BIGDATA) || (WORDSIZE == 64)
	return (((int) (*lpp)->i_handle) -  *((int *) data));
#else
	return (((short) (*lpp)->i_handle) - *((short *) data));
#endif
}

/* hc_cvt_fct -- Utility function called for each list element to convert a
                 skiplist to a direct chained list. */

static int hc_cvt_fct (Skiplist_t *lp, void *node, void *arg)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) arg;
	INSTANCE	*p, **pp = (INSTANCE **) node;

	ARG_NOT_USED (lp)

	p = *pp;
	if (hcp->hc_inst_head)
		hcp->hc_inst_tail->i_next = p;
	else
		hcp->hc_inst_head = p;
	hcp->hc_inst_tail = p;
	p->i_next = NULL;
	return (1);
}

/* hc_convert_to_short -- Convert a set of skiplists to a simple linked list. */

static void hc_convert_to_short (HistoryCache_t *hcp)
{
	Skiplist_t	*handles = hcp->hc_handles;

	/*log_printf (CACHE_ID, 0, "Convert cache to short format!\r\n");*/
	sl_free (hcp->hc_hashes);
	hcp->hc_inst_head = hcp->hc_inst_tail = NULL;
	sl_walk (handles, hc_cvt_fct, hcp);
	sl_free (handles);
	hcp->hc_skiplists = 0;
	/*log_printf (CACHE_ID, 0, "Cache converted to short format!\r\n");*/
}

/* hc_convert_to_long -- Attempts to convert a simple linked list to a set of
			 skiplists. */

static void hc_convert_to_long (HistoryCache_t *hcp)
{
	INSTANCE	*p, *next_p, **pp, *last_p;
	Skiplist_t	*hashes, *handles;
	int		is_new;

	/*log_printf (CACHE_ID, 0, "Convert cache to hashed format!\r\n");*/
	hashes = sl_new (sizeof (p));
	if (!hashes) {
		log_printf (CACHE_ID, 0, "hc_convert_to_long (): out of memory for hash list!\r\n");
		return;
	}
	handles = sl_new (sizeof (p));
	if (!handles) {
		log_printf (CACHE_ID, 0, "hc_convert_to_long (): out of memory for handles list!\r\n");
		sl_free (hashes);
		return;
	}
	p = hcp->hc_inst_head;
	last_p = hcp->hc_inst_tail;
	hcp->hc_hashes = hashes;
	hcp->hc_handles = handles;
	hcp->hc_skiplists = 1;
	for (; p; p = next_p) {
		next_p = p->i_next;
		p->i_next = NULL;
		pp = sl_insert (hashes, p->i_hash.hash, &is_new, hash_cmp_fct);
		if (!pp) {
			log_printf (CACHE_ID, 0, "hc_convert_to_long (): out of memory for instance key node!\r\n");
			goto recover;
		}
		p->i_next = (is_new) ? NULL : *pp;
		*pp = p;
		pp = sl_insert (handles, &p->i_handle, NULL, handle_cmp_fct);
		if (!pp) {
			log_printf (CACHE_ID, 0, "hc_convert_to_long (): out of memory for instance handle node!\r\n");
			goto recover;
		}
		*pp = p;
	}
	/*log_printf (CACHE_ID, 0, "Cache converted to hashed format!\r\n");*/
	return;

    recover:
	/* Out of memory error occurred. */

	/* 1. Convert back to short form. */
	hc_convert_to_short (hcp);

	/* 2. Recover not-yet converted nodes. */
	p->i_next = next_p;
	if (hcp->hc_inst_head)
		hcp->hc_inst_tail->i_next = p;
	else
		hcp->hc_inst_head = p;
	hcp->hc_inst_tail = last_p;
	return;
}

/* hc_get_instance_handle -- Lookup an instance based on an instance handle. */

static INSTANCE *hc_get_instance_handle (HistoryCache_t *hcp,
				         unsigned       inst_handle)
{
	INSTANCE	*p, **pp;
	CacheHandle_t	h = inst_handle;

	if (!hcp->hc_skiplists) {

		/* Search instance in simple linked-list. */
		for (p = hcp->hc_inst_head; p; p = p->i_next)
			if (p->i_handle == inst_handle)
				return (p);

		return (NULL);
	}
	else {
		/* Search instance in handle skiplist. */
		pp = sl_search (hcp->hc_handles, &h, handle_cmp_fct);
		return ((pp) ? *pp : NULL);
	}
}

#define HC_CAN_RECOVER(ip)	(!p->i_nchanges && \
				 (p->i_kind & NOT_ALIVE_UNREGISTERED) != 0 && \
				 p->i_recover)

/* hc_check_recover -- Check whether an instance is recoverable. */

static int hc_check_recover (Skiplist_t *list, void *node, void *arg)
{
	INSTANCE	*p, **pp = (INSTANCE **) node, **opp = (INSTANCE **) arg;

	ARG_NOT_USED (list)

	p = *pp;
	if (HC_CAN_RECOVER (p) &&
	    (!*opp || FTIME_LT (p->i_time, (*opp)->i_time)))
		*opp = p;
	return (1);
}

/* hc_recoverable -- Lookup a recoverable instance. */

static INSTANCE *hc_recoverable (HistoryCache_t *hcp)
{
	INSTANCE	*p, *old_p = NULL;

	if (hcp->hc_writer)
		return (NULL);

	if (!hcp->hc_skiplists) {

		/* Search instance in simple linked-list. */
		for (p = hcp->hc_inst_head; p; p = p->i_next)
			if (HC_CAN_RECOVER (p) &&
			    (!old_p || FTIME_LT (p->i_time, old_p->i_time)))
				old_p = p;
	}
	else	/* Search in skiplist. */
		sl_walk (hcp->hc_handles, hc_check_recover, &old_p);

	return (old_p);
}

/* hc_check_limits -- Verify if a sample can be added to an existing instance
		      without bumping into the configured resource limits. */

static INSTANCE *hc_check_limits (HistoryCache_t *hcp,
				  INSTANCE       *ip,
				  InstanceHandle *handle,
				  unsigned       ooo,
				  RejectCause_t  *cause)
{
	if (ip->i_ndata >= hcp->hc_max_depth) {
		*cause = RC_SamplesPerInstanceLimit;
		*handle = 0;
		return (NULL);
	}
	else if (hcp->hc_ndata + ooo >= hcp->hc_max_samples) {
		*cause = RC_SamplesLimit;
		*handle = 0;
		return (NULL);
	}
	*cause = RC_Accepted;
	*handle = ip->i_handle;
	return (ip);
}

/* HC_CHECK -- If c is non-NULL, the caller wishes to check resource limits
	       before deciding to add a sample.  Check limits if so, otherwise
	       just return the found instance. */

#define	HC_CHECK(hcp,p,h,add,o,c){ if (c && add && hcp->hc_must_ack)		\
					return (hc_check_limits(hcp,p,h,o,c));	\
				   else { *h = (p)->i_handle; return (p); }}

/* hc_get_instance_key -- Lookup or add (depending on the add argument) an
			  instance in the list of instances of the history
			  cache.  If the function returns a NULL pointer, the
			  instance could either not be added, or it doesn't
			  exist.  Either way, the *handle argument will be
			  undefined.  The key argument can be either a char
			  pointer (keylen != 0) or a string pointer (!keylen).*/

static INSTANCE *hc_get_instance_key (HistoryCache_t  *hcp,
				      const KeyHash_t *hp,
				      void            *key,
				      unsigned        keylen,
				      InstanceHandle  *handle,
				      int             add,
				      unsigned        ooo,
				      RejectCause_t   *cause)
{
	INSTANCE	*p, **hash_pp, **handle_pp, *r_ip = NULL;
	int		is_new, error;
	const unsigned char *dp, *sp;
	unsigned	dl, sl;
	KeyHash_t	hash;

	if (key) {
		if (!keylen) {
			sp = (const unsigned char *) str_ptr ((String_t *) key);
			sl = str_len ((String_t *) key);
		}
		else {
			sp = key;
			sl = keylen;
		}
	}
	else {
		sp = hp->hash;
		sl = hcp->hc_key_size;
	}
	if (!hcp->hc_skiplists) {

		/* Search instance in simple linked-list. */
		for (p = hcp->hc_inst_head; p; p = p->i_next) {
			if (p->i_key) {
				dp = (const unsigned char *) str_ptr (p->i_key);
				dl = str_len (p->i_key);
			}
			else {
				dp = p->i_hash.hash;
				dl = hcp->hc_key_size;
			}
			if (dl == sl && !memcmp (dp, sp, dl))

				/* Check resource limits. */
				HC_CHECK (hcp, p, handle, add, ooo, cause);
		}

		/* Not in cache yet, check if room for new sample/instance. */
		if (!add || 
		    (hcp->hc_ninstances >= hcp->hc_max_inst && 
		     (r_ip = hc_recoverable (hcp)) == NULL)) {

		    max_instances:
			if (cause)
				*cause = RC_InstanceLimit;
			goto not_accepted;
		}
		else if (cause && hcp->hc_nchanges + ooo >= hcp->hc_max_samples) {

		    max_samples:
			*cause = RC_SamplesLimit;
			goto not_accepted;
		}

		/* Attempt to convert list to a set of skiplists. */
		if (hcp->hc_ninstances >= MAX_INST_LLIST)
			hc_convert_to_long (hcp);
	}

	/* Derive hashkey if it doesn't exist yet. */
	if (!hp) {
		error = DDS_HashFromKey (hash.hash, sp, sl,
				hcp->hc_secure_h,
				hcp->hc_endpoint->ep.topic->type->type_support);
		if (error)
			goto max_instances;

		hp = (const KeyHash_t *) &hash;
	}

	/* If skiplists are used, try to add/lookup node via hashkey. */
	if (hcp->hc_skiplists) {

		/* Lookup/add instance to set of skiplists. */
		if (!add ||
		    (hcp->hc_ninstances >= hcp->hc_max_inst &&
		    (r_ip = hc_recoverable (hcp)) == NULL) ||
		    (cause && 
		     hcp->hc_nchanges + ooo >= hcp->hc_max_samples)) {
			hash_pp = sl_search (hcp->hc_hashes, hp, hash_cmp_fct);
			is_new = 0;
			if (!hash_pp) {
				if (!add)
					goto not_accepted;
				else if (hcp->hc_ninstances >= hcp->hc_max_inst)
					goto max_instances;
				else
					goto max_samples;
			}
		}
		else {
			hash_pp = sl_insert (hcp->hc_hashes, hp, &is_new,
								hash_cmp_fct);
			if (!hash_pp) {
				log_printf (CACHE_ID, 0, "hc_get_instance_key (): out of memory for key node!\r\n");
				goto max_instances;
			}
		}
		if (!add || !is_new) {	/* Hash found - check if collision. */
			if (hcp->hc_key_size &&
			    hcp->hc_key_size <= sizeof (KeyHash_t) &&
			    !hcp->hc_secure_h)

				/* Check resource limits. */
				HC_CHECK (hcp, *hash_pp, handle, add, ooo, cause)

			/* Long key! Search instance in collision-list. */
			for (p = *hash_pp; p; p = p->i_next)
				if ((!keylen && p->i_key == key) ||
				    (str_len (p->i_key) == keylen &&
				     !memcmp (str_ptr (p->i_key), key, keylen)))
					
					/* Check resource limits. */
					HC_CHECK (hcp, *hash_pp, handle, add, ooo, cause)

			if (!add)
				goto not_accepted;
		}
		else
			*hash_pp = NULL;
	}
	else
		hash_pp = NULL;		/* Avoid compiler warning! */

	/* Entry was not found; create a new instance. */
	if (r_ip) {
		hc_inst_free (hcp, r_ip->i_handle);
	}
	p = mds_pool_alloc (&mem_blocks [MB_INSTANCE]);
	if (!p) {
		log_printf (CACHE_ID, 0, "hc_get_instance_key (): out of memory for instance!\r\n");
		return (NULL);
	}
	p->i_next = NULL;
	FTIME_CLR (p->i_time);
	p->i_nchanges = 0;
	p->i_ndata = 0;
	p->i_nwriters = 0;
	if (add == LH_ADD_SET_H)
		p->i_handle = *handle;
	else {
		do {
			if (hcp->hc_last_handle >= MAX_INST_HANDLE) {
				hcp->hc_last_handle = 1;
				hcp->hc_recycle = 1;
			}
			else
				hcp->hc_last_handle++;
			p->i_handle = hcp->hc_last_handle;
		}
		while (hcp->hc_recycle &&
		       hc_get_instance_handle (hcp, p->i_handle));
		if (handle)
			*handle = p->i_handle;
	}
	p->i_hash = *hp;
	if (!hcp->hc_key_size ||
	    hcp->hc_key_size > sizeof (KeyHash_t) ||
	    hcp->hc_secure_h) {
		if (!keylen)
			p->i_key = str_ref ((String_t *) key);
		else
			p->i_key = str_new ((char *) key, keylen, keylen, 0);
		/*log_printf (CACHE_ID, 0, "hc_get_instance_key(): str_new()=>%p\r\n", (void *) p->i_key);*/
		if (!p->i_key)
			goto no_str_mem;
	}
	else
		p->i_key = NULL;
	p->i_kind = NOT_ALIVE_UNREGISTERED;	/* No writers yet. */
	p->i_view = NEW;			/* New instance. */
	p->i_wait = 0;
	p->i_deadlined = 0;
	p->i_inform = 0;
	p->i_recover = 0;
	p->i_registered = 0;
	p->i_disp_cnt = p->i_no_w_cnt = 0;	/* Generation counters. */
	p->i_tbf = NULL;
	if (hcp->hc_skiplists != 0) {

		/* Add to hash-collision list. */
		p->i_next = *hash_pp;	/* Add at head -- faster :-) */
		*hash_pp = p;

		/* Try to insert in handle list. */
		handle_pp = sl_insert (hcp->hc_handles, &p->i_handle, 
						&is_new, handle_cmp_fct);
		if (!handle_pp)
			goto no_handle;

		*handle_pp = p;
	}
	else {
		if (hcp->hc_inst_head)
			hcp->hc_inst_tail->i_next = p;
		else
			hcp->hc_inst_head = p;
		hcp->hc_inst_tail = p;
	}
#ifdef EXTRA_STATS
	memset (&p->i_stats, 0, sizeof (p->i_stats));
	p->i_register = 0;
	p->i_dispose = 0;
	p->i_unregister = 0;
#endif
	hcp->hc_ninstances++;
	return (p);

    no_handle:
    	if (p->i_next)
		*hash_pp = p->i_next;
	else
		sl_delete (hcp->hc_hashes, hp, hash_cmp_fct);
	if (p->i_key) {
		/*log_printf (CACHE_ID, 0, "hc_get_instance_key(): str_unref(%p)\r\n", (void *) p->i_key);*/
		str_unref (p->i_key);
	}

    no_str_mem:
    	mds_pool_free (&mem_blocks [MB_INSTANCE], p);

    not_accepted:
	if (handle)
		*handle = 0;
	return (NULL);
}

/* hc_tbf_inst_cleanup -- Cleanup a filter instance context. */

static void hc_tbf_inst_cleanup (TFNode_t *p)
{
	TFilter_t	*fp = p->filter;
	TFNode_t	**np;
	INSTANCE	*ip;

	/* If some sample still pending, get rid of it. */
	if (p->sample) {
		(*fp->done_fct) (fp->proxy, p->sample);
		p->sample = NULL;
	}

	/* Remove from filter list. */
	LIST_REMOVE (*p->filter, *p);
	if (LIST_EMPTY (*p->filter))
		tmr_stop (&fp->timer);

	/* Remove from cache instance. */
	ip = p->instance;
	if (ip)
		np = &ip->i_tbf;
	else
		np = &p->filter->cache->hc_tbf;
	for (; *np; np = &(*np)->i_next)
		if (*np == p) {
			*np = p->i_next;
			break;
		}

	/* Release to free pool. */
	mds_pool_free (&mem_blocks [MB_FINST], p);
}

/* hc_free_instance -- Free an instance keyed by a handle. */

static void hc_free_instance (HistoryCache_t *hcp, unsigned inst_handle)
{
	INSTANCE	*p, *prev, **handle_pp, **hash_pp;
	CCREF		*crp, *irp;
	CacheHandle_t	h = inst_handle;

	prof_start (cache_inst_free);

	/*log_printf (CACHE_ID, 0, "hc_inst_free(%lu);\r\n", entity_handle (hcp->hc_endpoint->ep.fh));*/
	if (!hcp->hc_skiplists) {

		/* Search instance in simple linked-list. */
		for (p = hcp->hc_inst_head, prev = NULL;
		     p;
		     prev = p, p = p->i_next)
			if (p->i_handle == inst_handle)
				break;

		if (!p) /* !found. */
			return;

		/* Unlink instance from instance list. */
		if (prev)
			prev->i_next = p->i_next;
		else
			hcp->hc_inst_head = p->i_next;
		if (!p->i_next)
			hcp->hc_inst_tail = prev;
	}
	else {
		/* Remove instance from handle list. */
		handle_pp = sl_search (hcp->hc_handles, &h, handle_cmp_fct);
		if (!handle_pp)
			return;

		p = *handle_pp;
		sl_delete (hcp->hc_handles, &h, handle_cmp_fct);

		/* Remove instance from hashkey list. */
		hash_pp = sl_search (hcp->hc_hashes, p->i_hash.hash, hash_cmp_fct);
		if (!hash_pp)
			dbg_printf ("hc_free_instance (): instance not in key list!\r\n");

		else if ((hcp->hc_key_size && 
			  hcp->hc_key_size <= sizeof (KeyHash_t) &&
			  !hcp->hc_secure_h) || !p->i_next)
			sl_delete (hcp->hc_hashes, p->i_hash.hash, hash_cmp_fct);
		else {
			/* Long key! Search instance in collision-list. */
			for (p = *hash_pp, prev = NULL;
			     p;
			     prev = p, p = p->i_next)
				if (p->i_handle == inst_handle)
					break;

			if (!p) /* !found. */
				return;

			if (prev)
				prev->i_next = p->i_next;
			else
				*hash_pp = p->i_next;
		}
	}

	/* Instance is no longer in any list. */
	hcp->hc_ninstances--;

	/* Notify monitor of instance disappearance. */
	if (p->i_inform)
		(*mon_iflush_fct) (hcp->hc_mon_user, p);

	/* Free all Time-based filter contexts. */
	while (p->i_tbf)
		hc_tbf_inst_cleanup (p->i_tbf);

	/* Free all Cache samples. */
	while (p->i_nchanges) {
		/*log_printf (CACHE_ID, 0, "hc_inst_free() - remove sample!\r\n");*/

		/* Remove first list entry. */
		irp = p->i_head;
		if (irp->change->c_kind == ALIVE) {
			p->i_ndata--;
			hcp->hc_ndata--;
		}
		LIST_REMOVE (p->i_list, *irp);
		p->i_nchanges--;
		if (irp->change->c_wack) {
			hcp->hc_unacked -= irp->change->c_wack;
			if (hcp->hc_monitor) {
				(*mon_remove_fct) (hcp->hc_mon_user, irp->change);
				STATS_INC (p->i_stats.m_remove);
			}
		}
		crp = irp->mirror;
		ccref_delete (irp);
		ccref_remove_ref (&hcp->hc_changes, crp);
		if (crp->change)
			crp->change->c_cached = 0;
		ccref_delete (crp);
	}

	/* Delete all instance parameters: key & writers list. */
	if (p->i_key) {
		/*log_printf (CACHE_ID, 0, "hc_free_instance(): str_unref(%p)\r\n", (void *) p->i_key);*/
		str_unref (p->i_key);
	}
	if (p->i_nwriters > DNWRITERS)
		xfree (p->i_writers.ptr);

	/* Destroy the instance. */
    	mds_pool_free (&mem_blocks [MB_INSTANCE], p);

	/* If list is small enough, convert it to a simple list to save some
	   memory resources. */
	if (hcp->hc_skiplists &&
	    hcp->hc_ninstances <= MIN_INST_LLIST &&
	    !hcp->hc_sl_walk)	/* Convert if not currently walking. */
		hc_convert_to_short (hcp);

	prof_stop (cache_inst_free, 1);
}

/* hc_accepts -- Check if a sample can be added to a single instance cache.  If
		 there is room in the cache, RC_Accepted is returned, otherwise
		 one of the reject causes are returned. */

int hc_accepts (Cache_t cache, unsigned ooo)
{
	HistoryCache_t *hcp = (HistoryCache_t *) cache;

	if (!ooo && hcp->hc_nchanges == hcp->hc_max_samples)
		hcp->hc_blocked = 1;
	return (hcp->hc_nchanges + ooo < hcp->hc_max_samples);
}


typedef int (*INSTFCT) (INSTANCE *ip, void *arg);

typedef struct hc_visit_data_st {
	INSTFCT		fct;
	void		*arg;
} HC_VISIT_DATA;

/* hc_visit_instance -- Instance skiplist node visit function. */

static int hc_visit_instance (Skiplist_t *list, void *node, void *arg)
{
	INSTANCE	*p, **pp = (INSTANCE **) node;
	HC_VISIT_DATA	*vdp = (HC_VISIT_DATA *) arg;

	ARG_NOT_USED (list)

	p = *pp;
	return ((*vdp->fct) (p, vdp->arg));
}

/* hc_walk_instances -- Walk over the list of instances. */

static void hc_walk_instances (HistoryCache_t *hcp, INSTFCT fct, void *arg)
{
	INSTANCE	*p, *next_p;
	HC_VISIT_DATA	vd;

	if (!hcp->hc_skiplists) {

		/* Search instance in simple linked-list. */
		for (p = hcp->hc_inst_head; p; p = next_p) {
			next_p = p->i_next;
			if (!(*fct) (p, arg))
				break;
		}
	}
	else {
		if (hcp->hc_sl_walk++ == 3)
			fatal_printf ("hc_walk_instances: recursion too deep!");

		vd.fct = fct;
		vd.arg = arg;
		sl_walk (hcp->hc_handles, hc_visit_instance, &vd);
		if (!--hcp->hc_sl_walk && hcp->hc_ninstances <= MIN_INST_LLIST)
			hc_convert_to_short (hcp);
	}
}

/* hc_inst_free -- Free the instance with the given instance handle. */

void hc_inst_free (Cache_t hcp, InstanceHandle handle)
{
	hc_free_instance ((HistoryCache_t *) hcp, handle);
}

/* hc_inst_done -- Cleanup an instance from a cache. */

void hc_inst_done (Cache_t hcp, const unsigned char *key, size_t keylen)
{
	HCI		ip;
	InstanceHandle	h;

	ip = hc_lookup_key (hcp, key, keylen, &h);
	if (ip)
		hc_free_instance ((HistoryCache_t *) hcp, h);
}

typedef struct {
	HistoryCache_t *hcp;
	GuidPrefix_t *prefix;
} reclaim_arg;

static int reclaim_fct (INSTANCE *ip, void *arg)
{
	reclaim_arg *reclaim = (reclaim_arg *) arg;

	if (!memcmp (&ip->i_hash, reclaim->prefix, sizeof (GuidPrefix_t)))
		hc_free_instance (reclaim->hcp, ip->i_handle);

	return (1);
}

void hc_reclaim_keyed (Cache_t cache, GuidPrefix_t *prefix)
{
	HistoryCache_t *hcp = (HistoryCache_t *) cache;
	reclaim_arg arg;

	arg.prefix = prefix;
	arg.hcp = hcp;

	hc_walk_instances (hcp, reclaim_fct, &arg);
}

/* hc_xfer_lookup -- Lookup a transfer list for a reader cache. */

static CacheXfers_t *hc_xfer_lookup (HistoryCache_t *hcp)
{
	CacheXfers_t	*xp;

	LIST_FOREACH (xfers_list, xp)
		if (xp->cache == hcp)
			return (xp);

	return (NULL);
}

/* hc_xfer_add -- Add a pending transfer to a reader context. */

static CacheXfers_t *hc_xfer_add (Cache_t        cache,
				  INSTANCE       *ip,
				  Change_t       *cp,
				  HistoryCache_t *hcp,
				  Change_t       *ncp)
{
	CacheXfers_t	*xp;
	CXFER		*sp;

	sp = mds_pool_alloc (&mem_blocks [MB_CXFER]);
	if (!sp)
		return (NULL);

	sp->scache = cache;
	sp->shci = ip;
	if (ip) {
		sp->key = ip->i_key;
		sp->hash = ip->i_hash;
	}
	sp->seqnr = cp->c_seqnr;
	sp->change = ncp;
	lock_take (xfers_lock);
	if (!hcp->hc_blocked || 
	    (xp = hc_xfer_lookup (hcp)) == NULL) {
		xp = mds_pool_alloc (&mem_blocks [MB_XFLIST]);
		if (!xp) {
			lock_release (xfers_lock);
			mds_pool_free (&mem_blocks [MB_CXFER], sp);
			return (NULL);
		}
		xp->cache = hcp;
		LIST_INIT (xp->list);
		xp->state = XS_IDLE;
		LIST_ADD_TAIL (xfers_list, *xp);
	}
	LIST_ADD_TAIL (xp->list, *sp);
	if  (xp->state == XS_IDLE)
		xp->state = XS_WAITING;
	if (sp->key)
		str_ref (sp->key);
	lock_release (xfers_lock);
	return (xp);
}

/* hc_add_key -- Add a change to a history cache. */

static int hc_add_key (HistoryCache_t  *cache,
		       Change_t        *cp,
		       const KeyHash_t *hp,
		       String_t        *key);

/* hc_transfer_change -- Transfer a cache change from a writer to a local
			 reader. */

static void hc_transfer_change (HistoryCache_t *wcache,	/* Writer Cache. */
				INSTANCE       *wip, 	/* Writer Instance. */
				Change_t       *cp,	/* Writer Change. */
				HistoryCache_t *hcp,	/* Reader Cache. */
				int            lock)
{
	Change_t		*ncp;
	Reader_t		*rp;
	CacheXfers_t		*xp;
	HCI			hci;
	int			error;

	/* Due to potentially different instance handles it is *not* possible to
	   reuse the change entry directly via its c_nrefs field.  Clone the
	   change and enqueue the cloned info. */
	if ((ncp = hc_change_clone (cp)) != NULL) {

		ncp->c_writer = entity_handle (&wcache->hc_endpoint->ep.entity);
		rp = (Reader_t *) hcp->hc_endpoint;

		/* Lock the destination cache. */
		if (lock && lock_take (rp->r_lock)) {
			hc_change_free (ncp);
			return;
		}

		/* Set reception timestamp if not yet done. */
		if (!FTIME_SEC (ncp->c_time) && !FTIME_FRACT (ncp->c_time))
			sys_getftime (&ncp->c_time);

		/* Add cloned sample to destination cache. */
		hci = NULL;
		if (hcp->hc_tfilter) {
			if (wip) {
				hci = hc_get_instance_key (hcp,
						           &wip->i_hash,
						           wip->i_key,
							   0,
						           &ncp->c_handle,
						           LH_ADD_NEW_H,
							   0,
						           NULL);
				if (!hci) {
					dcps_sample_rejected (rp,
					    DDS_REJECTED_BY_INSTANCES_LIMIT, 0);
					hc_change_free (ncp);
					return;
				}
			}
			if (!hc_tbf_add (hcp->hc_filters, hci, &ncp->c_time, ncp, 1))

				/* Sample queued. */
				return;
		}
		rcl_access (ncp);
		ncp->c_nrefs++;	/* Don't free change in hc_add_*() functions. */
		rcl_done (ncp);
		if (!wip || hci)
			error = hc_add_inst (hcp, ncp, hci, 1);
		else
			error = hc_add_key (hcp, ncp, &wip->i_hash, wip->i_key);

		/* In case of KEEP_ALL and if RESOURCE_LIMITS QoS settings
		   prevent us from adding data, the DDS_RETCODE_NO_DATA error is
		   returned.  In that case, add the sample to a pending
		   transfer list and increment c_wack, as if the sample was sent
		   on a reliable RTPS connection. */
		if (error == DDS_RETCODE_NO_DATA) {
			xp = hc_xfer_add (wcache, wip, cp, hcp, ncp);
			if (xp)
				cp->c_wack++;
		}
		hc_change_free (ncp);

		/* Unlock the destination cache. */
		if (lock)
			lock_release (rp->r_lock);
	}
}

/* hc_in_dest -- Check if a reader is in a list of destination handles. */

static int hc_in_dest (Reader_t *rep, handle_t *hp, unsigned max)
{
	unsigned	i;

	for (i = 0; i < max; i++) {
		if (!hp)
			break;

		if (rep->r_handle == *hp)
			return (1);

		hp++;
	}
	return (0);
}

static int hc_filter_match (Reader_t *rp, Change_t *cp)
{
	FilteredTopic_t	*ftp;
	DBW		dbw;
	int		err, res;

	if (!cp->c_data)
		return (1);

	ftp = (FilteredTopic_t *) rp->r_topic;
	dbw.dbp = cp->c_db;
	dbw.data = cp->c_data;
	dbw.left = cp->c_db->size - ((unsigned char *) cp->c_data - cp->c_db->data);
	dbw.length = cp->c_length;

	err = bc_interpret (&ftp->data.program,
			    ftp->data.filter.expression_pars,
			    &ftp->data.cache,
			    &dbw,
			    NULL,
			    1,
			    ftp->topic.type->type_support,
			    &res);
	return (!err && res);
}

/* hc_match_begin -- Match between a local writer and a local reader is detected. */

static void hc_match_begin (HistoryCache_t *wcp, HistoryCache_t *rcp)
{
	Writer_t	*wep = (Writer_t *) wcp->hc_endpoint;
	Reader_t	*rep = (Reader_t *) rcp->hc_endpoint;
	CCREF		*rp;
	INSTANCE	*ip;
	CacheRef_t	*cp;

	/* Add matched reader reference. */
	cp = (CacheRef_t *) mds_pool_alloc (&mem_blocks [MB_CREF]);
	if (!cp) {
		warn_printf ("hc_new_reader: out of memory for local match!");
		return;
	}
	cp->next = wcp->hc_readers;
	cp->cache = rcp;
	wcp->hc_readers = cp;

	/* Notify matching endpoints. */
	dcps_subscription_match (rep, 1, &wep->w_ep);
	dcps_publication_match (wep, 1, &rep->r_ep);

	/* New writer for cache. */
	hc_rem_writer_add (rcp, wep->w_handle);

	/* Enable liveliness. */
	if (rcp->hc_liveliness)
		liveliness_enable (&wep->w_ep, &rep->r_ep);

	/* Enable deadline. */
	if (rcp->hc_deadline)
		deadline_enable (&wep->w_ep, &rep->r_ep);

	/* Enable lifespan. */
	if (rcp->hc_lifespan)
		lifespan_enable (&wep->w_ep, &rep->r_ep);

	/* If writer cache contains data, transfer data. */
	if (wcp->hc_nchanges)
		LIST_FOREACH (wcp->hc_changes.l_list, rp)
			if ((!rp->change->c_dests [0] ||
			     hc_in_dest (rep, rp->change->c_dests, MAX_DW_DESTS)) &&
			    ((rep->r_topic->entity.flags & EF_FILTERED) == 0 ||
			     hc_filter_match (rep, rp->change))) {
				if (wcp->hc_multi_inst)
					ip = hc_get_instance_handle (wcp, rp->change->c_handle);
				else
					ip = NULL;
				hc_transfer_change (wcp, ip, rp->change, rcp, 0);
			}
}

/* hc_match_end -- Match between a local writer and a local reader has ended. */

static void hc_match_end (HistoryCache_t *wcp,
			  HistoryCache_t *rcp,
			  CacheRef_t     *cp,
			  CacheRef_t     *prev_cp)
{
	/* Disable lifespan. */
	if (rcp->hc_lifespan)
		lifespan_disable (&wcp->hc_endpoint->ep, &rcp->hc_endpoint->ep);

	/* Disable deadline checks. */
	if (rcp->hc_deadline)
		deadline_disable (&wcp->hc_endpoint->ep, &rcp->hc_endpoint->ep);

	/* Disable liveliness checks. */
	if (rcp->hc_liveliness)
		liveliness_disable (&wcp->hc_endpoint->ep, &rcp->hc_endpoint->ep);

	/* Notify writer gone. */
	hc_rem_writer_removed (rcp, wcp->hc_endpoint->ep.entity.handle);

	/* Notify matching writer. */
	dcps_publication_match ((Writer_t *) wcp->hc_endpoint, 0, &rcp->hc_endpoint->ep);
	dcps_subscription_match ((Reader_t *) rcp->hc_endpoint, 0, &wcp->hc_endpoint->ep);

	/* Remove reader from writers reader list. */
	if (prev_cp)
		prev_cp->next = cp->next;
	else
		wcp->hc_readers = cp->next;

	/* Free data. */
	mds_pool_free (&mem_blocks [MB_CREF], cp);
}

#define	local_active(fh) (((fh) & (EF_LOCAL | EF_ENABLED)) == (EF_LOCAL | EF_ENABLED))

/* hc_new_writer -- A new writer was added, update local matching endpoints. */

static void hc_new_writer (HistoryCache_t *hcp, Endpoint_t *readers)
{
	Endpoint_t	*ep;
	Writer_t	*wp;
	Reader_t	*rp;
	DDS_QOS_POLICY_ID qid;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t	*dp;
#endif

	wp = (Writer_t *) hcp->hc_endpoint;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	dp = wp->w_publisher->domain;
#endif
	for (ep = readers; ep; ep = ep->next) {
		if (!local_active (ep->entity.flags))
			continue;

		rp = (Reader_t *) ep;
		if (!qos_same_partition (wp->w_publisher->qos.partition,
					 rp->r_subscriber->qos.partition)) {
			dcps_offered_incompatible_qos (wp, DDS_PARTITION_QOS_POLICY_ID);
			dcps_requested_incompatible_qos (rp, DDS_PARTITION_QOS_POLICY_ID);
			continue;
		}
		if (!qos_match (qos_ptr (wp->w_qos), &wp->w_publisher->qos,
			        qos_ptr (rp->r_qos), &rp->r_subscriber->qos, &qid)) {
			dcps_offered_incompatible_qos (wp, qid);
			dcps_requested_incompatible_qos (rp, qid);
			continue;
		}
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		if (dp->security &&
		    dp->access_protected &&
		    sec_check_local_writer_match (dp->participant.p_permissions,
		    				  dp->participant.p_permissions,
						  wp,
						  ep))
			continue;
#endif
		/* Match detected: connect local endpoint caches. */
		hc_match_begin (hcp, rp->r_cache);
	}
}

/* hc_end_writer -- A writer will be removed, update local matching endpoints.*/

static void hc_end_writer (HistoryCache_t *hcp)
{
	CacheRef_t	*cp;

	while ((cp = hcp->hc_readers) != NULL)
		hc_match_end (hcp, cp->cache, cp, NULL);
}

/* hc_matches -- Check if the writer cache matches with the given reader cache. */

int hc_matches (Cache_t wc, Cache_t rc)
{
	HistoryCache_t	*wcp = (HistoryCache_t *) wc;
	HistoryCache_t	*rcp = (HistoryCache_t *) rc;
	CacheRef_t	*cp;

	for (cp = wcp->hc_readers; cp; cp = cp->next)
		if (cp->cache == rcp)
			return (1);

	return (0);
}

/* hc_new_reader -- A new reader was added, update local matching endpoints. */

static void hc_new_reader (HistoryCache_t *hcp, Endpoint_t *writers)
{
	Endpoint_t	*ep;
	Writer_t	*wep;
	Reader_t	*rep;
	DDS_QOS_POLICY_ID qid;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t	*dp;
#endif

	rep = (Reader_t *) hcp->hc_endpoint;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	dp = rep->r_subscriber->domain;
#endif
	for (ep = writers; ep; ep = ep->next) {
		if (!local_active (ep->entity.flags))
			continue;

		wep = (Writer_t *) ep;
		if (!qos_same_partition (wep->w_publisher->qos.partition,
					 rep->r_subscriber->qos.partition)) {
			dcps_offered_incompatible_qos (wep, DDS_PARTITION_QOS_POLICY_ID);
			dcps_requested_incompatible_qos (rep, DDS_PARTITION_QOS_POLICY_ID);
			continue;
		}
		if (!qos_match (qos_ptr (wep->w_qos), &wep->w_publisher->qos,
			        qos_ptr (rep->r_qos), &rep->r_subscriber->qos, &qid)) {
			dcps_offered_incompatible_qos (wep, qid);
			dcps_requested_incompatible_qos (rep, qid);
			continue;
		}
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		if (dp->security &&
		    dp->access_protected &&
		    sec_check_local_reader_match (dp->participant.p_permissions,
		    				  dp->participant.p_permissions,
						  rep,
						  ep))
			continue;
#endif

		/* Match detected: connect endpoint caches locally. */
		hc_match_begin (wep->w_cache, rep->r_cache);
	}
}

/* hc_end_reader -- A reader will be removed, update local matching endpoints.*/

static void hc_end_reader (HistoryCache_t *hcp, Endpoint_t *writers)
{
	Endpoint_t	*ep;
	Writer_t	*wp;
	HistoryCache_t	*wcp;
	CacheRef_t	*cp, *prev_cp;

	for (ep = writers; ep; ep = ep->next) {
		if (!local_active (ep->entity.flags))
			continue;

		wp = (Writer_t *) ep;
		wcp = wp->w_cache;
		for (prev_cp = NULL, cp = wcp->hc_readers;
		     cp;
		     prev_cp = cp, cp = cp->next)
			if (cp->cache == hcp)
				break;

		if (!cp)
			continue;

		/* Was matched: disconnect endpoint caches. */
		hc_match_end (wcp, hcp, cp, prev_cp);
	}
}

/* hc_new -- Create a new history cache with the given parameters. */

Cache_t hc_new (void *endpoint, int prefix)
{
	HistoryCache_t		*hcp;
	UniQos_t		*qp;
	const TypeSupport_t	*tsp;
	Reader_t		*rp;
	int			writer;

	if ((hcp = mds_pool_alloc (&mem_blocks [MB_HIST_CACHE])) == NULL) {
		log_printf (CACHE_ID, 0, "hc_new (): out of memory!\r\n");
		return (NULL);
	}
	hcp->hc_endpoint = endpoint;
	qp = &hcp->hc_endpoint->ep.qos->qos;
	tsp = hcp->hc_endpoint->ep.topic->type->type_support;
	hcp->hc_ref_type = DDS_HIST_PURGE_NA;
	if ((hcp->hc_endpoint->ep.entity.flags & EF_BUILTIN) != 0)
		if (!tsp) {
			hcp->hc_multi_inst = 1;
			hcp->hc_key_size = sizeof (DDS_BuiltinTopicKey_t);
		}
		else {
			hcp->hc_multi_inst = tsp->ts_keys;
			hcp->hc_key_size = tsp->ts_mkeysize;
			if (tsp->ts_prefer == MODE_PL_CDR)
				hcp->hc_ref_type = tsp->ts_pl->builtin;
		}
	else {
		hcp->hc_multi_inst = tsp->ts_keys;
		hcp->hc_key_size = tsp->ts_mkeysize;
	}
	writer = entity_writer (entity_type (&hcp->hc_endpoint->ep.entity));
	hcp->hc_writer = writer;
	hcp->hc_durability = qp->durability_kind >= DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	hcp->hc_src_time = qp->destination_order_kind;
	hcp->hc_inst_order = qp->presentation_access_scope != DDS_TOPIC_PRESENTATION_QOS ||
			     qp->presentation_ordered_access;
	hcp->hc_readers = NULL;
	hcp->hc_monitor = 0;
	hcp->hc_skiplists = 0;
	hcp->hc_kind = NOT_ALIVE_UNREGISTERED;
	hcp->hc_view = NEW;
	hcp->hc_auto_disp = !qp->no_autodispose;
	hcp->hc_prefix = prefix;
	hcp->hc_blocked = 0;
#ifdef DDS_NATIVE_SECURITY
	hcp->hc_secure_h = ENC_DATA (hcp->hc_endpoint);
#else
	hcp->hc_secure_h = 0;
#endif
	if (qp->history_kind == DDS_KEEP_ALL_HISTORY_QOS) {
		hcp->hc_must_ack = 1;
		if (hcp->hc_multi_inst) {
			if (qp->resource_limits.max_samples_per_instance == DDS_LENGTH_UNLIMITED)
				hcp->hc_max_depth = MAX_LIST_CHANGES;
			else
				hcp->hc_max_depth = qp->resource_limits.max_samples_per_instance;
		}
		else if (qp->resource_limits.max_samples == DDS_LENGTH_UNLIMITED)
			hcp->hc_max_depth = MAX_LIST_CHANGES;
		else
			hcp->hc_max_depth = qp->resource_limits.max_samples;
	}
	else {
		hcp->hc_must_ack = 0;
		hcp->hc_max_depth = qp->history_depth;
	}
	if (qp->resource_limits.max_instances == DDS_LENGTH_UNLIMITED)
		hcp->hc_max_inst = MAX_INST_HANDLE;
	else
		hcp->hc_max_inst = qp->resource_limits.max_instances;
	if (qp->resource_limits.max_samples == DDS_LENGTH_UNLIMITED)
		hcp->hc_max_samples = MAX_LIST_CHANGES;
	else
		hcp->hc_max_samples = qp->resource_limits.max_samples;
	FTIME_CLR (hcp->hc_time);
	hcp->hc_nchanges = 0;
	hcp->hc_ndata = 0;
	hcp->hc_inst_head = hcp->hc_inst_tail = NULL;
	hcp->hc_ninstances = 0;
	hcp->hc_notify_fct = NULL;
	hcp->hc_last_handle = 0;
	hcp->hc_recycle = 0;
	hcp->hc_exclusive = qp->ownership_kind;
	hcp->hc_liveliness = liveliness_used (qp);
	hcp->hc_deadline = deadline_used (qp);
	hcp->hc_deadlined = 0;
	hcp->hc_dlc_idle = 0;
	hcp->hc_tfilter = 0;
	hcp->hc_sl_walk = 0;
	hcp->hc_lsc_idle = 0;
	if (hcp->hc_writer) {

		/* Start Lifespan timer if Lifespan QoS enabled. */
		hcp->hc_lifespan = lifespan_used (qp);
		if (hcp->hc_lifespan)
			lifespan_enable (endpoint, NULL);

		hcp->hc_last_seqnr.low = 0;
		hcp->hc_last_seqnr.high = 0;
	}
	else {
		hcp->hc_lifespan = 0;
		if (!hcp->hc_multi_inst) {
			hcp->hc_disp_cnt = 0;	/* Disposed generation count. */
			hcp->hc_no_w_cnt = 0;	/* No Writers generation count. */
		}
		rp = (Reader_t *) hcp->hc_endpoint;
		if (rp->r_time_based_filter.minimum_separation.sec ||
		    rp->r_time_based_filter.minimum_separation.nanosec)
			hcp->hc_tfilter = 1;

		/* Start Autopurge QoS timers if enabled. */
		if (autopurge_no_writers_used (rp)) {
			hcp->hc_purge_nw = 1;
			autopurge_no_writers_enable (rp);
		}
		if (autopurge_disposed_used (rp)) {
			hcp->hc_purge_dis = 1;
			autopurge_disposed_enable (rp);
		}
	}
	hcp->hc_apw_idle = 0;
	hcp->hc_apd_idle = 0;
	if (!hcp->hc_multi_inst)
		hcp->hc_tbf = NULL;
	hcp->hc_filters = NULL;
	hcp->hc_unacked = 0;
#ifdef EXTRA_STATS
	memset (&hcp->hc_stats, 0, sizeof (hcp->hc_stats));
	hcp->hc_mon_start  = 0;
	hcp->hc_mon_stop   = 0;
	hcp->hc_mon_replay = 0;
	hcp->hc_unblock    = 0;
#endif

	return (hcp);
}

/* hc_enable -- Enable a cache, i.e. allow local matching with other local
		endpoints. */

void hc_enable (Cache_t cache)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	FilteredTopic_t	*ftp;

	/* Check local matching endpoints. */
	if (entity_writer (entity_type (&hcp->hc_endpoint->ep.entity))) {
		hc_new_writer (hcp, hcp->hc_endpoint->ep.topic->readers);
		for (ftp = hcp->hc_endpoint->ep.topic->filters; ftp; ftp = ftp->next)
			hc_new_writer (hcp, ftp->topic.readers);
	}
	else if ((hcp->hc_endpoint->ep.topic->entity.flags & EF_FILTERED) == 0)
		hc_new_reader (hcp, hcp->hc_endpoint->ep.topic->writers);
	else {
		ftp = (FilteredTopic_t *) hcp->hc_endpoint->ep.topic;
		hc_new_reader (hcp, ftp->related->writers);
	}
}

/* hc_updated_writer_qos -- Update a local Writer cache due to QoS parameter
			    updates. */

static void hc_updated_writer_qos (HistoryCache_t *hcp, Endpoint_t *readers)
{
	Endpoint_t	*ep;
	Writer_t	*wp;
	Reader_t	*rp;
	CacheRef_t	*cp, *prev_cp;
	DDS_QOS_POLICY_ID qid;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t	*dp;
#endif

	wp = (Writer_t *) hcp->hc_endpoint;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	dp = wp->w_publisher->domain;
#endif
	for (ep = readers; ep; ep = ep->next) {
		if (!local_active (ep->entity.flags))
			continue;

		rp = (Reader_t *) ep;

		for (prev_cp = NULL, cp = hcp->hc_readers;
		     cp;
		     prev_cp = cp, cp = cp->next)
			if (cp->cache == (HistoryCache_t *) rp->r_cache)
				break;

		if (!qos_same_partition (wp->w_publisher->qos.partition,
					 rp->r_subscriber->qos.partition)) {
			if (cp)
				hc_match_end (hcp, rp->r_cache, cp, prev_cp);
			dcps_offered_incompatible_qos (wp, DDS_PARTITION_QOS_POLICY_ID);
			dcps_requested_incompatible_qos (rp, DDS_PARTITION_QOS_POLICY_ID);
			continue;
		}
		if (!qos_match (qos_ptr (wp->w_qos), &wp->w_publisher->qos,
			        qos_ptr (rp->r_qos), &rp->r_subscriber->qos, &qid)) {
			if (cp)
				hc_match_end (hcp, rp->r_cache, cp, prev_cp);
			dcps_offered_incompatible_qos (wp, qid);
			dcps_requested_incompatible_qos (rp, qid);
			continue;
		}
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		if (dp->security &&
		    dp->access_protected &&
		    sec_check_local_writer_match (dp->participant.p_permissions,
		    				  dp->participant.p_permissions,
						  wp,
						  ep)) {
			if (cp)
				hc_match_end (hcp, rp->r_cache, cp, prev_cp);
			continue;
		}
#endif

		/* Match detected: add to writer's reader list if not yet matched. */
		if (!cp)
			hc_match_begin (hcp, rp->r_cache);
	}
}

/* hc_updated_reader_qos -- Update a local Reader cache due to QoS parameter
			    updates. */

static void hc_updated_reader_qos (HistoryCache_t *hcp, Endpoint_t *writers)
{
	Endpoint_t	*ep;
	Writer_t	*wep;
	Reader_t	*rep;
	CacheRef_t	*cp, *prev_cp;
	HistoryCache_t	*wcp;
	DDS_QOS_POLICY_ID qid;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t	*dp;
#endif

	rep = (Reader_t *) hcp->hc_endpoint;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	dp = rep->r_subscriber->domain;
#endif
	for (ep = writers; ep; ep = ep->next) {
		if (!local_active (ep->entity.flags))
			continue;

		wep = (Writer_t *) ep;
		wcp = wep->w_cache;
		for (prev_cp = NULL, cp = wcp->hc_readers;
		     cp;
		     prev_cp = cp, cp = cp->next)
			if (cp->cache == hcp)
				break;

		if (!qos_same_partition (wep->w_publisher->qos.partition,
					 rep->r_subscriber->qos.partition)) {
			if (cp)
				hc_match_end (wcp, hcp, cp, prev_cp);
			dcps_offered_incompatible_qos (wep, DDS_PARTITION_QOS_POLICY_ID);
			dcps_requested_incompatible_qos (rep, DDS_PARTITION_QOS_POLICY_ID);
			continue;
		}
		if (!qos_match (qos_ptr (wep->w_qos), &wep->w_publisher->qos,
			        qos_ptr (rep->r_qos), &rep->r_subscriber->qos, &qid)) {
			if (cp)
				hc_match_end (wcp, hcp, cp, prev_cp);
			dcps_offered_incompatible_qos (wep, qid);
			dcps_requested_incompatible_qos (rep, qid);
			continue;
		}
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		if (dp->security &&
		    dp->access_protected &&
		    sec_check_local_reader_match (dp->participant.p_permissions,
		    				  dp->participant.p_permissions,
						  rep,
						  ep)) {
			if (cp)
				hc_match_end (wcp, hcp, cp, prev_cp);
			continue;
		}
#endif

		/* Match detected: add to writer's reader list if not matched yet. */
		if (!cp)
			hc_match_begin (wcp, hcp);
	}
}

/* hc_qos_update -- QoS of an endpoint was updated.  Check local matching since
		    matched endpoints may become unmatched and vice-versa. */

void hc_qos_update (Cache_t cache)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	FilteredTopic_t	*ftp;

	/* Check local matching endpoints. */
	if (entity_writer (entity_type (&hcp->hc_endpoint->ep.entity))) {
		/*dbg_printf ("hc_qos_update: writer qos updated!\r\n");*/
		hc_updated_writer_qos (hcp, hcp->hc_endpoint->ep.topic->readers);
		for (ftp = hcp->hc_endpoint->ep.topic->filters; ftp; ftp = ftp->next)
			hc_updated_writer_qos (hcp, ftp->topic.readers);
	}
	else if ((hcp->hc_endpoint->ep.topic->entity.flags & EF_FILTERED) == 0) {
		/*dbg_printf ("hc_qos_update: reader qos updated!\r\n");*/
		hc_updated_reader_qos (hcp, hcp->hc_endpoint->ep.topic->writers);
	}
	else {
		/*dbg_printf ("hc_qos_update: filtered reader qos updated!\r\n");*/
		ftp = (FilteredTopic_t *) hcp->hc_endpoint->ep.topic;
		hc_updated_reader_qos (hcp, ftp->related->writers);
	}
}

/* hc_wait_lookup -- Check if there is already someone waiting on either a
		     a change or a cache, and return the context if so. */

static CacheWait_t *hc_wait_lookup (HistoryCache_t *hcp, Change_t *cp)
{
	CacheWait_t	*wp;

	LIST_FOREACH (waiters_list, wp)
		if (wp->cache == hcp &&
		    ((!cp && !wp->wchange) ||
		     (cp && wp->wchange && wp->change == cp)))
			return (wp);

	return (NULL);
}

/* hc_wait_add -- Add a waiter to a Waiter context. */

static CacheWait_t *hc_wait_add (HistoryCache_t *hcp, INSTANCE *ip, Change_t *cp)
{
	CacheWait_t	*wp;

	lock_take (waiters_lock);
	if ((!cp && hcp->hc_blocked) || (cp && cp->c_urgent)) {
		wp = hc_wait_lookup (hcp, cp);
		if (wp) {
			if (ip && ip == wp->instance) {
				lock_release (waiters_lock);
				return (NULL);
			}
			wp->nwaiting++;
			goto waiter_ready;
		}
	}
	wp = mds_pool_alloc (&mem_blocks [MB_CWAIT]);
	if (!wp) {
		lock_release (waiters_lock);
		return (NULL);
	}
	wp->cache = hcp;
	wp->instance = ip;
	if (ip)
		ip->i_wait = 1;
	wp->change = cp;
	cond_init (wp->wcond);
	wp->nwaiting = 1;
	if (cp) {
		wp->wchange = 1;
		cp->c_urgent = 1;
	}
	else {
		wp->wchange = 0;
		hcp->hc_blocked = 1;
	}
	LIST_ADD_TAIL (waiters_list, *wp);

    waiter_ready:

	lock_release (waiters_lock);
	return (wp);
}

/* hc_wait_free -- Release a waiter from a Waiter context. */

static void hc_wait_free (CacheWait_t *wp)
{
	lock_take (waiters_lock);
	if (!--wp->nwaiting) {
		if (wp->instance)
			wp->instance->i_wait = 0;
		if (wp->change)
			wp->change->c_urgent = 0;
		else
			wp->cache->hc_blocked = 0;
		LIST_REMOVE (waiters_list, *wp);
		cond_destroy (wp->wcond);
		mds_pool_free (&mem_blocks [MB_CWAIT], wp);
	}
	lock_release (waiters_lock);
}

/* hc_wait_acked -- Wait until a change is either acknowledged by all reliable
		    connections or a time-out occurred. */

static int hc_wait_acked (HistoryCache_t *hcp, INSTANCE *ip, Change_t *cp)
{
	Ticks_t		d, now, end_time;	/* *10ms */
	UniQos_t	*qp = &hcp->hc_endpoint->ep.qos->qos;
	CacheWait_t	*wp;
	int		timeout, ret;
	struct timespec	ts;

	if (hcp->hc_monitor) {
		/*log_printf (CACHE_ID, 0, "!Urgency!\r\n");*/
		(*mon_urgent_fct) (hcp->hc_mon_user, cp);
#ifdef EXTRA_STATS
		if (ip)
			ip->i_stats.m_urgent++;
		else
			hcp->hc_stats.m_urgent++;
#endif
	}
	if (dds_listener_state)
		return (0);

	now = sys_getticks ();
	end_time = now + duration2ticks ((Duration_t *) &qp->reliability_max_blocking_time);

	if (ip && ip->i_wait)
		return (1);
	
	wp = hc_wait_add (hcp, ip, cp);
	if (!wp)
		return (0);

	duration2timespec (&qp->reliability_max_blocking_time, &ts);
	timeout = 0;
	do {
		if (ts.tv_sec || ts.tv_nsec)
			ret = cond_wait_to (wp->wcond, ((Writer_t *) hcp->hc_endpoint)->w_lock, ts);
		else
			ret = cond_wait (wp->wcond, ((Writer_t *) hcp->hc_endpoint)->w_lock);
		d = end_time - now;
		if (ret || !d || d >= 0x7fffffffUL) {
			timeout = 1;
			break;
		}
		if (ts.tv_sec || ts.tv_nsec) {
			ts.tv_sec = d / TICKS_PER_SEC;
			d -= (Ticks_t) (ts.tv_sec * TICKS_PER_SEC);
			ts.tv_nsec = d * TMR_UNIT_MS * 1000000;
		}
		now = sys_getticks ();
	}
	while (!ret && wp->change && wp->change->c_wack);
	hc_wait_free (wp);
	return (timeout);
}

/* hc_remove_i -- Remove a change from an instance of the history cache. */

static int hc_remove_i (HistoryCache_t *hcp, INSTANCE *ip, Change_t *cp, int rel)
{
	CCREF	*crp, *irp;

	/* On reliable connections we should either force remove the change or
	   wait (block) until the change is acknowledged by all readers. */
	if (hcp->hc_must_ack) {
		if (cp->c_wack) { /* Writer: wait for acknowledgements. */
			if (hc_wait_acked (hcp, ip, cp))
				return (DDS_RETCODE_TIMEOUT);
		}
		else if (!hcp->hc_writer && rel 
		         /*&& cp->c_sstate == NOT_READ : shouldn't matter?*/) {
			hcp->hc_blocked = 1;
			return (DDS_RETCODE_NO_DATA);
		}
	}
	else if (cp->c_wack) { /* Writer: forced removal. */
		hcp->hc_unacked -= cp->c_wack;
		/*log_printf (CACHE_ID, 0, "hc_remove_i(): unacked=%u\r\n", hcp->hc_unacked);*/
		if (hcp->hc_monitor) {
			(*mon_remove_fct) (hcp->hc_mon_user, cp);
#ifdef EXTRA_STATS
			if (ip)
				ip->i_stats.m_remove++;
			else
				hcp->hc_stats.m_remove++;
#endif
		}
	}

	/* Remove change from cache lists. */
	if (ip) {
		irp = ccref_remove_change (&ip->i_list, cp);
		if (!irp)
			return (DDS_RETCODE_OK);

		if (cp->c_nrefs < 2)
			fatal_printf ("hc_remove_i: invalid change!");

		crp = irp->mirror;
		ccref_delete (irp);
		ccref_remove_ref (&hcp->hc_changes, crp);
		if (cp->c_kind == ALIVE) {
			ip->i_ndata--;
			hcp->hc_ndata--;
		}
	}
	else {
		crp = ccref_remove_change (&hcp->hc_changes, cp);
		if (!crp)
			return (DDS_RETCODE_OK);

		if (cp->c_kind == ALIVE)
			hcp->hc_ndata--;
		if (cp->c_nrefs < 1)
			fatal_printf ("hc_remove_i: invalid change!");

	}
	if (crp->change)
		crp->change->c_cached = 0;

	/* Finally delete the change reference. */
	ccref_delete (crp);

	return (DDS_RETCODE_OK);
}

/* hc_lookup_handle -- Lookup a handle in a handle array. */

static int hc_lookup_handle (handle_t writers [], handle_t w, unsigned n)
{
	handle_t	*lp, *mp, *hp;

	lp = writers;
	hp = &writers [n - 1];
	while (lp <= hp) {
		mp = lp + ((hp - lp) >> 1);
		if (w < *mp)
			hp = --mp;
		else if (w > *mp)
			lp = ++mp;
		else
			return (mp - writers);
	}
	return (-(lp - writers) - 1);
}

/* writer_strength -- Return the writer QoS parameter for Ownership Strength. */

static INLINE unsigned writer_strength (handle_t h)
{
	Entity_t	*p = entity_ptr (h);
	Endpoint_t	*ep;

	if (p != NULL && entity_writer (entity_type (p))) {
		ep = (Endpoint_t *) p;
		return (ep->qos->qos.ownership_strength.value);
	}
	else
		return (0);
}

/* hc_add_writer_handle -- Add a writer to a reader's handle array. */

static int hc_add_writer_handle (INSTANCE *ip, handle_t writer, unsigned index)
{
	WLBLOCK		*bp;
	handle_t	*hp;

	/*log_printf (CACHE_ID, 0, "hc_add_writer_handle(%u);\r\n", writer);*/
	if (ip->i_nwriters < DNWRITERS)
		hp = ip->i_writers.w;
	else if (ip->i_nwriters == DNWRITERS) {
		bp = xmalloc (sizeof (WLBLOCK));
		if (!bp)
			return (DDS_RETCODE_OUT_OF_RESOURCES); /* Ignored by caller. ;-) */

		bp->max_writers = NWRITERS_INC;
		memcpy (bp->w, ip->i_writers.w, DNWRITERS * sizeof (handle_t));
		ip->i_writers.ptr = bp;
		hp = bp->w;
	}
	else if (ip->i_nwriters == ip->i_writers.ptr->max_writers) {
		bp = xrealloc (ip->i_writers.ptr, sizeof (WLBLOCK) +
		       	       ip->i_writers.ptr->max_writers * sizeof (handle_t));
		if (!bp)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		bp->max_writers += NWRITERS_INC;
		ip->i_writers.ptr = bp;
		hp = bp->w;
	}
	else {
		bp = ip->i_writers.ptr;
		hp = bp->w;
	}
	if (index < ip->i_nwriters)
		memmove (hp + index + 1,
			 hp + index,
			 (ip->i_nwriters - index) * sizeof (handle_t));
	hp [index] = writer;
	ip->i_nwriters++;
	return (DDS_RETCODE_OK);
}

static void hc_rem_writer_handle (INSTANCE *ip, unsigned index)
{
	handle_t	*hp;
	WLBLOCK		*bp;

	/*log_printf (CACHE_ID, 0, "hc_rem_writer_handle()\r\n");*/

	if (ip->i_nwriters <= DNWRITERS) {
		bp = NULL;
		hp = ip->i_writers.w;
	}
	else {
		bp = ip->i_writers.ptr;
		hp = bp->w;
	}
	ip->i_nwriters--;
	if (index < ip->i_nwriters)
		memmove (hp + index, hp + index + 1,
			 (ip->i_nwriters - index) * sizeof (handle_t));
	if (ip->i_nwriters == DNWRITERS) {
		memcpy (ip->i_writers.w, hp, DNWRITERS * sizeof (handle_t));
		xfree (bp);
	}
}

/* writer_id_info -- Extract GuidPrefix and EntityId from a local or remote writer. */

#define writer_id_info(p, p_prefix, p_eid)	if (entity_local (p->flags)) { \
		p_prefix = &((Writer_t *) p)->w_publisher->domain->participant.p_guid_prefix; \
		p_eid = ((Writer_t *) p)->w_entity_id.w; } else { \
		p_prefix = &((DiscoveredWriter_t *) p)->dw_participant->p_guid_prefix; \
		p_eid = ((DiscoveredWriter_t *) p)->dw_entity_id.w; }

/* cmp_writer_guid -- Compare the GUIDs of two writers. */

static int cmp_writer_guid (handle_t h1, handle_t h2)
{
	Entity_t		*p1, *p2;
	GuidPrefix_t		*p1_prefix, *p2_prefix;
	uint32_t		p1_eid, p2_eid;
	int			r;

	p1 = entity_ptr (h1);
	p2 = entity_ptr (h2);

	if (!p1 ||
	    !p2 ||
	    !entity_writer (entity_type (p1)) ||
	    !entity_writer (entity_type (p1)))
		return (-1);

	if (entity_local (p1->flags) && entity_local (p2->flags))
		return (((Writer_t *) p1)->w_entity_id.w - 
			((Writer_t *) p2)->w_entity_id.w);

	writer_id_info (p1, p1_prefix, p1_eid);
	writer_id_info (p2, p2_prefix, p2_eid);
	r = memcmp (p1_prefix, p2_prefix, GUIDPREFIX_SIZE);
	if (!r)
		r = p1_eid - p2_eid;
	return (r);
}

/* hc_update_owner -- Update the exclusive owner of an instance.
		      The used algorithm compares the respective ownership
		      strengths of each owner and picks the one with the highest
		      strength.  If multiple writers have the same (highest)
		      ownership strength, the writer with the lowest GUID value
		      is chosen. */

static void hc_update_owner (INSTANCE *ip)
{
	handle_t	*hp, high_w;
	unsigned	i, i_s, high_s;

	/*log_printf (CACHE_ID, 0, "hc_update_owner()\r\n");*/

	/* Find the writer with the highest strength in the list of
	   writers -> set that one to the new owner. */
	hp = (ip->i_nwriters <= DNWRITERS) ? ip->i_writers.w :
					     ip->i_writers.ptr->w;
	high_w = 0;
	high_s = 0;
	for (i = 0; i < ip->i_nwriters; i++) {
		i_s = writer_strength (hp [i]);
		if (!high_w || i_s > high_s) {
			high_w = hp [i];
			high_s = i_s;
		}
		else if (i_s == high_s &&
			 cmp_writer_guid (high_w, hp [i]) > 0)
			high_w = hp [i];
	}
	ip->i_owner = high_w;
}

/* hc_rem_writer_gone -- Remove a writer from a reader's handle array. */

static int hc_rem_writer_gone (INSTANCE       *ip,
			       handle_t       writer)
{
	int	i;

	/*log_printf (CACHE_ID, 0, "hc_rem_writer_gone(%u);\r\n", writer);*/

	if (ip->i_nwriters <= DNWRITERS)
		i = hc_lookup_handle (ip->i_writers.w, writer, ip->i_nwriters);
	else
		i = hc_lookup_handle (ip->i_writers.ptr->w, writer, ip->i_nwriters);
	if (i < 0)
		return (0);

	hc_rem_writer_handle (ip, i);
	return (1);
}

typedef struct inst_rem_w_info_st {
	HistoryCache_t	*hcp;
	handle_t	writer;
	FTime_t		time;
} InstRWInfo_t;


/* hc_release -- Release an existing instance from the cache. */

static int hc_release (Cache_t        cache,
		       InstanceHandle h,
		       HCI            hci,
		       ChangeKind_t   k,
		       handle_t       w,
		       const FTime_t  *t,
	               handle_t       dests [],
	               unsigned       ndests)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	Change_t	*cp;
	int		error;

	prof_start (cache_release);

	/*log_printf (CACHE_ID, 0, "hc_release(%lu, %u);\r\n", entity_handle (hcp->hc_endpoint->ep.fh), w);*/

	/* If there are no interested parties in this info, ignore request. */
	if (!hcp->hc_durability && !hcp->hc_monitor && !hcp->hc_readers && 
							!hcp->hc_deadline) {

		/* Update Liveliness. */
		hcp->hc_endpoint->ep.entity.flags |= EF_ALIVE;
		if ((hcp->hc_endpoint->ep.entity.flags & EF_LNOTIFY) != 0)
			liveliness_restored (hcp->hc_endpoint, 0);

		prof_stop (cache_release, 1);
		return (DDS_RETCODE_OK);
	}
	if (ndests > MAX_DW_DESTS)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	cp = hc_change_new ();
	if (!cp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	TRC_CHANGE (cp, "hc_release", 1);

	if (ndests)
		memcpy (cp->c_dests, dests, ndests * sizeof (handle_t));

     /* cp->c_wack = 0; */
	cp->c_kind = k;
	cp->c_linear = 1;
     /* cp->c_guid = NULL; **.prefix = *hcp->c_guid_prefix; cp->c_guid.entity_id = hcp->c_entity_id;*/
	cp->c_time = *t;
	cp->c_writer = w;
	cp->c_handle = h;
     /* cp->c_db = NULL; */
     /* cp->c_data = NULL; */
     /* cp->c_length = 0; */
	error = hc_add_inst (cache, cp, hci, 0);
	prof_stop (cache_release, 1);
	return (error);
}

static int inst_rem_writer (INSTANCE *ip, void *arg)
{
	InstRWInfo_t	*rwp = (InstRWInfo_t *) arg;

	if (ip->i_nwriters) {
		if (ip->i_nwriters == 1 && ip->i_writers.w [0] == rwp->writer) {
			hc_release ((Cache_t) rwp->hcp,
				    ip->i_handle,
				    ip,
				    NOT_ALIVE_UNREGISTERED,
				    rwp->writer,
				    &rwp->time,
				    NULL,
				    0);
			if (rwp->hcp->hc_apw_idle)
				autopurge_no_writers_continue (rwp->hcp->hc_endpoint);
		}
		else if (hc_rem_writer_gone (ip, rwp->writer) &&
			 rwp->hcp->hc_exclusive &&
			 rwp->writer == ip->i_owner)
			hc_update_owner (ip);
	}
	return (1);
}

void hc_add_sample (void *proxy, void *sample, HCI hci, int rel)
{
	hc_add_inst ((Cache_t) proxy, (Change_t *) sample, hci, rel);
}

void hc_done_sample (void *proxy, void *sample)
{
	ARG_NOT_USED (proxy)

	hc_change_free ((Change_t *) sample);
}

void hc_rem_writer_add (Cache_t cache, handle_t writer)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	Reader_t	*rp;
	TBF		fp;

	ARG_NOT_USED (writer)

	if (!hcp->hc_tfilter)
		return;

	rp = (Reader_t *) hcp->hc_endpoint;
	if (hcp->hc_filters) {
		hcp->hc_filters->nusers++;
		return;
	}
	fp = hc_tbf_new (cache,
			 cache,
			 hc_add_sample,
			 hc_done_sample,
			 (Duration_t *) &rp->r_time_based_filter.
			 			minimum_separation);
	if (!fp)
		warn_printf ("hc_rem_writer_add: can't add time-based filter context!\r\n");
}

void hc_rem_writer_removed (Cache_t cache, handle_t writer)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	InstRWInfo_t	rw;

	/* Remove Time-based filter contexts if this was the last writer. */
	if (hcp->hc_tfilter &&
	    hcp->hc_filters &&
	    !--hcp->hc_filters->nusers)
		hc_tbf_free (cache, cache);

	/* No need to continue if not multi-instances. */
	if (!hcp->hc_multi_inst || !hcp->hc_ninstances)
		return;

	/*log_printf (CACHE_ID, 0, "hc_rem_writer_removed(%lu, %u);\r\n", entity_handle (hcp->hc_endpoint->ep.fh), writer);*/

	sys_getftime (&rw.time);
	rw.hcp = hcp;
	rw.writer = writer;
	hc_walk_instances (hcp, inst_rem_writer, &rw);
}
 
/* hc_add_i -- Add a change to an instance of the history cache. */

static int hc_add_i (HistoryCache_t *hcp, INSTANCE *ip, Change_t *cp)
{
	CCREF		*crp, *irp;
	CacheRef_t	*xp;
	Reader_t	*rp;
	int		order = hcp->hc_src_time;
#ifdef EXTRA_STATS
	INST_STATS	*sp;
#endif

	/* Add change to cache. */
	crp = ccref_add (&hcp->hc_changes, cp, order);
	if (!crp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (ip) {
		if ((irp = ccref_add (&ip->i_list, cp, order)) == NULL) {
			ccref_remove_ref (&hcp->hc_changes, crp);
			ccref_delete (crp);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		crp->mirror = irp;
		irp->mirror = crp;
#ifdef EXTRA_STATS
		sp = &ip->i_stats;
#endif
		if (cp->c_kind == ALIVE) {
			ip->i_ndata++;
			hcp->hc_ndata++;
#ifdef EXTRA_STATS
			sp->i_octets += cp->c_length;
			sp->i_add++;
		}
		else if (cp->c_kind == NOT_ALIVE_DISPOSED)
			ip->i_dispose++;
		else if (cp->c_kind == NOT_ALIVE_UNREGISTERED)
			ip->i_unregister++;
		else {
			ip->i_unregister++;
			ip->i_dispose++;
#endif
		}
	}
	else {
		crp->mirror = NULL;
		if (cp->c_kind == ALIVE)
			hcp->hc_ndata++;
#ifdef EXTRA_STATS
		sp = &hcp->hc_stats;
		sp->i_octets += cp->c_length;
		sp->i_add++;
#endif
	}
	cp->c_cached = 1;
	rcl_access (cp);
	cp->c_nrefs--;	/* Since we're not going to refer to the change anymore
			   we can safely hand it over to the cache. */
	rcl_done (cp);

	/* Change added successfully -- notify all interested parties. */
	if (hcp->hc_monitor) {
		if (hcp->hc_must_ack) {
			if (ip &&
			    hcp->hc_max_depth != MAX_LIST_CHANGES &&
			    ip->i_ndata >= (hcp->hc_max_depth >> 1))
				cp->c_urgent = 1;
			else if (hcp->hc_max_samples != MAX_LIST_CHANGES &&
			         hcp->hc_ndata >= (hcp->hc_max_samples >> 1))
				cp->c_urgent = 1;
			else
				cp->c_urgent = 0;
		}
		else
			cp->c_urgent = 0;

		if (cp->c_kind == ZOMBIE && hcp->hc_unacked > 12)
			cp->c_urgent = 1;

		(*mon_new_fct) (hcp->hc_mon_user, cp, ip);
		STATS_INC (sp->m_new);
		cp->c_urgent = 0;
		hcp->hc_unacked += cp->c_wack;
		/*log_printf (CACHE_ID, 0, "mon_new_fct(): unacked=%u\r\n", hcp->hc_unacked);*/
	}
	else if (hcp->hc_notify_fct) {
		(*hcp->hc_notify_fct) (hcp->hc_notify_user, hcp);
		STATS_INC (sp->l_notify);
	}

	/* Writers write directly to local reader caches. */
	for (xp = hcp->hc_readers; xp; xp = xp->next) {
		rp = (Reader_t *) xp->cache->hc_endpoint;
		if ((!cp->c_dests [0] ||
		     hc_in_dest (rp, cp->c_dests, MAX_DW_DESTS)) &&
		    ((rp->r_topic->entity.flags & EF_FILTERED) == 0 ||
		     hc_filter_match (rp, cp))) {
			hc_transfer_change (hcp, ip, cp, xp->cache, 
#ifdef RW_TOPIC_LOCK
								0);
#else
								1);
#endif
			STATS_INC (sp->c_transfer);
		}
	}

	/* If no outstanding acknowledgements remain, and this is a volatile
	   cache, we must delete the change immediately. */
	if (hcp->hc_writer && !hcp->hc_durability && !cp->c_wack) {
		hc_remove_i (hcp, ip, cp, 0);

		/* If an instance is unregistered, clean it up. */
		if (hcp->hc_multi_inst &&
		    (ip->i_kind & NOT_ALIVE_UNREGISTERED) != 0 &&
		    !ip->i_nchanges)
			hc_free_instance (hcp, ip->i_handle);

		return (DDS_RETCODE_OK);
	}

	/* If lifespan checks were enabled but idle, restart them. */
	if (hcp->hc_lifespan && hcp->hc_lsc_idle)
		lifespan_continue (hcp->hc_endpoint);

	return (DDS_RETCODE_OK);
}

/* hc_unblock -- Unblock the writers of a Reader cache. */

static void hc_unblock (HistoryCache_t *hcp)
{
	CacheXfers_t	*xp;
	int		first_ready;

	hcp->hc_blocked = 0;
	lock_take (xfers_lock);
	xp = hc_xfer_lookup (hcp);
	if (xp) {
		if (xp->state < XS_READY) {
			first_ready = (xfers_ready.head == NULL);
			xp->ready = NULL;
			if (first_ready)
				xfers_ready.head = xp;
			else
				xfers_ready.tail->next = xp;
			xfers_ready.tail = xp;
			xp->state = XS_READY;
		}
		else
			first_ready = 0;
		lock_release (xfers_lock);
		if (first_ready)
			dds_signal (DDS_EV_CACHE_X);
	}
	else {
		lock_release (xfers_lock);
		if (hcp->hc_inform) {
			(*mon_unblock_fct) (hcp->hc_mon_user, NULL);
			STATS_INC (hcp->hc_unblock);
		}
	}
}

/* hc_change_remove -- Remove a change from the history cache. */

static int hc_change_remove (HistoryCache_t *hcp,
			     CCLIST         *lp,
			     INSTANCE       *ip,
			     CCREF          *rp)
{
	CCLIST		*xlp;
	CCREF		*xrp;
	Change_t	*cp;

	ccref_remove_ref (lp, rp);
	cp = rp->change;
	if ((xrp = rp->mirror) != NULL) {

		/* Need to remove from other list as well! */
		if (lp == &hcp->hc_changes) {
			if (!ip)
				ip = hc_get_instance_handle (hcp,
							rp->change->c_handle);
			xlp = &ip->i_list;
		}
		else
			xlp = &hcp->hc_changes;
		if (cp->c_kind == ALIVE)
			ip->i_ndata--;
		ccref_remove_ref (xlp, xrp);
		ccref_delete (xrp);
	}
	if (cp->c_kind == ALIVE)
		hcp->hc_ndata--;
	ccref_delete (rp);
	return (DDS_RETCODE_OK);
}

/* hc_free -- Release a complete history cache. */

void hc_free (Cache_t cache)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	CCREF		*rp;

	/* Remove matching local endpoints. */
	if (hcp->hc_writer) {
		hc_end_writer (hcp);

		/* If Lifespan QoS was enabled, remove it. */
		if (hcp->hc_lifespan)
			lifespan_disable (&hcp->hc_endpoint->ep, NULL);
	}
	else {
		hc_end_reader (hcp, hcp->hc_endpoint->ep.topic->writers);

		/* Disable Reader Data Lifecycle checks. */
		autopurge_no_writers_disable ((Reader_t *) hcp->hc_endpoint);
		autopurge_disposed_disable ((Reader_t *) hcp->hc_endpoint);
	}

	/* Remove Time-based filter contexts. */
	while (hcp->hc_filters)
		hc_tbf_free (hcp, hcp->hc_filters->proxy);

	/* Remove cache changes. */
	if (hcp->hc_multi_inst) {

		/* Remove all existing/queued changes and their instances. */
		while (hcp->hc_nchanges) {
			rp = LIST_HEAD (hcp->hc_changes.l_list);
			hc_free_instance (hcp, rp->change->c_handle);
		}

		/* Remove all empty instances. */
		if (hcp->hc_skiplists)
			hc_convert_to_short (hcp);
		while (hcp->hc_ninstances)
			hc_free_instance (hcp, hcp->hc_inst_head->i_handle);
	}
	else
		ccref_list_delete (&hcp->hc_changes);

	/* Free the cache. */
	mds_pool_free (&mem_blocks [MB_HIST_CACHE], hcp);
}

/* hc_register -- Register a new instance in the cache, based on given key. */

HCI hc_register (Cache_t             hcp,
		 const unsigned char *key,
		 size_t              keylen,
		 const FTime_t       *time,
		 InstanceHandle      *h)
{
	INSTANCE	*ip;

	ARG_NOT_USED (time)

	prof_start (cache_register);
	if (!key) {
		*h = 0;
		return (NULL);
	}
	ip = hc_get_instance_key ((HistoryCache_t *) hcp, NULL,
				  (void *) key, keylen, h, 1, 0, NULL);
	if (!ip)
		*h = 0;
	else {
		ip->i_registered = 1;
		STATS_INC (ip->i_register);
	}
	prof_stop (cache_register, 1);
	return ((HCI) ip);
}

/* hc_not_yet_read -- Check if sample has been read by the user. */

#define	hc_not_yet_read(cp)	((cp)->c_kind == ALIVE && (cp)->c_sstate == NOT_READ)

/* hc_add -- Add a change to a cache instance.  This function makes sure that
	     the history depth is respected and all listeners are properly
	     notified of all operations. */

static int hc_add (HistoryCache_t *hcp,
		   INSTANCE       *ip,
		   CCLIST         *lp,
		   Change_t       *cp,
		   ChangeKind_t   kind,
		   unsigned       *disp_cnt,
		   unsigned       *no_w_cnt,
		   int            rel)
{
	int	error, order, new_writer/*, not_read*/;

	if (hcp->hc_multi_inst) {

		/* Update Deadline parameters. */
		if (ip->i_deadlined) {
			ip->i_deadlined = 0;
			if (hcp->hc_dlc_idle)
				deadline_continue (hcp->hc_endpoint);
		}

		/* If reader, then check/update list of writers and ownership.*/
		if (!hcp->hc_writer) {

			/* If this is an unregister, just remove the writer from
			   the writers list. */
			new_writer = 0;
			if ((cp->c_kind & NOT_ALIVE_UNREGISTERED) != 0) {
				/*log_printf (CACHE_ID, 0, "hc_add(%lu, NOT_ALIVE_UNREGISTERED);\r\n",
							entity_handle (hcp->hc_endpoint->ep.fh));*/
				if (!ip->i_nwriters || 
				    (ip->i_nwriters == 1 &&
				     ip->i_writers.w [0] == cp->c_writer)) {
					ip->i_nwriters = 0;
					ip->i_owner = 0;
				}
				else if (hc_rem_writer_gone (ip, cp->c_writer) &&
					 hcp->hc_exclusive &&
					 cp->c_writer == ip->i_owner)
					hc_update_owner (ip);
				if (cp->c_kind == NOT_ALIVE_UNREGISTERED &&
				    ip->i_nwriters) {
					/*log_printf (CACHE_ID, 0, "hc_add(%lu, %u) - done;\r\n",
						entity_handle (hcp->hc_endpoint->ep.fh), cp->c_writer);*/
					hc_change_free (cp);
					return (DDS_RETCODE_OK);
				}
				else if (ip->i_nwriters)
					cp->c_kind &= ~NOT_ALIVE_UNREGISTERED;
			}
			else	/* Update list of writers (accelerate checking
				   typical, i.e. most frequent cases first). */
			     if (ip->i_nwriters == 1) { /* Single writer. */
				if (ip->i_writers.w [0] != cp->c_writer) {
					hc_add_writer_handle (ip, cp->c_writer,
					   (ip->i_writers.w [0] < cp->c_writer) ? 1 : 0);
					new_writer = 1;
				}
			}
			else if (!ip->i_nwriters) { /* First sample. */
				ip->i_nwriters = 1;
				ip->i_writers.w [0] = cp->c_writer;
				ip->i_owner = (hcp->hc_exclusive) ? cp->c_writer : 0;
			}
			else if (ip->i_nwriters <= DNWRITERS) {
				order = hc_lookup_handle (ip->i_writers.w,
						          cp->c_writer,
						          ip->i_nwriters);
				if (order < 0) {
					hc_add_writer_handle (ip, cp->c_writer,
								   -order - 1);
					new_writer = 1;
				}
			}
			else {
				order = hc_lookup_handle (ip->i_writers.ptr->w,
							  cp->c_writer,
							  ip->i_nwriters);
				if (order < 0) {
					hc_add_writer_handle (ip, cp->c_writer,
								   -order - 1);
					new_writer = 1;
				}
			}

			/* If ownership is exlusive and this is not the owner,
			   then ignore the sample. */
			if (hcp->hc_exclusive) {
				if (new_writer)
					hc_update_owner (ip);
				if (ip->i_owner && cp->c_writer != ip->i_owner) {
					/*log_printf (CACHE_ID, 0, "hc_add() - ignore sample;\r\n");*/
					hc_change_free (cp);
					return (DDS_RETCODE_OK);
				}
			}
		}

		/* If the instance is no longer alive, and this is an additional
		   request for non-aliveliness, just update the state of the
		   instance.  If the new change unregisters the instance and
		   when no more samples remain, the instance is cleaned up. */
		if (kind != ALIVE && cp->c_kind != ALIVE &&
				(hcp->hc_endpoint->ep.entity.flags & EF_BUILTIN) == 0) {
			/*log_printf (CACHE_ID, 0, "hc_add(double not alive);\r\n");*/
			ip->i_kind |= cp->c_kind;
			if ((ip->i_kind & NOT_ALIVE_UNREGISTERED) != 0 && 
			    !ip->i_nchanges &&
			    !ip->i_tbf) {
				/*log_printf (CACHE_ID, 0, "hc_add(free instance %u);\r\n", ip->i_handle);*/
				hc_free_instance (hcp, ip->i_handle);
			}
			hc_change_free (cp);
			return (DDS_RETCODE_OK);
		}

		/* If instance has already the maximum # of entries, remove the
		   oldest sample. */
		if ((hcp->hc_ref_type && lp->l_nchanges >= hcp->hc_max_depth) ||
		    (cp->c_kind == ALIVE && ip->i_ndata >= hcp->hc_max_depth)) {
			do {
				/*not_read = hc_not_yet_read (lp->l_head->change);*/
				error = hc_remove_i (hcp, ip, 
						       lp->l_head->change, rel);
			}
			while (!error && ip->i_ndata >= hcp->hc_max_depth);
			if (error /*&& not_read*/ && !hcp->hc_writer)
				dcps_sample_rejected ((Reader_t *) hcp->hc_endpoint,
				     DDS_REJECTED_BY_SAMPLES_PER_INSTANCE_LIMIT, 0);
			if (error)
				return (error);
		}
	}
	else {
		if (hcp->hc_deadlined) {
			hcp->hc_deadlined = 0;
			if (hcp->hc_dlc_idle)
				deadline_continue (hcp->hc_endpoint);
		}
		if ((hcp->hc_ref_type && hcp->hc_nchanges >= hcp->hc_max_depth) ||
		    (cp->c_kind == ALIVE && hcp->hc_ndata >= hcp->hc_max_depth)) {
			do {
				error = hc_remove_i (hcp, ip,
						     hcp->hc_head->change, rel);
			}
			while (!error && hcp->hc_ndata >= hcp->hc_max_depth);
			if (error /*&& not_read*/ && !hcp->hc_writer)
				dcps_sample_rejected ((Reader_t *) hcp->hc_endpoint,
				     DDS_REJECTED_BY_SAMPLES_PER_INSTANCE_LIMIT, 0);
			if (error)
				return (error);
		}
	}

	/* If the number of samples exceeds the cache capacity, remove the
	   oldest sample. */
	if ((hcp->hc_ref_type && hcp->hc_nchanges >= hcp->hc_max_samples) ||
	    (cp->c_kind == ALIVE && hcp->hc_ndata >= hcp->hc_max_samples)) {
		do {
			/*not_read = hc_not_yet_read (hcp->hc_head->change);*/
			error = hc_remove_i (hcp, ip, hcp->hc_head->change, rel);
		}
		while (!error && hcp->hc_nchanges >= hcp->hc_max_samples);
		if (error /*&& not_read*/ && !hcp->hc_writer)
			dcps_sample_rejected ((Reader_t *) hcp->hc_endpoint,
						  DDS_REJECTED_BY_SAMPLES_LIMIT,
									     0);
		if (error)
			return (error);
	}

	/* Update Liveliness. */
	if (hcp->hc_liveliness) {
		if (hcp->hc_endpoint->guard)
			hcp->hc_endpoint->guard->time = cp->c_time;
		hcp->hc_endpoint->ep.entity.flags |= EF_ALIVE;
		if ((hcp->hc_endpoint->ep.entity.flags & EF_LNOTIFY) != 0)
			liveliness_restored (hcp->hc_endpoint, cp->c_writer);
	}

	/* Update sequence # for writer and generation counters for readers. */
	if (hcp->hc_writer) {
		if (!++hcp->hc_last_seqnr.low)
			hcp->hc_last_seqnr.high++;
		cp->c_seqnr = hcp->hc_last_seqnr;
	}
	else {
		cp->c_disp_cnt = *disp_cnt;
		cp->c_no_w_cnt = *no_w_cnt;
		cp->c_sstate = NOT_READ;
	}

	/* Update generation counters. */
	if (cp->c_kind != kind) {
		if ((kind & NOT_ALIVE_DISPOSED) != 0) {
			if (cp->c_kind == ALIVE) {
				(*disp_cnt)++;
				if (hcp->hc_multi_inst)
					ip->i_view = NEW;
				else
					hcp->hc_view = NEW;
			}
			if (hcp->hc_apd_idle)
				autopurge_disposed_continue (hcp->hc_endpoint);
		}
		else if ((kind & NOT_ALIVE_UNREGISTERED) != 0) {
			if (cp->c_kind == ALIVE) {
				(*no_w_cnt)++;
				if (hcp->hc_multi_inst) {
					ip->i_view = NEW;
					ip->i_recover = 0;
				}
				else
					hcp->hc_view = NEW;
			}
			if (hcp->hc_apw_idle)
				autopurge_no_writers_continue (hcp->hc_endpoint);
		}

		if (hcp->hc_multi_inst) {
			ip->i_kind = cp->c_kind;
			if ((cp->c_kind & NOT_ALIVE_UNREGISTERED) != 0)
				ip->i_registered = 0;
		}
		else
			hcp->hc_kind = cp->c_kind;
	}

	/* Finally, add the new sample. */
	error = hc_add_i (hcp, ip, cp);
	return (error);
}

/* hc_add_inst -- Add a change to a cache instance list.  The instance handle
		  must be setup correctly. */

int hc_add_inst (Cache_t cache, Change_t *cp, HCI hci, int rel)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	ChangeKind_t	kind;
	INSTANCE	*ip;
	CCLIST		*lp;
	unsigned	*disp_cnt, *no_w_cnt;
	int		error;

	prof_start (cache_add_inst);

	/* Lookup instance since it should exist. */
	if (hcp->hc_multi_inst) {
		if (hci)
			ip = hci;
		else {
			ip = hc_get_instance_handle (hcp, cp->c_handle);
			if (!ip) {	/* Instance couldn't be added! */
				error = DDS_RETCODE_BAD_PARAMETER;
				goto cleanup;
			}
		}
		kind = ip->i_kind;
		lp = &ip->i_list;
		disp_cnt = &ip->i_disp_cnt;
		no_w_cnt = &ip->i_no_w_cnt;
	}
	else {
		ip = NULL;
		lp = &hcp->hc_changes;
		kind = hcp->hc_kind;
		disp_cnt = &hcp->hc_disp_cnt;
		no_w_cnt = &hcp->hc_no_w_cnt;
	}
	error = hc_add (hcp, ip, lp, cp, kind, disp_cnt, no_w_cnt, rel);
	if (error)
		goto cleanup;

	prof_stop (cache_add_inst, 1);
	return (DDS_RETCODE_OK);

    cleanup:
	TRC_CHANGE (cp, "hc_add_inst", 0);
   	hc_change_free (cp);
	return (error);
}

/* hc_add_key -- Add a sample to the cache. This function uses hash and key
		 fields to specify the instance. */

static int hc_add_key (HistoryCache_t  *hcp,
		       Change_t        *cp,
		       const KeyHash_t *hp,
		       String_t        *key)
{
	INSTANCE	*ip;
	CCLIST		*lp;
	ChangeKind_t	kind;
	RejectCause_t	cause;
	unsigned	*disp_cnt, *no_w_cnt;
	int		error;

	/* Lookup instance since it should exist. */
	if (hcp->hc_multi_inst) {
		ip = hc_get_instance_key (hcp, hp, key, 0, &cp->c_handle,
								 1, 0, &cause);
		if (!ip)	/* Instance couldn't be added! */
			goto cleanup_cause;

		lp = &ip->i_list;
		kind = ip->i_kind;
		disp_cnt = &ip->i_disp_cnt;
		no_w_cnt = &ip->i_no_w_cnt;
	}
	else {
		ip = NULL;
		lp = &hcp->hc_changes;
		kind = hcp->hc_kind;
		disp_cnt = &hcp->hc_disp_cnt;
		no_w_cnt = &hcp->hc_no_w_cnt;
	}
	error = hc_add (hcp, ip, lp, cp, kind, disp_cnt, no_w_cnt, 1);
	if (error)
		goto cleanup;

	return (DDS_RETCODE_OK);

    cleanup_cause:
	error = DDS_RETCODE_OUT_OF_RESOURCES;
	dcps_sample_rejected ((Reader_t *) hcp->hc_endpoint,
			      (DDS_SampleRejectedStatusKind) cause,
			      cp->c_handle);
    cleanup:
	TRC_CHANGE (cp, "hc_add_key", 0);
   	hc_change_free (cp);
	return (error);
}

/* hc_alive -- Indicate that an endpoint is still alive. */

int hc_alive (Cache_t cache)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	CacheRef_t	*rp;

	/* Update local Liveliness. */
	hcp->hc_endpoint->ep.entity.flags |= EF_ALIVE;
	if ((hcp->hc_endpoint->ep.entity.flags & EF_LNOTIFY) != 0)
		liveliness_restored (hcp->hc_endpoint, 0);

	/* Inform local readers of liveliness. */
	if (hcp->hc_writer) {
		for (rp = hcp->hc_readers; rp; rp = rp->next)
			hc_alive (rp->cache);
	
		/* Notify liveliness to remote peer. */
		if (hcp->hc_monitor)
			(*mon_alive_fct) ((uintptr_t) hcp->hc_endpoint);
	}
	return (DDS_RETCODE_OK);
}

/* hc_lookup_key -- Lookup an existing instance in the cache. */

HCI hc_lookup_key (Cache_t             hcp,
		   const unsigned char *key,
		   size_t              keylen,
		   InstanceHandle      *h)
{
	HCI	hci;

	prof_start (cache_lookup_key);
	if (!key) {
		if (h)
			*h = 0;
		return (NULL);
	}
	hci = hc_get_instance_key ((HistoryCache_t *) hcp, NULL,
				   (void *) key, keylen, h, 0, 0, NULL);
	if (!hci && h)
		*h = 0;

	prof_stop (cache_lookup_key, 1);
	return (hci);
}

/* hc_lookup_hash -- Lookup an instance in the cache using a hash key. */

HCI hc_lookup_hash (Cache_t             cache,
		    const KeyHash_t     *hpp,
		    const unsigned char *key,
		    size_t              keylen,
		    InstanceHandle      *h,
		    int                 add,
		    unsigned            ooo,
		    RejectCause_t       *cause)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	HCI		hci;

	prof_start (cache_lookup_hash);
	if (!key) {
		*h = 0;
		return (NULL);
	}
	hci = hc_get_instance_key (hcp, hpp,
				   (void *) key, keylen, h, add, ooo, cause);
	if (!hci && h)
		*h = 0;

	if (!hci && !ooo && add)
		hcp->hc_blocked = 1;

	prof_stop (cache_lookup_hash, 1);
	return (hci);
}

/* hc_get_key -- Get the key data from an instance handle. */

int hc_get_key (Cache_t        cache,
		InstanceHandle h,
		void           *data,
		int            dynamic)
{
	HistoryCache_t		*hcp = (HistoryCache_t *) cache;
	INSTANCE		*ip;
	const unsigned char	*kp;
	int			ret;

	prof_start (cache_get_key);

	if (!hcp->hc_multi_inst)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	ip = hc_get_instance_handle (hcp, h);
	if (!ip)
		return (DDS_RETCODE_ALREADY_DELETED);

	if (!hcp->hc_secure_h &&
			hcp->hc_endpoint->ep.entity_id.id [ENTITY_KIND_INDEX] ==
			(ENTITY_KIND_BUILTIN | ENTITY_KIND_READER_KEY)) {
		memcpy (data, ip->i_hash.hash, hcp->hc_key_size);
		prof_stop (cache_get_key, 1);
		return (DDS_RETCODE_OK);
	}
	if (!ip->i_key)
		kp = ip->i_hash.hash;
	else
		kp = (const unsigned char *) str_ptr (ip->i_key);
	ret = DDS_KeyToNativeData (data, dynamic, hcp->hc_secure_h, kp, 
				hcp->hc_endpoint->ep.topic->type->type_support);

	prof_stop (cache_get_key, 1);
	return (ret);
}

/* hc_key_ptr -- Return a direct pointer to the instance key data. */

const unsigned char *hc_key_ptr (Cache_t cache, HCI hci)
{
	HistoryCache_t		*hcp = (HistoryCache_t *) cache;
	INSTANCE		*ip = (INSTANCE *) hci;
	const unsigned char	*kp;

	if (!hcp->hc_multi_inst || !ip)
		return (NULL);

	kp = (!ip->i_key) ? ip->i_hash.hash : (unsigned char *) str_ptr (ip->i_key);
	return (kp);
}

/* hc_acknowledged -- Signal an acknowled transfer in reliable mode. */

void hc_acknowledged (Cache_t cache, HCI hci, SequenceNumber_t *seqnr)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	INSTANCE	*ip;
	CCLIST		*lp;
	CCREF		*rp;
	Change_t	*cp;
	CacheWait_t	*wp;

	if (hcp->hc_multi_inst) {
		ip = (INSTANCE *) hci;
		if (!ip || !ip->i_nchanges)
			return;

		lp = &ip->i_list;
	}
	else {
		ip = NULL;
		if (!hcp->hc_nchanges)
			return;

		lp = &hcp->hc_changes;
	}
	if (lp->l_nchanges == 1) {
		if (SEQNR_EQ (*seqnr, lp->l_head->change->c_seqnr))
			cp = lp->l_head->change;
		else
			return;
	}
	else if (SEQNR_LT (*seqnr, lp->l_head->change->c_seqnr) ||
		 SEQNR_GT (*seqnr, lp->l_tail->change->c_seqnr))
		return;
	else {
		LIST_FOREACH (lp->l_list, rp)
			if (SEQNR_EQ (*seqnr, rp->change->c_seqnr))
				break;

		if (LIST_END (lp->l_list, rp))
			return;

		cp = rp->change;
	}

	/* Found the change: update the acknowledgement counters. */
	hcp->hc_unacked--;
	/*log_printf (CACHE_ID, 0, "hc_acknowledged(): unacked=%u\r\n", hcp->hc_unacked);*/
	if (--cp->c_wack)
		return;

	if (cp->c_urgent)
		wp = hc_wait_lookup (hcp, cp);
	else
		wp = NULL;

	/* Last connection released the sample. If cache is volatile, remove
	   the sample, since nobody needs it anymore. */
	if (!hcp->hc_durability ||
	    ((cp->c_kind & NOT_ALIVE_UNREGISTERED) != 0 && lp->l_nchanges == 1)) {
		hc_remove_i (hcp, ip, cp, 0);
		if (wp)
			wp->change = NULL;
	}

	/* If the instance was unregistered, clean it up. */
	if (hcp->hc_multi_inst &&
	    (ip->i_kind & NOT_ALIVE_UNREGISTERED) != 0 &&
	    !ip->i_nchanges &&
	    !ip->i_registered &&
	    !ip->i_wait) {
		hc_free_instance (hcp, ip->i_handle);
		if (wp)
			wp->instance = NULL;
	}

	/* If someone was waiting for this sample to be acknowledged, signal
	   the waiter. */
	if (wp) {
		if (wp->nwaiting > 1)
			cond_signal_all (wp->wcond);
		else
			cond_signal (wp->wcond);
	}

	/* If all changes acknowledged and someone's waiting for that event,
	   signal the waiter. */
	if (!hcp->hc_unacked && hcp->hc_blocked) {
		wp = hc_wait_lookup (hcp, NULL);
		if (wp) {
			if (wp->nwaiting > 1)
				cond_signal_all (wp->wcond);
			else
				cond_signal (wp->wcond);
		}
	}
}

/* hc_wait_acks -- Wait for acknowledgements. */

int hc_wait_acks (Cache_t cache, const Duration_t *max_wait)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	Ticks_t		ticks = duration2ticks (max_wait);
	Ticks_t		d, now, end_time;
	CacheWait_t	*wp;
	int		timeout, ret;
	struct timespec	ts;

	if (!hcp->hc_unacked)
		return (DDS_RETCODE_OK);

	if (dds_listener_state || (hcp->hc_unacked && !ticks))
		return (DDS_RETCODE_TIMEOUT);

	now = sys_getticks ();
	end_time = now + ticks;

	wp = hc_wait_add (hcp, NULL, NULL);
	duration2timespec ((DDS_Duration_t *) max_wait, &ts);
	timeout = 0;
	do {
		if (ts.tv_sec || ts.tv_nsec)
			ret = cond_wait_to (wp->wcond, ((Reader_t *) hcp->hc_endpoint)->r_lock, ts);
		else
			ret = cond_wait (wp->wcond, ((Reader_t *) hcp->hc_endpoint)->r_lock);
		d = end_time - now;
		if (ret || !d || d >= 0x7fffffffUL) {
			timeout = 1;
			break;
		}
		if (ts.tv_sec || ts.tv_nsec) {
			ts.tv_sec = d / TICKS_PER_SEC;
			d -= (Ticks_t) (ts.tv_sec * TICKS_PER_SEC);
			ts.tv_nsec = d * TMR_UNIT_MS * 1000000;
		}
		now = sys_getticks ();
	}
	while (hcp->hc_unacked);
	hc_wait_free (wp);
	return ((timeout) ? DDS_RETCODE_TIMEOUT : DDS_RETCODE_OK);
}

/* hc_inst_info -- Returns instance information, i.e. Hash key and the Key
		   fields. */

int hc_inst_info (HCI             hci,
		  const KeyHash_t **hpp,
	          String_t        **spp)
{
	INSTANCE	*ip = (INSTANCE *) hci;

	if (!ip) {
		log_printf (CACHE_ID, 0, "hc_inst_info(): instance not found!\r\n");
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	*hpp = &ip->i_hash;
	if (ip->i_key) {
		*spp = str_ref (ip->i_key);
		/*log_printf (CACHE_ID, 0, "hc_inst_info(): str_ref(%p)\r\n", (void *) ip->i_key);*/
	}
	else
		*spp = NULL;
	return (DDS_RETCODE_OK);
}

/* hc_seqnr_info -- Return the sequence numbers that are stored in the cache. */

int hc_seqnr_info (Cache_t          cache, 
		   SequenceNumber_t *min_snr,
		   SequenceNumber_t *max_snr)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;

	*max_snr = hcp->hc_last_seqnr;
	if (hcp->hc_nchanges)
		*min_snr = hcp->hc_head->change->c_seqnr;
	else {
		*min_snr = hcp->hc_last_seqnr;
		SEQNR_INC (*min_snr);
	}
	return (DDS_RETCODE_OK);
}

/* hc_monitor_fct -- Install callback functions to monitor cache state changes
		      and associated actions. */

int hc_monitor_fct (HCREQIFCT  nfct,
		    HCREQFCT   rfct,
		    HCREQFCT   ufct,
		    HCREQFCT   ubfct,
		    HCALIVEFCT alfct,
		    HCFINSTFCT iffct)
{
	mon_new_fct = nfct;
	mon_remove_fct = rfct;
	mon_urgent_fct = ufct;
	mon_unblock_fct = ubfct;
	mon_alive_fct = alfct;
	mon_iflush_fct = iffct;
	return (DDS_RETCODE_OK);
}

/* hc_monitor_start -- Enable cache monitoring for a given cache and with the
		       specified user parameter. */

int hc_monitor_start (Cache_t hcp, uintptr_t user)
{
	HistoryCache_t	*cp = (HistoryCache_t *) hcp;

	cp->hc_mon_user = user;
	cp->hc_monitor = 1;
	STATS_INC (cp->hc_mon_start);
	return (DDS_RETCODE_OK);
}

/* hc_monitor_end -- Disable cache monitoring for a given cache. */

int hc_monitor_end (Cache_t hcp)
{
	HistoryCache_t	*cp = (HistoryCache_t *) hcp;

	cp->hc_monitor = 0;
	STATS_INC (cp->hc_mon_stop);
	return (DDS_RETCODE_OK);
}

/* hc_inform_start -- Enable cache instance monitoring with the specified user
		      parameter. */

int hc_inform_start (Cache_t hcp, uintptr_t user)
{
	HistoryCache_t	*cp = (HistoryCache_t *) hcp;

	cp->hc_mon_user = user;
	cp->hc_inform = 1;
	return (DDS_RETCODE_OK);
}

/* hc_inst_inform -- Start receiving instance lifetime info for the given
		     cache instance. */

void hc_inst_inform (Cache_t hcp, HCI hci)
{
	HistoryCache_t	*cp = (HistoryCache_t *) hcp;
	INSTANCE 	*ip = (INSTANCE *) hci;

	if (!cp)
		return;

	if (!ip)
		return;

	ip->i_inform = 1;
	
}

/* hc_inf_end_fct -- Visit an instance and clear the inform flag. */

static int hc_inf_end_fct (INSTANCE *ip, void *arg)
{
	ARG_NOT_USED (arg)

	ip->i_inform = 0;
	return (1);
}

/* hc_inform_end -- Disable HCI notifications. */

void hc_inform_end (Cache_t cache)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	INSTANCE	*p;

	if (!hcp || !hcp->hc_multi_inst)
		return;

	hcp->hc_inform = 0;
	if (!hcp->hc_skiplists)
		for (p = hcp->hc_inst_head; p; p = p->i_next)
			p->i_inform = 0;
	else
		hc_walk_instances (hcp, hc_inf_end_fct, NULL);
}

/* hc_request_notification -- Enable cache notifications for a given cache and
			      with the specified user parameter. */

int hc_request_notification (Cache_t hcp, HCRDNOTFCT notify_fct, uintptr_t user)
{
	HistoryCache_t	*cp = (HistoryCache_t *) hcp;

	cp->hc_notify_fct = notify_fct;
	cp->hc_notify_user = user;
	return (DDS_RETCODE_OK);
}

/* hc_write_required -- Check if cache requires samples. */

int hc_write_required (Cache_t cache)
{
	HistoryCache_t		*hcp = (HistoryCache_t *) cache;

	return (hcp->hc_durability || hcp->hc_monitor || hcp->hc_readers);
}

/* hc_unregister -- Unregister an existing instance from the cache. */

int hc_unregister (Cache_t        cache,
		   InstanceHandle h,
		   HCI            hci,
		   const FTime_t  *t,
	           handle_t       dests [],
	           unsigned       ndests)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;

	return (hc_release (cache, h, hci, (hcp->hc_auto_disp) ? 
					ZOMBIE : NOT_ALIVE_UNREGISTERED, 0, t,
								dests, ndests));
}

/* hc_dispose -- Dispose a previously written sample from the cache. */

int hc_dispose (Cache_t        cp,
		InstanceHandle handle,
		HCI            hci,
		const FTime_t  *time,
	        handle_t       dests [],
	        unsigned       ndests)
{
	return (hc_release (cp, handle, hci, NOT_ALIVE_DISPOSED, 0, time,
								dests, ndests));
}

typedef struct reader_get_st {
	HistoryCache_t	*hcp;
	Change_t	**cdata;
	Changes_t	*ccache;
	unsigned	skipmask;
	BCProgram	*filter;
	Strings_t	*pars;
	void		*bc_cache;
	BCProgram	*order;
	unsigned	changes_left;
	unsigned	nwritten;
	unsigned	disp_cnt;
	unsigned	no_w_cnt;
	unsigned	view;
	unsigned	kind;
	InstanceHandle	handle;
	int		next;
	int		rem;
	int		result;
} ReaderGet_t;

/* hc_get_list -- Read/take changes from a changes list. */

static int hc_get_list (CCLIST *lp, INSTANCE *ip, ReaderGet_t *data)
{
	Change_t	*cp;
	CCREF		*refp, *next_refp;
	unsigned	disp_cnt, no_w_cnt, view, kind;
	DBW		dbw;
	ChangeInfo_t	*p;
	int		err, res;
#ifdef EXTRA_STATS
	INST_STATS	*sp;
#endif

	if (ip) {
		disp_cnt = ip->i_disp_cnt;
		no_w_cnt = ip->i_no_w_cnt;
		view = ip->i_view;
		kind = ip->i_kind;
#ifdef EXTRA_STATS
		sp = &ip->i_stats;
#endif
	}
	else {
		disp_cnt = data->disp_cnt;
		no_w_cnt = data->no_w_cnt;
		view = data->view;
		kind = data->kind;
#ifdef EXTRA_STATS
		sp = &data->hcp->hc_stats;
#endif
	}
	for (refp = LIST_HEAD (lp->l_list); refp; refp = next_refp) {

		/* If done: return. */
		if (!data->changes_left)
			return (0);

		/* Already point to next change. */
		next_refp = LIST_NEXT (lp->l_list, *refp);

		/* Get cache change data in {*kind, *data, *length, *h}. */
		cp = refp->change;

		/* Check if change must be skipped due to incorrect mask. */
		if (data->skipmask &&
		    ((!cp->c_sstate && (data->skipmask & SKM_READ) != 0) ||
		     (cp->c_sstate && (data->skipmask & SKM_NOT_READ) != 0) ||
		     (kind == ALIVE && (data->skipmask & SKM_ALIVE) != 0) ||
		     ((kind & NOT_ALIVE_DISPOSED) != 0 && 
		     			(data->skipmask & SKM_DISPOSED) != 0) ||
		     (kind == NOT_ALIVE_UNREGISTERED &&	
		     			(data->skipmask & SKM_NO_WRITERS) != 0) ||
		     (!view && (data->skipmask & SKM_NEW_VIEW) != 0) ||
		     (view && (data->skipmask & SKM_OLD_VIEW) != 0)))
			continue;

		/* Check if change must be skipped due to not matching a query
		   expression. */
		if (data->filter) {
			if (!cp->c_length)
				continue;

			dbw.dbp = cp->c_db;
			dbw.data = cp->c_data;
			dbw.left = cp->c_db->size - ((unsigned char *) cp->c_data - cp->c_db->data);
			dbw.length = cp->c_length;

			err = bc_interpret (data->filter, data->pars, data->bc_cache,
					    &dbw,
					    NULL,
					    1,
					    data->hcp->hc_endpoint->ep.topic->type->type_support,
					    &res);
			if (err || !res)
				continue;
		}

		/* Update change state. */
		cp->c_vstate = (ip) ? ip->i_view : data->hcp->hc_view;
		cp->c_istate = (ip) ? ip->i_kind : data->hcp->hc_kind;
		cp->c_abs_cnt = (disp_cnt + no_w_cnt) -
				(cp->c_disp_cnt + cp->c_no_w_cnt);

		/* Copy change parameters to user arrays if requested. */
		if (data->cdata) {
			rcl_access (cp);
			cp->c_nrefs++;
			rcl_done (cp);
			if (cp->c_nrefs > MAX_CHANGE_REFS)
				fatal_printf ("hc_get_list: maximum # of change clients exceeded!");

			*data->cdata++ = cp;
			cp->c_sstate = READ;
# if 0
			data->cdata->kind = cp->c_kind;
			if (cp->c_length) {
				if (data->cdata->length < cp->c_length ||
				    !data->cdata->data) {
					data->result = DDS_RETCODE_OUT_OF_RESOURCES;
					break;
				}
				dp = cp->c_data;
				if (cp->c_db)
					db_get_data (data->cdata->data,
						     cp->c_db,
						     dp, 0, cp->c_length);
				else
					memcpy (data->cdata->data,
						dp,
						cp->c_length);
			}
			else if (ip && cp->c_kind != ALIVE) {
				hc_get_key (data->hcp, cp->c_handle, data->cdata->data);
				data->cdata->length = data->hcp->hc_key_size;
			}
			else
				data->cdata->length = 0;
			data->cdata->h = cp->c_handle;
			data->cdata->writer = cp->c_writer;
# endif
			/*if (!data->rem && data->hcp->hc_blocked)
				hc_unblock (data->hcp);*/
		}

		/* Copy cache change element pointer to cache list. */
		else if (data->ccache) {
			rcl_access (cp);
			cp->c_nrefs++;
			rcl_done (cp);
			if (cp->c_nrefs > MAX_CHANGE_REFS)
				fatal_printf ("hc_get_list: maximum # of change clients exceeded!");

			if (data->ccache->length == data->ccache->maximum) {
				res = sizeof (ChangeInfo_t) *
						(data->ccache->length + 1);
				if (!data->ccache->maximum)
					p = xmalloc (res);
				else
					p = xrealloc (data->ccache->buffer, res);
				if (!p)
					return (0);

				data->ccache->buffer = p;
				data->ccache->maximum = data->ccache->length + 1;
			}
			data->ccache->buffer [data->ccache->length++].change = cp;
		}
		data->nwritten++;
		data->changes_left--;
		STATS_INC (sp->i_get);

		/* Remove cache change element if user doesn't want it anymore. */
		if (data->rem) {
			hc_change_remove (data->hcp, lp, ip, refp);
			cp->c_cached = 0;
			if (data->hcp->hc_blocked)
				hc_unblock (data->hcp);
			if (!lp->l_nchanges)
				break;
		}
	}
	return (data->changes_left);
}

/* hc_get_fct -- Function called when an instance is visited. */

static int hc_get_fct (INSTANCE *ip, void *arg)
{
	ReaderGet_t	*data = (ReaderGet_t *) arg;
	int		cont;

	if ((data->skipmask && 
	     ((!ip->i_view && (data->skipmask & SKM_NEW_VIEW) != 0) ||
	      (ip->i_view && (data->skipmask & SKM_OLD_VIEW) != 0))) ||
	    (data->handle &&
	     ((!data->next && ip->i_handle != data->handle) ||
	      (data->next && ip->i_handle <= data->handle))))
		return (1);

	if (ip->i_nchanges)
		cont = hc_get_list (&ip->i_list, ip, data);
	else
		cont = 1;
	ip->i_view = NOT_NEW;
	if (data->next && data->nwritten)
		return (0);

	else if (data->handle && !data->next)
		return (0);
	else
		return (cont);
}

static const BCProgram *hc_cmp_program;
static const Strings_t *hc_cmp_pars;
static const TypeSupport_t *hc_cmp_typecode;

/* hc_cmp_entries -- Utility function to determine the match result between two
		     samples. */

static int hc_cmp_entries (const void *p1, const void *p2)
{
	Change_t	**cpp1 = (Change_t **) p1, 
			**cpp2 = (Change_t **) p2;
	DBW		dbw1, dbw2;
	int		err, res;

	dbw1.dbp = (*cpp1)->c_db;
	dbw1.data = (*cpp1)->c_data;
	dbw1.left = (*cpp1)->c_db->size - ((unsigned char *) (*cpp1)->c_data - (*cpp1)->c_db->data);
	dbw1.length = (*cpp1)->c_length;
	dbw2.dbp = (*cpp2)->c_db;
	dbw2.data = (*cpp2)->c_data;
	dbw2.left = (*cpp2)->c_db->size - ((unsigned char *) (*cpp2)->c_data - (*cpp2)->c_db->data);
	dbw2.length = (*cpp2)->c_length;

	err = bc_interpret (hc_cmp_program, hc_cmp_pars, NULL, &dbw1, &dbw2, 1,
			    				hc_cmp_typecode, &res);
	if (err)
		return (-1);

	return (res);
}

/* hc_get -- Get a number of samples from the cache, either for reading or for
	     taking. Note that entries, when taken, should be disposed later on
	     by calling hc_done().  If h == 0 and next == 0 it indicates that 
	     any instance is valid.  If h != 0 and next == 0, it specifies that
	     a sample must be from the given instance.  If next != 0, it speci-
	     fies that a sample should be from the first instance following h
	     that contains data. */

int hc_get (Cache_t        cache,
            unsigned       *nchanges,
	    Changes_t      *entries,
	    unsigned       skipmask,
	    BCProgram      *fp,
	    Strings_t      *pp,
	    void           *bc_cache,
	    BCProgram      *op,
	    InstanceHandle h,
	    int            next,
	    int            rem)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	ReaderGet_t	get_data;
	INSTANCE	*ip;
	static lock_t	sort_lock;
	static int	sort_lock_ready = 0;

	prof_start (cache_get);

	get_data.hcp = hcp;
	get_data.cdata = NULL;
	get_data.ccache = entries;
	get_data.skipmask = skipmask;
	get_data.changes_left = *nchanges;
	get_data.nwritten = 0;
	get_data.disp_cnt = hcp->hc_disp_cnt;
	get_data.no_w_cnt = hcp->hc_no_w_cnt;
	get_data.view = hcp->hc_view;
	get_data.kind = hcp->hc_kind;
	get_data.filter = fp;
	get_data.pars = pp;
	get_data.bc_cache = bc_cache;
	get_data.order = op;
	get_data.handle = h;
	get_data.next = next;
	get_data.rem = rem;
	get_data.result = DDS_RETCODE_OK;
	if (hcp->hc_multi_inst && (hcp->hc_inst_order || h)) {
		if (h && !next && hcp->hc_skiplists) {
			ip = hc_get_instance_handle (hcp, h);
			if (ip)
				hc_get_list (&ip->i_list, ip, &get_data);
		}
		else
			hc_walk_instances (hcp, hc_get_fct, &get_data);
	}
	else if (hcp->hc_nchanges)
		hc_get_list (&hcp->hc_changes, NULL, &get_data);
	*nchanges = get_data.nwritten;
	if (op && op->length) {
		if (!sort_lock_ready) {
			lock_init_nr (sort_lock, "hc_sort");
			sort_lock_ready = 1;
		}
		lock_take (sort_lock);
		hc_cmp_program = op;
		hc_cmp_pars = pp;
		hc_cmp_typecode = hcp->hc_endpoint->ep.topic->type->type_support;
		qsort (entries->buffer, *nchanges, sizeof (Change_t *), hc_cmp_entries);
		lock_release (sort_lock);
	}
	prof_stop (cache_get, (op && op->length) ? op->length : 1);
	return (get_data.result);
}

/* hc_get_data -- Get a number of samples from the cache, either for reading or
		  for taking.  Note that entries, when taken, should be disposed
		  later on by calling hc_done().  The meaning of h and next is
		  the same as in hc_get(). */

int hc_get_data (Cache_t        cache,
                 unsigned       *nchanges,
	         Change_t       **changes,
		 unsigned	skipmask,
		 InstanceHandle h,
	         int            next,
	         int            rem)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	ReaderGet_t	get_data;
	INSTANCE	*ip;

	prof_start (cache_get_data);

	get_data.hcp = hcp;
	get_data.cdata = changes;
	get_data.ccache = NULL;
	get_data.skipmask = skipmask;
	get_data.changes_left = *nchanges;
	get_data.nwritten = 0;
	get_data.disp_cnt = hcp->hc_disp_cnt;
	get_data.no_w_cnt = hcp->hc_no_w_cnt;
	get_data.view = hcp->hc_view;
	get_data.kind = hcp->hc_kind;
	get_data.filter = NULL;
	get_data.pars = NULL;
	get_data.order = NULL;
	get_data.handle = h;
	get_data.next = next;
	get_data.rem = rem;
	get_data.result = DDS_RETCODE_OK;
	if (hcp->hc_multi_inst && (hcp->hc_inst_order || h))
		if (h && !next && hcp->hc_skiplists) {
			ip = hc_get_instance_handle (hcp, h);
			if (ip)
				hc_get_list (&ip->i_list, ip, &get_data);
		}
		else
			hc_walk_instances (hcp, hc_get_fct, &get_data);
	else if (hcp->hc_nchanges)
		hc_get_list (&hcp->hc_changes, NULL, &get_data);
	*nchanges = get_data.nwritten;

	prof_stop (cache_get_data, get_data.nwritten ? get_data.nwritten : 1);
	return (get_data.result);
}

/* free_builtin_handle -- free the handles for entities in builtin readers when
                          the instance is free-ed/becomes recoverable. */

static INLINE void free_builtin_handle (InstanceHandle handle)
{
	Entity_t * ep = entity_ptr (handle);

	if (!ep)
		handle_done (handle);
	else
		ep->flags &= ~EF_CACHED;
}

/* hc_done -- The cache entries are processed completely and can finally be
	      disposed. */

int hc_done (Cache_t      cache,
             unsigned     nchanges,
	     ChangeInfo_t changes [])
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	Change_t	*cp;
	INSTANCE	*ip;
	unsigned	i;

	ARG_NOT_USED (cache)

	prof_start (cache_done);

	for (i = 0; i < nchanges; i++) {
		cp = changes [i].change;
		changes [i].change = NULL;
		TRC_CHANGE (cp, "hc_done", 0);
		if (cp->c_cached) {
			cp->c_sstate = READ;
			/*if (hcp->hc_blocked) <- only when taken!
				hc_unblock (hcp);*/
		}
		else if (hcp->hc_multi_inst &&
			 (cp->c_kind & NOT_ALIVE_UNREGISTERED) != 0) {
			ip = hc_get_instance_handle (hcp, cp->c_handle);
			if (ip &&
			    !ip->i_nchanges &&
			    ip->i_time == cp->c_time &&
			    !ip->i_registered) {
				if ((hcp->hc_endpoint->ep.entity.flags & EF_BUILTIN) != 0)
					free_builtin_handle (cp->c_handle);

				if (hcp->hc_max_inst < MAX_INST_HANDLE)
					ip->i_recover = 1;
#ifdef RX_INST_CLEANUP
				else
					hc_free_instance (hcp, ip->i_handle);
#endif
			}
		}
		hc_change_free (cp);
	}
	prof_stop (cache_done, nchanges);
	return (DDS_RETCODE_OK);
}

typedef struct reader_check_st {
	HistoryCache_t	*hcp;
	unsigned	skipmask;
	unsigned	view;
	BCProgram	*query;
	Strings_t	*pars;
	void		*bc_cache;
	int		match;
} ReaderCheck_t;

/* hc_check_list -- Check whether the samples in the list have a status that
		    matches the mask. */

static int hc_check_list (CCLIST *lp, INSTANCE *ip, ReaderCheck_t *data)
{
	Change_t	*cp;
	DBW		dbw;
	CCREF		*refp, *next_refp;
	unsigned	view;
	int		err, res;

	if (ip)
		view = ip->i_view;
	else
		view = data->view;
	for (refp = LIST_HEAD (lp->l_list); refp; refp = next_refp) {

		/* Already point to next change. */
		next_refp = LIST_NEXT (lp->l_list, *refp);
		cp = refp->change;

		/* Check if change matches. */
		if (data->skipmask &&
		    ((!cp->c_sstate && (data->skipmask & SKM_READ) != 0) ||
		     (cp->c_sstate && (data->skipmask & SKM_NOT_READ) != 0) ||
		     (cp->c_kind == ALIVE && (data->skipmask & SKM_ALIVE) != 0) ||
		     ((cp->c_kind & NOT_ALIVE_DISPOSED) != 0 &&
		    			(data->skipmask & SKM_DISPOSED) != 0) ||
		     ((cp->c_kind & NOT_ALIVE_UNREGISTERED) != 0 &&
		    			(data->skipmask & SKM_NO_WRITERS) != 0) ||
		     (!view && (data->skipmask & SKM_NEW_VIEW) != 0) ||
		     (view && (data->skipmask & SKM_OLD_VIEW) != 0)))
			continue;

		if (data->query) {
			dbw.dbp = cp->c_db;
			dbw.data = cp->c_data;
			dbw.left = cp->c_db->size - ((unsigned char *) cp->c_data - cp->c_db->data);
			dbw.length = cp->c_length;

			err = bc_interpret (data->query,
					    data->pars,
					    data->bc_cache,
					    &dbw,
					    NULL,
					    1,
					    data->hcp->hc_endpoint->ep.topic->type->type_support,
					    &res);
			if (err)
				data->match = 0;
			else
				data->match = res;
		}
		else
			data->match = 1;
		return (0);
	}
	return (1);
}
		    
/* hc_check_fct -- Function called when an instance is visited. */

static int hc_check_fct (INSTANCE *ip, void *arg)
{
	ReaderCheck_t	*data = (ReaderCheck_t *) arg;

	if (data->skipmask &&
	    ((!ip->i_view && (data->skipmask & SKM_NEW_VIEW) != 0) ||
	     (ip->i_view && (data->skipmask & SKM_OLD_VIEW) != 0)))
		return (1);

	if (!ip->i_nchanges)
		return (1);

	return (hc_check_list (&ip->i_list, ip, data));
}

/* hc_avail -- Return a non-0 result if there are samples in the cache not
	       matching the skip mask. */

int hc_avail (Cache_t cache, unsigned skipmask)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	ReaderCheck_t	check_data;

	prof_start (cache_avail);

	check_data.hcp = hcp;
	check_data.skipmask = skipmask;
	check_data.view = hcp->hc_view;
	check_data.query = NULL;
	check_data.pars = NULL;
	check_data.bc_cache = NULL;
	check_data.match = 0;
	if (hcp->hc_multi_inst)
		hc_walk_instances (hcp, hc_check_fct, &check_data);
	else if (hcp->hc_nchanges)
		hc_check_list (&hcp->hc_changes, NULL, &check_data);

	prof_stop (cache_avail, 1);
	return (check_data.match);
}

/* hc_avail_condition -- Return a non-0 result if there are samples in the cache
			 matching the status mask and the query filter. */

int hc_avail_condition (Cache_t   cache,
			unsigned  skipmask,
			Strings_t *pars,
			BCProgram *query,
			void      *bc_cache)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	ReaderCheck_t	check_data;

	check_data.hcp = hcp;
	check_data.skipmask = skipmask;
	check_data.view = hcp->hc_view;
	check_data.query = query;
	check_data.pars = pars;
	check_data.bc_cache = bc_cache;
	check_data.match = 0;
	if (hcp->hc_multi_inst)
		hc_walk_instances (hcp, hc_check_fct, &check_data);
	else if (hcp->hc_nchanges)
		hc_check_list (&hcp->hc_changes, NULL, &check_data);
	return (check_data.match);
}

/* hc_replay -- Replay all cache entries to a new reader. */

int hc_replay (Cache_t cache, HCREQIFCT fct, uintptr_t user)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	CCREF		*rp;
	INSTANCE	*ip;
	unsigned	wack;
	int		again;

	if (!hcp->hc_nchanges)
		return (DDS_RETCODE_OK);

	LIST_FOREACH (hcp->hc_list, rp) {
		wack = rp->change->c_wack;
		ip = (hcp->hc_multi_inst) ? 
			hc_get_instance_handle (hcp, rp->change->c_handle) : 
			NULL;
		again = (*fct) (user, rp->change, ip);
		STATS_INC (hcp->hc_mon_replay);
		if (rp->change->c_wack != wack) {

			/* Update global unacked changes counter! */
			hcp->hc_unacked += rp->change->c_wack;
			hcp->hc_unacked -= wack;
		}
		/*log_printf (CACHE_ID, 0, "hc_replay(): unacked=%u\r\n", hcp->hc_unacked);*/
		if (!again)
			break;
	}
	return (DDS_RETCODE_OK);
}

void hc_transfer_samples (void)
{
	CXFER		*np;
	CacheXfers_t	*xp;
	int		error;

	lock_take (xfers_lock);
	xp = xfers_ready.head;
	if (!xp) {
		lock_release (xfers_lock);
		return;
	}
	xfers_ready.head = xp->ready;
	xp->state = XS_WAITING;
	for (np = LIST_HEAD (xp->list); np; np = LIST_HEAD (xp->list)) {
		if (xp->cache->hc_multi_inst)
			error = hc_add_key (xp->cache, np->change,
							&np->hash, np->key);
		else
			error = hc_add_inst (xp->cache, np->change, NULL, 1);
		if (error == DDS_RETCODE_NO_DATA)
			break;

		LIST_REMOVE (xp->list, *np);
		if (np->scache)
			hc_acknowledged (np->scache, np->shci, &np->seqnr);
		mds_pool_free (&mem_blocks [MB_CXFER], np);
	}
	if (LIST_EMPTY (xp->list)) {
		LIST_REMOVE (xfers_list, *xp);
		xp->state = XS_IDLE;
		mds_pool_free (&mem_blocks [MB_XFLIST], xp);
	}
	lock_release (xfers_lock);
	if (xfers_ready.head)
		dds_signal (DDS_EV_CACHE_X);
}

typedef struct cache_action_check_st {
	HistoryCache_t	*hcp;
	GuardType_t	type;
	handle_t	writer;
	unsigned	nalive;
	FTime_t		now;
	FTime_t		next;
	FTime_t		period;
} CacheActCheck_t;

/*#define ACT_XQOS_TRACE	** Trace extra QoS info. */

#ifdef ACT_XQOS_TRACE
#define	act_printf(s)		dbg_printf(s)
#define	act_print1(s,a1)	dbg_printf(s,a1)
#define	act_print2(s,a1,a2)	dbg_printf(s,a1,a2)
#define act_printtime(t)	dbg_print_time(t)
#else
#define	act_printf(s)
#define	act_print1(s,a1)
#define	act_print2(s,a1,a2)
#define act_printtime(t)
#endif

/* hc_reader_lifespan -- Set the status of a reader for lifespan checks. */

void hc_reader_lifespan (Cache_t cache, int enable)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;

	hcp->hc_lifespan = enable;
	act_print1 ("<reader lifespan = %d>\r\n", enable);
}

/* hc_inst_check_deadline -- Check if an instance has deadlined. */

static int hc_inst_check_deadline (CacheActCheck_t *cp, INSTANCE *ip)
{
	FTime_t		ntime;

	act_print2 ("..<hc_inst_check_deadline(%p,%p)>\r\n", (void *) cp->hcp, (void *) ip);
	if (ip->i_deadlined)
		return (0);

	if (!ip->i_nchanges)
		ntime = ip->i_time;
	else
		ntime = ip->i_tail->change->c_time;
	FTIME_ADD (ntime, cp->period);
	if (FTIME_LT (ntime, cp->now)) { /* Deadline detected! */
		ip->i_deadlined = 1;
		if (cp->hcp->hc_writer)
			dcps_offered_deadline_missed ((Writer_t *) cp->hcp->hc_endpoint, ip->i_handle);
		else
			dcps_requested_deadline_missed ((Reader_t *) cp->hcp->hc_endpoint, ip->i_handle);
		return (0);
	}
	if (FTIME_ZERO (cp->next) || FTIME_LT (ntime, cp->next))
		cp->next = ntime;
	return (1);
}

/* hc_inst_check_lifespan -- Check if an instance has samples with lifespan
			     exceeded. */

static int hc_inst_check_lifespan (CacheActCheck_t *cp, INSTANCE *ip)
{
	FTime_t		ntime;
	CCREF		*rp, *next_rp;
	CCREF		*crp, *irp;

	act_print2 ("..<hc_inst_check_lifespan(%p,%p)>\r\n", (void *) cp->hcp, (void *) ip);
	if (!ip->i_nchanges || (!cp->hcp->hc_writer && !ip->i_nwriters))
		return (0);

	if (!cp->hcp->hc_writer &&
	    hc_lookup_handle (WLIST_HANDLES (ip),
			      cp->writer,
			      ip->i_nwriters) < 0)
		return (0);

	for (rp = ip->i_head; rp; rp = next_rp) {
		next_rp = LIST_NEXT (ip->i_list, *rp);
		if (!cp->hcp->hc_writer && rp->change->c_writer != cp->writer)
			continue;

		/* Calculate lifespan exceeded time of sample. */
		ntime = rp->change->c_time;
		FTIME_ADD (ntime, cp->period);
		if (FTIME_GT (ntime, cp->now)) { /* No time-out on instance. */

			/* Update next scheduling time. */
			if (FTIME_ZERO (cp->next) || FTIME_LT (ntime, cp->next))
				cp->next = ntime;
			return (1);
		}

		/* Lifespan exceeded of sample - check if it can be removed. */
		act_print1 ("..<lifespan:remove(%p)>\r\n", (void *) rp->change);
		if (cp->hcp->hc_must_ack && rp->change->c_wack) {

			/* Can't remove sample, since underway on RTPS! */
			cp->next = cp->now; /* Recheck as fast as possible! */
			return (1);
		}

		/* We are allowed to remove the sample. Notify lower-layer if it
		   is still using the change. */
		if (!cp->hcp->hc_must_ack && rp->change->c_wack) {
			cp->hcp->hc_unacked -= rp->change->c_wack;
			if (cp->hcp->hc_monitor) {
				(*mon_remove_fct) (cp->hcp->hc_mon_user,
						   rp->change);
				STATS_INC (ip->i_stats.m_remove);
			}
		}

		/* Remove the change. */
		irp = ccref_remove_change (&ip->i_list, rp->change);
		if (!irp)
			return (0);

		if (rp->change->c_nrefs < 2)
			fatal_printf ("hc_inst_check_lifespan: invalid change!");

		crp = irp->mirror;
		ccref_delete (irp);
		ccref_remove_ref (&cp->hcp->hc_changes, crp);
		if (crp->change)
			crp->change->c_cached = 0;
		
		/* Finally delete the change reference. */
		ccref_delete (crp);

		/* If this was the last change on the instance, free instance. */
		if (!ip->i_nchanges &&
		    cp->hcp->hc_writer &&
		    (ip->i_kind & NOT_ALIVE_UNREGISTERED) != 0) {
			act_print1 ("..<lifespan:remove-instance(%p)>\r\n", (void *) ip);
			hc_free_instance (cp->hcp, ip->i_handle);
			return (0);
		}
	}
	return (0);
}

static void hc_inst_purge (HistoryCache_t *hcp, INSTANCE *ip)
{
	act_print1 ("..<purge:remove-instance(%p)>\r\n", (void *) ip);
	while (ip->i_nchanges)
		hc_remove_i (hcp, ip, ip->i_head->change, 0);

	hc_free_instance (hcp, ip->i_handle);
}

static int hc_inst_check_autopurge_no_writer (CacheActCheck_t *cp, INSTANCE *ip)
{
	FTime_t		ntime;

	act_print2 ("..<hc_inst_check_autopurge_nw(%p,%p)", (void *) cp->hcp, (void *) ip);
	if ((ip->i_kind & NOT_ALIVE_UNREGISTERED) != 0) {
		if (!ip->i_nchanges)
			ntime = ip->i_time;
		else
			ntime = ip->i_tail->change->c_time;
		act_printf (": ");
		act_printtime (&ntime);
		act_printf (">\r\n");
		FTIME_ADD (ntime, cp->period);
		if (FTIME_GT (ntime, cp->now)) { /* No time-out on instance. */

			/* Update next scheduling time. */
			if (FTIME_ZERO (cp->next) || FTIME_LT (ntime, cp->next))
				cp->next = ntime;
			return (1);
		}
		hc_inst_purge (cp->hcp, ip);
		return (0);
	}
	act_printf (">\r\n");
	return (0);
}

static int hc_inst_check_autopurge_disposed (CacheActCheck_t *cp, INSTANCE *ip)
{
	FTime_t		ntime;

	act_print2 ("..<hc_inst_check_autopurge_d(%p,%p)", (void *) cp->hcp, (void *) ip);
	if ((ip->i_kind & NOT_ALIVE_DISPOSED) != 0) {
		if (!ip->i_nchanges)
			ntime = ip->i_time;
		else
			ntime = ip->i_tail->change->c_time;
		act_printf (": ");
		act_printtime (&ntime);
		act_printf (">\r\n");
		FTIME_ADD (ntime, cp->period);
		if (FTIME_GT (ntime, cp->now)) { /* No time-out on instance. */

			/* Update next scheduling time. */
			if (FTIME_ZERO (cp->next) || FTIME_LT (ntime, cp->next))
				cp->next = ntime;
			return (1);
		}
		hc_inst_purge (cp->hcp, ip);
		return (0);
	}
	act_printf (">\r\n");
	return (0);
}

static int hc_inst_qos_action (INSTANCE *ip, void *arg)
{
	CacheActCheck_t	*cp = (CacheActCheck_t *) arg;

	act_print2 (".<hc_inst_action(%p,%p)>\r\n", (void *) cp->hcp, (void *) ip);
	switch (cp->type) {
		case GT_DEADLINE:
			if (cp->hcp->hc_deadline &&
			    hc_inst_check_deadline (cp, ip))
				cp->nalive++;
			break;
		case GT_LIFESPAN:
			if (hc_inst_check_lifespan (cp, ip))
				cp->nalive++;
			break;
		case GT_AUTOP_NW:
			if (cp->hcp->hc_purge_nw &&
			    hc_inst_check_autopurge_no_writer (cp, ip))
				cp->nalive++;
			break;
		case GT_AUTOP_DISP:
			if (cp->hcp->hc_purge_dis &&
			    hc_inst_check_autopurge_disposed (cp, ip))
				cp->nalive++;
			break;
		default:
			break;
	}
	return (1);
}

/* hc_handle_xqos -- Called periodically to handle one of the special QoS
		     parameters (Deadline/Lifespan/Reader-Data-lifecycle).
		     The # of timer ticks for rechecks is returned if !=0. */

Ticks_t hc_handle_xqos (Cache_t cache, int type, handle_t w, Ticks_t period)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	Ticks_t		ticks;
	CacheActCheck_t	check;

	act_print2 ("<handle_xqos:%d,%u - ", type, w);
	check.hcp = cache;
	check.type = type;
	check.writer = w;
	sys_getftime (&check.now);
	act_printtime (&check.now);
	act_printf (">\r\n");
	FTIME_SETT (check.period, period);
	if (!hcp->hc_multi_inst) {
		if (type != GT_DEADLINE || !hcp->hc_deadline)
			return (0);

		if (hcp->hc_deadlined)
			return (0);

		if (!hcp->hc_nchanges)
			check.next = hcp->hc_time;
		else
			check.next = hcp->hc_tail->change->c_time;
		FTIME_ADD (check.next, period);
		if (FTIME_LT (check.next, check.now)) { /* Deadline detected! */
			hcp->hc_deadlined = 1;
			if (hcp->hc_writer)
				dcps_offered_deadline_missed ((Writer_t *) hcp->hc_endpoint, 0);
			else
				dcps_requested_deadline_missed ((Reader_t *) hcp->hc_endpoint, 0);
			hcp->hc_dlc_idle = 1;
			return (0);
		}
	}
	else {
		FTIME_CLR (check.next);
		check.nalive = 0;
		hc_walk_instances (hcp, hc_inst_qos_action, &check);
		if (!check.nalive) {
			switch (type) {
				case GT_DEADLINE:
					hcp->hc_dlc_idle = 1;
					break;
				case GT_LIFESPAN:
					hcp->hc_lsc_idle = 1;
					break;
				case GT_AUTOP_NW:
					hcp->hc_apw_idle = 1;
					break;
				case GT_AUTOP_DISP:
					hcp->hc_apd_idle = 1;
					break;
				default:
					break;
			}
			act_printf ("<handle_actions:idle>\r\n");
			return (0);
		}
	}
	FTIME_SUB (check.next, check.now);
	ticks = FTIME_TICKS (check.next);
	if (ticks < MIN_XQOS_DELAY)
		ticks = MIN_XQOS_DELAY;

	act_print2 ("<handle_actions:#=%u,%lu ticks>\r\n", check.nalive, ticks);
	return (ticks);
}

/* hc_tbf_new -- Create a new Time-based filter context for the given writer, 
		 with suitable completion functions and the min. duration
		 between samples of the same instance. */

TBF hc_tbf_new (Cache_t    cache,
		void       *proxy,
		TBFSENDFCT send_fct,
		TBFDONEFCT done_fct,
		Duration_t *delay)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	TFilter_t	*fp;

	if (!hcp)
		return (NULL);

	if ((fp = mds_pool_alloc (&mem_blocks [MB_FILTER])) == NULL) {
		warn_printf ("tbf_new: out of memory for filter context!\r\n");
		return (NULL);
	}
	LIST_INIT (*fp);
	fp->cache = hcp;
	fp->nusers = 1;
	fp->proxy = proxy;
	FTIME_SET (fp->delay, delay->secs, delay->nanos);
	fp->send_fct = send_fct;
	fp->done_fct = done_fct;
	tmr_init (&fp->timer, "TimeBasedFilter");

	/* Add filter to cache. */
	fp->next = hcp->hc_filters;
	hcp->hc_filters = fp;
	return (fp);
}

/* check_inst_idle -- Check whether a cache instance can be safely deleted. */

static void check_inst_idle (Cache_t cache, INSTANCE *ip)
{
	/* Check if instance can be freed. */
	if (ip &&
	    (ip->i_kind & NOT_ALIVE_UNREGISTERED) != 0 &&
	    !ip->i_nchanges &&
	    !ip->i_tbf)

		/*log_printf (CACHE_ID, 0, "hc_add(free instance %u);\r\n", ip->i_handle);*/
		hc_free_instance (cache, ip->i_handle);
}

/* hc_tbf_free -- Free a Time-based filter context and cleanup all pending
		  changes/instances. */

void hc_tbf_free (Cache_t cache, void *proxy)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	TFilter_t	*prev_fp, *fp;
	TFNode_t	*p;
	INSTANCE	*ip;

	for (prev_fp = NULL, fp = hcp->hc_filters;
	     fp;
	     prev_fp = fp, fp = fp->next)
		if (fp->proxy == proxy)
			break;

	if (!fp)
		return;

	if (prev_fp)
		prev_fp->next = fp->next;
	else
		hcp->hc_filters = fp->next;
	while ((p = LIST_HEAD (*fp)) != NULL) {
		ip = p->instance;
		hc_tbf_inst_cleanup (p);
		if (ip)
			check_inst_idle (hcp, ip);
	}
	mds_pool_free (&mem_blocks [MB_FILTER], fp);
}

/* hc_tbf_timeout -- Timeout handling of a time-based filtering proxy. */

static void hc_tbf_timeout (uintptr_t user)
{
	TFilter_t	*fp = (TFilter_t *) user;
	TFNode_t	*p;
	FTime_t		now, next;
	void		*sp;
	Ticks_t		t;
	INSTANCE	*ip;
	HCI		hci;
	int		rel;

	sys_getftime (&now);
	while ((p = LIST_HEAD (*fp)) != NULL &&
	       FTIME_GT (now, p->tx_time)) {
		if (!p->sample) {
			ip = p->instance;
			hc_tbf_inst_cleanup (p);
			if (ip)
				check_inst_idle (fp->cache, ip); 
		}
		else {
			LIST_REMOVE (*p->filter, *p);
			sp = p->sample;
			hci = p->instance;
			rel = p->rel;
			p->sample = NULL;
			FTIME_ADD (p->tx_time, fp->delay);
			LIST_ADD_TAIL (*fp, *p);
			(*fp->send_fct) (fp->proxy, sp, hci, rel);
		}
	}
	if (p) {
		/* Get duration for next periodic check. */
		next = p->tx_time;
		FTIME_SUB (next, now);

		/* Convert duration to ticks. */
		t = FTIME_TICKS (next);
		if (!t)
			t = 1;

		/* Restart timer. */
		tmr_start_lock (&fp->timer,
				t,
				(uintptr_t) fp,
				hc_tbf_timeout,
				&((Writer_t *) fp->cache->hc_endpoint)->w_lock);
	}
}

/* hc_tbf_add -- Check if we may enqueue a sample to a destination proxy.  If
	         allowed, a non-zero result is returned, and the change can be
	         enqueud normally.  If not, the change will be queued
		 automatically for future transmission (after a suitable time-
		 out). */

int hc_tbf_add (TBF tbf, HCI hci, const FTime_t *t, void *sample, int rel)
{
	TFilter_t	*fp = (TFilter_t *) tbf;
	INSTANCE	*ip = (INSTANCE *) hci;
	TFNode_t	*p;

	if (!fp)
		return (1);	/* Can't filter -> no filter installed. */

	for (p = (ip) ? ip->i_tbf : fp->cache->hc_tbf; p; p = p->i_next)
		if (p->filter == fp)
			break;

	if (p && p->sample) {

		/* Instance exists and already contains a sample: remove the
		   pending sample and replace it with the new one. */
		(*fp->done_fct) (fp, p->sample);
		p->sample = sample;
		p->rel = rel;
		return (0);	/* Sample is pending, don't send. */
	}
	else if (p) {

		/* Nothing pending, but instance still not immediately usable.
		   Enqueue sample to instance record. */
		p->sample = sample;
		p->rel = rel;
		return (0);	/* Sample is pending, don't send. */
	}

	/* First time we send on this instance: add new context. */
	p = mds_pool_alloc (&mem_blocks [MB_FINST]);
	if (!p)
		return (1);	/* Can't filter: no memory. */

	p->filter = fp;
	p->instance = ip;

	/* Calculate time for next earliest transmission. */
	p->tx_time = *t;
	FTIME_ADD (p->tx_time, fp->delay);

	p->sample = NULL;

	/* Add to list of instances. */
	if (ip) {
		p->i_next = ip->i_tbf;
		ip->i_tbf = p;
	}
	else {
		p->i_next = fp->cache->hc_tbf;
		fp->cache->hc_tbf = p;
	}

	/* Add to filter node list.

	   Note: following is a bit simplistic since the change time
	         might be completely off, due to the use of application-
		 generated timestamps (*_w_timestamp() functions).
		 We should really do a sorted add to be completely
		 correct.  In the short term, the current approach
		 will typically be perfectly ok.  Even when the time
		 is completely off, we are still doing the logical
		 thing and will cope with it in time-out handling.
	 */
	LIST_ADD_TAIL (*fp, *p);

	/* If first node, start timer. */
	if (LIST_SINGLE (*p))
		tmr_start_lock (&fp->timer,
				FTIME_TICKS (fp->delay),
				(uintptr_t) fp,
				hc_tbf_timeout,
				&((Writer_t *) fp->cache->hc_endpoint)->w_lock);

	return (1);	/* We got a context: this sample can be sent,
			   but the next sample will have to wait until
			   enough time has passed. */
}

/* hc_add_received -- Same as hc_add_inst, but should be used for reader caches
		      that have an active time-based filter. */

int hc_add_received (Cache_t cp, Change_t *change, HCI hci, int rel)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cp;

	if (!hcp->hc_tfilter)
		return (hc_add_inst (cp, change, hci, rel));

	if (!hc_tbf_add (hcp->hc_filters, hci, &change->c_time, change, rel))
		return (0);

	return (hc_add_inst (cp, change, hci, rel));
}

#ifdef CACHE_CHECK

static const char *check_name;

static void hc_verify_change (Change_t *cp)
{
	if (mds_pool_contains (&mem_blocks [MB_CHANGE], cp))
		fatal_printf ("hc_check: CacheChange already released (%s)!", check_name);

	if (cp->c_nrefs == 0 || cp->c_nrefs > MAX_CHANGE_REFS || cp->c_wack > 0x7ff)
		fatal_printf ("hc_check: cache is corrupt (%s)!", check_name);
}

static void hc_verify_ref (CCREF *rp)
{
	if (mds_pool_contains (&mem_blocks [MB_CCREF], rp))
		fatal_printf ("hc_check: Reference already released (%s)!", check_name);
	if (rp->change)
		hc_verify_change (rp->change);
}

static void hc_verify_cclist (CCLIST *lp)
{
	CCREF		*rp;
	unsigned	n = 0;

	if (lp->head) {
		if (lp->head->prev || lp->tail->next)
			fatal_printf ("hc_check: ref-list is corrupt-a (%s)!", check_name);
		LIST_FOREACH (lp->l_list, rp) {
			hc_verify_ref (rp);
			n++;
			if (!rp->next && lp->tail != rp)
				fatal_printf ("hc_check: ref-list is corrupt-b (%s)!", check_name);
		}
	}
	if (n != lp->nchanges)
		fatal_printf ("hc_check: ref-list is corrupt-c (%s)!", check_name);
}

static void hc_verify_instance (INSTANCE *ip)
{
	if (mds_pool_contains (&mem_blocks [MB_INSTANCE], ip))
		fatal_printf ("hc_check: Instance already released (%s)!", check_name);
	hc_verify_cclist (&ip->i_list);
}

/* hc_verify_sl_inst -- Verify a skiplist node containing an instance. */

static int hc_verify_sl_inst (Skiplist_t *list, void *node, void *arg)
{
	INSTANCE	*ip, **ipp = (INSTANCE **) node;

	ARG_NOT_USED (list)

	ip = *ipp;
	hc_verify_instance (ip);
	if (arg)
		while (ip->next) {
			ip = ip->next;
			hc_verify_instance (ip);
		}
	return (1);
}

/* hc_check -- Validate the cache contents of an endpoint. */

void hc_check (Cache_t cache)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	INSTANCE	*ip;
	Change_t	*cp;

	if (hcp->hc_nchanges) {
		LIST_FOREACH (hcp->hc_list, cp) {
			hc_verify_change (cp);
			if (!cp->c_next && hcp->c_tail != cp)
				fatal_printf ("hc_check: primary list is corrupt-a (%s)!", check_name);
		}
		if (hcp->head->prev || hcp->tail->next)
			fatal_printf ("hc_check: primary list is corrupt-b (%s)!", check_name);
	}
	if (!hcp->hc_multi_inst)
		hc_verify_cclist (&hcp->hc_changes);
	else {
		if (!hcp->hc_skiplists)
			for (ip = hcp->hc_inst_head; ip; ip = ip->i_next)
				hc_verify_instance (ip);
		else {
			sl_walk (hcp->hc_hashes, hc_verify_sl_inst, (void *) 1LU);
			sl_walk (hcp->hc_handles, hc_verify_sl_inst, NULL);
		}
	}
}

#endif
#ifdef DDS_DEBUG

static const char *kind_str [] = { "ALIVE", "DISPOSED", "UNREGISTERED", "ZOMBIE" };

static void hc_dump_change (Change_t *cp, int newline, int reader_bits)
{
	if (newline)
		dbg_printf ("\t");
	else
		dbg_printf (" ");
	dbg_print_time (&cp->c_time);
	dbg_printf (" - [%p*%u:%s:%u:h=%u,", (void *) cp,
			cp->c_nrefs, kind_str [cp->c_kind],
			cp->c_wack,
			cp->c_handle);
	if (reader_bits)
		dbg_printf ("%s,%s",
			(cp->c_sstate) ? "NR" : "R",
			(cp->c_vstate) ? "NN" : "N");
	else {
		dbg_printf ("%u.%u", cp->c_seqnr.high, cp->c_seqnr.low);
		if (cp->c_dests [0]) {
			dbg_printf ("=>");
			dbg_printf ("%u", cp->c_dests [0]);
			if (cp->c_dests [1])
				dbg_printf ("+%u", cp->c_dests [1]);
		}
	}
	dbg_printf ("]");
	if (cp->c_length)
		dbg_printf ("->%p*%u,%lu bytes", cp->c_data,
					cp->c_db ? cp->c_db->nrefs : 0,
					(unsigned long) cp->c_length);
	if (newline)
		dbg_printf ("\r\n");
}

static void hc_dump_ref (CCREF *rp, int reader_bits)
{
	if (rp->change)
		hc_dump_change (rp->change, 1, reader_bits);
}

static void hc_dump_cclist (const char *s, CCLIST *lp, int reader_bits)
{
	CCREF	*rp;

	dbg_printf ("%s: (%u)", s, lp->l_nchanges);
	if (lp->l_nchanges) {
		dbg_printf (" {\r\n");
		LIST_FOREACH (lp->l_list, rp)
			hc_dump_ref (rp, reader_bits);
		dbg_printf ("      }");
	}
	dbg_printf ("\r\n");
}

#ifdef EXTRA_STATS

static int is_writer;

static void hc_inst_stats_dump (INST_STATS *sp, int writer, INSTANCE *ip)
{
	dbg_printf ("\t# of cache bytes added:   %llu\r\n", (unsigned long long) sp->i_octets);
	dbg_printf ("\t# of cache samples added: %u\r\n", sp->i_add);
	if (ip) {
		dbg_printf ("\t# of cache registers:     %u\r\n", ip->i_register);
		dbg_printf ("\t# of cache disposes:      %u\r\n", ip->i_dispose);
		dbg_printf ("\t# of cache unregisters:   %u\r\n", ip->i_unregister);
	}
	if (writer) {
		dbg_printf ("\t# of cache transfers:     %u\r\n", sp->c_transfer);
		dbg_printf ("\t# of RTPS new samples:    %u\r\n", sp->m_new);
		dbg_printf ("\t# of RTPS remove samples: %u\r\n", sp->m_remove);
		dbg_printf ("\t# of RTPS urgents:        %u\r\n", sp->m_urgent);
	}
	else {
		dbg_printf ("\t# of cache notifications: %u\r\n", sp->l_notify);
		dbg_printf ("\t# of cache gets:          %u\r\n", sp->i_get);
	}
}

#endif

static void hc_dump_instance (INSTANCE            *ip,
			      const TypeSupport_t *ts,
			      int                 secure,
			      int                 reader_bits)
{
	unsigned	i;
	handle_t	*hp;

	dbg_printf ("%4u: ", ip->i_handle);
	for (i = 0; i < 16; i++) {
		if (i && (i & 0x3) == 0)
			dbg_printf (":");
		dbg_printf ("%02x", ip->i_hash.hash [i]);
	}
	dbg_printf (", %s, %s, D/NW=%u/%u, ", kind_str [ip->i_kind],
				ip->i_view ? "NOT_NEW" : "NEW",
				ip->i_disp_cnt,
				ip->i_no_w_cnt);
	if (ip->i_inform)
		dbg_printf ("I, ");
	if (ip->i_deadlined)
		dbg_printf ("DL, ");
	if (ip->i_nwriters) {
		dbg_printf ("W:");
		if (ip->i_nwriters <= DNWRITERS)
			hp = ip->i_writers.w;
		else
			hp = ip->i_writers.ptr->w;
		for (i = 0; i < ip->i_nwriters; i++) {
			if (i)
				dbg_printf (",");
			if (hp [i] == ip->i_owner)
				dbg_printf ("[");
			dbg_printf ("%u", hp [i]);
			if (hp [i] == ip->i_owner)
				dbg_printf ("]");
		}
		dbg_printf (" ");
	}
	dbg_printf ("{%u}\r\n", ip->i_ndata);
#ifdef EXTRA_STATS
	dbg_printf ("      Statistics:\r\n");
	hc_inst_stats_dump (&ip->i_stats, is_writer, ip);
#endif
	if (ip->i_key) {
		dbg_printf ("      Key:\r\n");
#ifdef PARSE_KEY
		dbg_printf ("\t");
		DDS_TypeSupport_dump_key (1, ts, str_ptr (ip->i_key), 0, 0,
								secure, 1);
		dbg_printf ("\r\n");
#else
		ARG_NOT_USED (ts)

		dbg_print_region (str_ptr (ip->i_key), str_len (ip->i_key), 0, 0);
#endif
	}
	if (!ip->i_nchanges) {
		dbg_printf ("      Time:");
		dbg_print_time (&ip->i_time);
		dbg_printf ("\r\n");
	}
	dbg_printf ("      ");
	hc_dump_cclist ("->", &ip->i_list, reader_bits);
}

typedef struct hc_dump_inst_st {
	const TypeSupport_t	*ts;
	int			hashes;
	int			secure;
	int			reader;
} DUMP_INST;

/* hc_dump_sl_inst -- Display a skiplist node containing an instance. */

static int hc_dump_sl_inst (Skiplist_t *list, void *node, void *arg)
{
	DUMP_INST	*di = (DUMP_INST *) arg;
	INSTANCE	*ip, **ipp = (INSTANCE **) node;

	ARG_NOT_USED (list)

	ip = *ipp;
	hc_dump_instance (ip, di->ts, di->secure, di->reader);
	if (di->hashes)
		while (ip->i_next) {
			ip = ip->i_next;
			hc_dump_instance (ip, di->ts, di->secure, di->reader);
		}
	return (1);
}

/* dbg_print_limit -- Display a cache limit. */

void dbg_print_limit (unsigned limit, unsigned max, const char *s)
{
	if (limit != max)
		dbg_printf ("%u", limit);
	else
		dbg_printf ("*");
	dbg_printf (" %s", s);
}

unsigned hc_total_changes (Cache_t cache)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;

	return (hcp->hc_nchanges);
}

int hc_total_instances (Cache_t cache)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;

	return ((hcp->hc_multi_inst) ? (int) hcp->hc_ninstances : -1);
}

/* hc_cache_dump -- Display the cache contents. */

void hc_cache_dump (Cache_t cache)
{
	HistoryCache_t	*hcp = (HistoryCache_t *) cache;
	CCREF		*rp;
	INSTANCE	*ip;
	GUID_t		guid;
	DUMP_INST	dinst;

	dbg_printf ("Topic/Type: %s/%s%c\r\n", str_ptr (hcp->hc_endpoint->ep.topic->name),
					     str_ptr (hcp->hc_endpoint->ep.topic->type->type_name),
					     (hcp->hc_ref_type) ? '*' : ' ');
	dbg_printf ("GUID: ");
	guid.prefix = hcp->hc_endpoint->ep.u.subscriber->domain->participant.p_guid_prefix;
	guid.entity_id = hcp->hc_endpoint->ep.entity_id;
	dbg_print_guid_prefix (&guid.prefix);
	dbg_printf ("-");
	dbg_print_entity_id (NULL, &guid.entity_id);
	dbg_printf ("\r\n");
	if (!hcp->hc_must_ack)
		dbg_printf ("Max. depth = %u, ", hcp->hc_endpoint->ep.qos->qos.history_depth);
	dbg_printf ("Flags = %s/", (hcp->hc_durability == 0) ? "Volatile" : "Transient-Local");
	if (hcp->hc_auto_disp)
		dbg_printf ("Auto-dispose/");
	if (hcp->hc_src_time)
		dbg_printf ("SrcTime/");
	dbg_printf ("%s", hcp->hc_inst_order ? "Inst-ordered/" : "Arrival-ordered/");
	dbg_printf ("%s", hcp->hc_must_ack ? "Keep-all" : "Keep-last");
#ifdef DDS_NATIVE_SECURITY
	if (hcp->hc_secure_h)
		dbg_printf ("/secure");
#endif
	dbg_printf ("\r\nLimits: ");
	dbg_print_limit (hcp->hc_max_samples, MAX_LIST_CHANGES, "samples, ");
	dbg_print_limit (hcp->hc_max_inst, MAX_INST_HANDLE, "instances, ");
	dbg_print_limit (hcp->hc_max_depth, MAX_LIST_CHANGES, "samples/instance\r\n");
	dbg_printf ("Hashed: %u, monitor: %u, inform: %u, unacked: %u",
		hcp->hc_skiplists, hcp->hc_monitor, hcp->hc_inform, 
		hcp->hc_unacked);
	if (hcp->hc_multi_inst)
		dbg_printf (", ownership: %s", hcp->hc_exclusive ? "exclusive" : "shared");
	dbg_printf ("\r\n");
#ifdef EXTRA_STATS
	if (!hcp->hc_multi_inst || hcp->hc_writer) {
		dbg_printf ("Statistics:\r\n");
		if (!hcp->hc_multi_inst)
			hc_inst_stats_dump (&hcp->hc_stats, hcp->hc_writer, NULL);
		if (hcp->hc_writer) {
			dbg_printf ("\t# of RTPS connects:       %u\r\n", hcp->hc_mon_start);
			dbg_printf ("\t# of RTPS disconnects:    %u\r\n", hcp->hc_mon_stop);
			dbg_printf ("\t# of RTPS replays:        %u\r\n", hcp->hc_mon_replay);
			dbg_printf ("\t# of RTPS unblocks:       %u\r\n", hcp->hc_unblock);
		}
	}
#endif
	dbg_printf ("Changes:");
	if (hcp->hc_nchanges)  {
		dbg_printf (" {%u}\r\n", hcp->hc_ndata);
		LIST_FOREACH (hcp->hc_list, rp)
			hc_dump_change (rp->change, 1, !hcp->hc_writer);
	}
	else
		dbg_printf ("None {%u}.\r\n", hcp->hc_ndata);

	if (!hcp->hc_multi_inst)
		hc_dump_cclist ("\r\nNo key changes", &hcp->hc_changes, !hcp->hc_writer);
	else {
		dbg_printf ("Last handle = %u, Maximum key size = ",
				hcp->hc_last_handle);
		if (hcp->hc_key_size)
			dbg_printf ("%u\r\n", hcp->hc_key_size);
		else
			dbg_printf ("*\r\n");
#ifdef EXTRA_STATS
		is_writer = hcp->hc_writer;
#endif
		dinst.ts = hcp->hc_endpoint->ep.topic->type->type_support;
		dinst.hashes = 1;
		dinst.secure = hcp->hc_secure_h;
		dinst.reader = !hcp->hc_writer;
		if (!hcp->hc_skiplists)
			for (ip = hcp->hc_inst_head; ip; ip = ip->i_next)
				hc_dump_instance (ip, dinst.ts, dinst.secure, dinst.reader);
		else {
			dbg_printf ("Hashes:\r\n");
			sl_walk (hcp->hc_hashes, hc_dump_sl_inst, &dinst);
			dinst.hashes = 0;
			dbg_printf ("Handles:\r\n");
			sl_walk (hcp->hc_handles, hc_dump_sl_inst, &dinst);
		}
	}
}

/* hc_pool_dump -- Display pool data and statistics. */

void hc_pool_dump (size_t sizes [])
{
	print_pool_table (mem_blocks, (unsigned) MB_END, sizes);
}

#endif

