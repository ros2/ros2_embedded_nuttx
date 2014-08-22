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

/* cache.h -- Defines the interface to the history cache. */

#ifndef __cache_h_
#define	__cache_h_

#include <stdint.h>
#include "sys.h"
#include "str.h"
#include "seqnr.h"
#include "db.h"
#include "bytecode.h"
#include "guid.h"
#include "handle.h"

typedef handle_t InstanceHandle;

typedef enum {
	ALIVE,
	NOT_ALIVE_DISPOSED,
	NOT_ALIVE_UNREGISTERED,
	ZOMBIE
} ChangeKind_t;

typedef enum {
	READ,
	NOT_READ
} SampleState_t;

typedef enum {
	NEW,
	NOT_NEW
} ViewState_t;

typedef struct {
	uint32_t	secs;
	uint32_t	nanos;
} Duration_t;

Ticks_t duration2ticks (const Duration_t *dp);

/* Convert a duration parameter to a # of timer ticks. */

typedef struct key_hash_t {
	unsigned char		hash [16];
} KeyHash_t;

typedef struct gen_ctrs_st {
	unsigned	disp_cnt;	/* Disposed generation count. */
	unsigned	no_w_cnt;	/* No Writers generation count. */
	unsigned	abs_cnt;	/* Absolute generation count. */
} GenCounters_t;

#define	MAX_DW_DESTS	((sizeof (GenCounters_t) - sizeof (SequenceNumber_t)) / \
							sizeof (handle_t))

typedef struct change_winfo_st {
	SequenceNumber_t seqnr;			/* Sequence number. */
	handle_t	 dests [MAX_DW_DESTS];	/* Destination endpoints. */
} ChangeWInfo_t; 

#define	C_DSIZE	sizeof (void *)

typedef struct change_st Change_t;
struct change_st {
	unsigned	c_nrefs:12;	/* # of entities using this. */
	unsigned	c_wack:11;	/* # of active reliable transfers. */
	unsigned	c_urgent:1;	/* Urgent data. */
	unsigned	c_kind:2;	/* Type of cache change. */
	unsigned	c_linear:1;	/* Data buffer is linear. */
	unsigned	c_cached:1;	/* Sample is still in cache. */
	unsigned	c_sstate:1;	/* Sample state: READ=0, NOT_READ=1. */
	unsigned	c_vstate:1;	/* View state: NEW=0, NOT_NEW=1. */
	unsigned	c_istate:2;	/* Instance state at collect time. */
	handle_t	c_writer;	/* Writer endpoint. */
	InstanceHandle	c_handle;	/* Instance/destination handle(rx/tx).*/
	FTime_t		c_time;		/* Timestamp. */
	size_t		c_length;	/* Length of data chunk. */
	DB		*c_db;		/* Data buffer if non-NULL. */
	unsigned char	*c_data;	/* Data pointer (possibly prefixed). */
	union {
	 ChangeWInfo_t	winfo;		/* Writer information. */
	 GenCounters_t  ctrs;		/* Generation counters. */
	}		u;
#define c_seqnr		u.winfo.seqnr
#define	c_dests		u.winfo.dests
#define c_disp_cnt	u.ctrs.disp_cnt
#define c_no_w_cnt	u.ctrs.no_w_cnt
#define c_abs_cnt	u.ctrs.abs_cnt
	unsigned char	c_xdata [C_DSIZE]; /* Data storage if <= C_DSIZE. */
};

typedef struct change_info_st {
	Change_t	*change;
        void            *sample_info;
	void		*user;
	void		*bufp;
} ChangeInfo_t;

typedef struct changes_st {
	ChangeInfo_t	*buffer;
	unsigned	length;
	unsigned	maximum;
} Changes_t;

typedef void *Cache_t;	/* History Cache descriptor. */

typedef struct cache_config_st {
	POOL_LIMITS	cache;		/* History caches. */
	POOL_LIMITS	instance;	/* Instances. */
	POOL_LIMITS	change;		/* Cache change. */
	POOL_LIMITS	ccrefs;		/* Cache change references. */
	POOL_LIMITS	crefs;		/* Cache references. */
	POOL_LIMITS	cwaits;		/* Cache wait contexts. */
	POOL_LIMITS	cxfers;		/* Cache pending transfer contexts. */
	POOL_LIMITS	xflists;	/* Cache transfer lists. */
	POOL_LIMITS	filters;	/* Time-based filters. */
	POOL_LIMITS	finsts;		/* Filter instance nodes. */
} CACHE_CONFIG;


int hc_pool_init (const CACHE_CONFIG *cfg);

/* Initialize the pools used for the history cache. */

void hc_pool_free (void);

/* Free the pools used for the history cache. */

Cache_t hc_new (void *endpoint, int raw_prefix);

/* Create a new (empty) history cache with the given parameters. */

void hc_enable (Cache_t cache);

/* Enable a cache. */

void hc_qos_update (Cache_t cache);

/* Updated QoS parameters of a cache. */

void hc_free (Cache_t cache);

/* Free a history cache. */

typedef void *HCI;

typedef int (*HCREQIFCT) (uintptr_t user, Change_t *cp, HCI hci);

/* Callback function that is used when a new cache entry is added. */

typedef int (*HCREQFCT) (uintptr_t user, Change_t *cp);

/* Callback function that is used when a new cache entry is to be deleted,
   unblocked or needs to be sent urgently. */

typedef void (*HCALIVEFCT) (uintptr_t user);

/* Callback funtion to indicate liveliness. */

typedef void (*HCFINSTFCT) (uintptr_t user, HCI hci);

/* Callback function to inform instance disappearance. */

int hc_monitor_fct (HCREQIFCT  add_fct,
		    HCREQFCT   delete_fct,
		    HCREQFCT   urgent_fct,
		    HCREQFCT   unblock_fct,
		    HCALIVEFCT alive_fct,
		    HCFINSTFCT iflush_fct);

/* Install callback functions to monitor cache addition/removals on writer
   caches as well as to be notified of urgent reliable acknowledgements and
   reader being unblocked. */

int hc_monitor_start (Cache_t hcp, uintptr_t user);

/* Enable cache monitoring for a given cache and with the specified user
   parameter. */

int hc_monitor_end (Cache_t hcp);

/* Disable cache monitoring for a cache. */

int hc_inform_start (Cache_t hcp, uintptr_t user);

/* Enable instance lifetime notifications. */

void hc_inform_end (Cache_t hcp);

/* Disable instance lifetime notifications. */

void hc_inst_inform (Cache_t hcp, HCI hci);

/* Notify a reader when the instance is removed. */

typedef void (*HCRDNOTFCT) (uintptr_t user, Cache_t cdp);

/* Function that can be used for notification of cache clients.
   Reader notifications will be delivered when new cache data becomes available.
   The new data must still be fetched via hc_get[_data] of course.
   Writer notifications can optionally be done to be notified of successful
   transfers of reliable data to remote endpoints. */

int hc_request_notification (Cache_t hcp, HCRDNOTFCT notify_fct, uintptr_t user);

/* Enable cache notifications for a given cache and with the specified user
   parameter.  The semantics of notifications change between reader and writer
   caches are as described above. */

HCI hc_register (Cache_t             hcp,
		 const unsigned char *key,
		 size_t              keylen,
		 const FTime_t       *time,
		 InstanceHandle      *h);

/* Register a new instance in the cache, based on the given key. */

HCI hc_lookup_key (Cache_t             hcp,
		   const unsigned char *key,
		   size_t              keylen,
		   InstanceHandle      *h);

/* Lookup an existing instance in the cache. */

#define	LH_LOOKUP	0	/* Just lookup. */
#define	LH_ADD_NEW_H	1	/* Add and allocate a new handle. */
#define	LH_ADD_SET_H	2	/* Add and set the handle to *h. */

typedef enum {
	RC_Accepted,
	RC_InstanceLimit,
	RC_SamplesLimit,
	RC_SamplesPerInstanceLimit
} RejectCause_t;

HCI hc_lookup_hash (Cache_t             hcp,
		    const KeyHash_t     *hpp,
		    const unsigned char *key,
		    size_t              keylen,
		    InstanceHandle      *h,
		    int                 add,
		    unsigned		ooo,
		    RejectCause_t       *cause);

/* Lookup an instance in the cache using a hash key.  If the key doesn't exist
   yet, it depends on the add parameter what will happen.  If add != 0, and
   current resource usage allows it, a new instance is added, either with a new
   allocated handle (LH_ADD_NEW_H) or with the handle specified in *h
   (LH_ADD_SET_H).  The ooo parameter indicates that the add is for an out-of-
   order data sample.  If successful, an HCI is returned. Else, the function
   returns NULL, and *cause will be set. */

void hc_inst_free (Cache_t hcp, InstanceHandle handle);

/* Free the instance with the given instance handle. */

int hc_inst_info (HCI             hci,
	          const KeyHash_t **hpp,
	          String_t        **spp);

/* Returns instance information, i.e. the Hash key and the Key fields.
   Caller is only supposed to do a str_unref() when finished with key *spp. */

void hc_inst_done (Cache_t cp, const unsigned char *key, size_t keylen);

/* Cleanup an instance from a cache. */

void hc_reclaim_keyed (Cache_t cache, GuidPrefix_t *prefix);

/* Reclaim instances with a key matching the given prefix. */

int hc_accepts (Cache_t hcp, unsigned ooo);

/* Check if a sample can be added to a no-instance cache with the out-of-order
   degree specified in ooo.  If there is room in the cache, a non-0 result is
   returned, otherwise 0 is returned. */

int hc_seqnr_info (Cache_t          cp, 
		   SequenceNumber_t *min_snr,
		   SequenceNumber_t *max_snr);

/* Return the next sequence number that will be used. */

int hc_write_required (Cache_t cp);

/* Check if cache requires samples. */

int hc_dispose (Cache_t        cp,
		InstanceHandle handle,
		HCI            hci,
		const FTime_t  *time,
	        handle_t       dests [],
	        unsigned       ndests);

/* Free a previously written sample from the cache. */

int hc_unregister (Cache_t        hcp,
		   InstanceHandle h,
		   HCI            hci,
		   const FTime_t  *time,
	           handle_t       dests [],
	           unsigned       ndests);

/* Unregister an existing instance in the cache. */

int hc_alive (Cache_t hcp);

/* Indicate that Writer is still alive. */

int hc_add_inst (Cache_t cp, Change_t *change, HCI hci, int rel);

/* Add a sample to the cache.  Either the hci parameter, or if NULL, the instance
   handle in the change record is used to specify the instance.  The timestamp
   should be set correctly. The rel parameter indicates that the request is from
   a reliable connection and CACHE_ERR_FULL may be returned. */

int hc_add_received (Cache_t cp, Change_t *change, HCI hci, int rel);

/* Same as hc_add_inst, but should be used for reader caches that have an active
   time-based filter. */

void hc_rem_writer_add (Cache_t cache, handle_t writer);

/* Notify that a new cache writer has arrived. */

void hc_rem_writer_removed (Cache_t cache, handle_t writer);

/* Notify that a cache writer was removed. */

void hc_reader_lifespan (Cache_t cache, int enable);

/* Set the status of a reader for lifespan checks. */

/* Mask of items to be skipped in hc_get() or to check in hc_avail[_condition](): */
#define	SKM_READ	0x01	/* Already read samples. */
#define	SKM_NOT_READ	0x02	/* Unread samples. */
#define	SKM_NEW_VIEW	0x04	/* New instances. */
#define	SKM_OLD_VIEW	0x08	/* Existing instances. */
#define	SKM_ALIVE	0x10	/* Real samples. */
#define	SKM_DISPOSED	0x20	/* Disposed instances. */
#define	SKM_NO_WRITERS	0x40	/* Instances without writers. */

#define	SKM_ALL		0x7f	/* All possibilities. */

int hc_get (Cache_t        cp,
            unsigned       *nchanges,
	    Changes_t      *entries,
	    unsigned       skip_mask,
	    BCProgram      *fp,
	    Strings_t      *pp,
	    void           *cache,
	    BCProgram      *op,
	    InstanceHandle h,
	    int            next,
	    int            rem);

/* Get a number of samples from the cache, either for reading or for taking.
   Note that entries, when taken, should be disposed later on by calling
   hc_done().  If h == 0 and next == 0 it indicates that any instance is valid.
   If h != 0 and next == 0, it specifies that sample must be from the given
   instance.  If next != 0, it specifies that sample should be from the first
   instance following h that contains data.
   The abs_gen_rank parameter will be filled in with the DDS absolute generation
   ranking of each sample, relative for the instance.
   The skip_mask parameter determines which samples are *not* of interest to the
   caller.  See SKM_* for details for what can be skipped.
   The fp, pp and op parameters are used when a Query expression was given. */

int hc_done (Cache_t      cp,
             unsigned     nchanges,
	     ChangeInfo_t entries []);

/* The cache entries are processed completely and can finally be disposed. */

int hc_get_data (Cache_t        cp,
                 unsigned       *nchanges,
	         Change_t       **changes,
		 unsigned       skipmask,
		 InstanceHandle h,
	         int            next,
	         int            rem);

/* Get a number of samples from the cache, either for reading or for taking.
   Note that entries, when taken, should be disposed later on by calling
   hc_done().  If h == 0 and next == 0 it indicates that any instance is valid.
   If h != 0 and next == 0, it specifies that sample must be from the given
   instance.  If next != 0, it specifies that sample should be from the first
   instance following h that contains data. */

int hc_get_key (Cache_t        cp,
		InstanceHandle h,
		void           *data,
		int            dynamic);

/* Get the key data from an instance handle in either a native data struct or
   via a DynamicData reference. */

const unsigned char *hc_key_ptr (Cache_t cp, HCI hci);

/* Return a direct pointer to the instance key data. */

int hc_avail (Cache_t cp, unsigned skipmask);

/* Return a non-0 result if there are samples in the cache not matching the
   given skipmask. */

int hc_avail_condition (Cache_t   cp,
			unsigned  mask,
			Strings_t *pars,
			BCProgram *query,
			void      *bc_cache);

/* Return a non-0 result if there are samples in the cache matching the status
   mask and the query filter. */

Change_t *hc_change_new (void);

/* Allocate a new change. */

Change_t *hc_change_clone (Change_t *cp);

/* Clone an existing change record. */

void hc_change_dispose (Change_t *change);

/* Dispose of a change. */

#define hc_change_free(cp) do { rcl_access (cp); (cp)->c_nrefs--; \
				if (!(cp)->c_nrefs) { \
					rcl_done (cp);\
					hc_change_dispose(cp); }\
				else \
					rcl_done (cp);\
			   } while (0)

/* Free a change. */

int hc_replay (Cache_t cp, HCREQIFCT fct, uintptr_t user);

/* Replay all cache entries to a new reader. */

void hc_acknowledged (Cache_t cache, HCI hci, SequenceNumber_t *seqnr);

/* Signal an acknowled transfer in reliable mode. */

int hc_wait_acks (Cache_t cp, const Duration_t *max_wait);

/* Wait for acknowledgements. */

void hc_deliver_notifications (void);

/* Notify interested parties of changes that occurred. */

int hc_matches (Cache_t wcp, Cache_t rcp);

/* Check if the writer cache matches with the given reader cache. */

void hc_pool_dump (size_t sizes []);

/* Display pool data and statistics. */

void hc_cache_dump (Cache_t cache);

/* Dump the cache contents. */

unsigned hc_total_changes (Cache_t cache);

/* Return the number of samples in the cache. */

int hc_total_instances (Cache_t cache);

/* Return the number of instances in the cache. */

void hc_transfer_samples (void);

/* Perform cache transfers if possible. */

Ticks_t hc_handle_xqos (Cache_t cache, int type, handle_t w, Ticks_t period);

/* Called periodically to handle one of the special QoS parameters (Deadline/
   Lifespan/Reader-Data-lifecycle).  The returned value indicates either that no
   checks of the given type need to be done anymore (== 0), or that a timer
   needs to be started to recheck the QoS after some time (# of ticks returned).
 */

typedef void *TBF;	/* Time-based filter context. */

typedef void (*TBFSENDFCT) (void *proxy, void *sample, HCI hci, int rel);
typedef void (*TBFDONEFCT) (void *proxy, void *sample);

/* Callback function to inform a user that a sample is either no longer needed
   or when it becomes valid after having been pending for some time. */

TBF hc_tbf_new (Cache_t    cache,
		void       *proxy,
		TBFSENDFCT send_fct,
		TBFDONEFCT done_fct,
		Duration_t *separation);

/* Create a new time-based filter context for a cache, with suitable completion
   functions and the minimum duration between samples of the same instance. */

void hc_tbf_free (Cache_t cache, void *proxy);

/* Free a Time-based filter context and cleanup all pending changes/instances.*/

int hc_tbf_add (TBF f, HCI hci, const FTime_t *t, void *sample, int rel);

/* Check if we may enqueue a sample to a destination proxy.  If allowed, a 
   non-zero result is returned, and the sample can be enqueud normally.
   If not allowed (0-return), the sample will be queued automatically for future
   transmission (after a suitable time-out). */


void hc_check (Cache_t cache);

/* Validate the contents of a cache (if CACHE_CHECK defined). */


#endif	/* !__cache_h_ */
