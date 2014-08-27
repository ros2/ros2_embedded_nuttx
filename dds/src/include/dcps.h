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

/* dcps.h -- Interface to the DCPS functionality.  Note that this header file
	     only specifies the internal interface, since the public DDS API
	     is mapped directly on the DCPS layer and that API is located in
	     dds.h */

#ifndef __dcps_h_
#define __dcps_h_

#include "dds/dds_dcps.h"
#include "dds_data.h"

#define	DCPS_OK			0	/* No error. */
#define	DCPS_ERR_NOMEM		1	/* Out of memory. */
#define	DCPS_ERR_EXISTS		2	/* Already exists. */
#define	DCPS_ERR_NFOUND		3	/* Not found. */
#define	DCPS_ERR_PARAM		4	/* Invalid parameter error. */
#define	DCPS_ERR_UNIMPL		5	/* Feature not implemented (yet). */

typedef enum {
	CC_STATUS,	/* StatusCondition. */
	CC_READ,	/* ReadCondition. */
	CC_QUERY,	/* QueryCondition. */
	CC_GUARD	/* GuardCondition. */
} ConditionClass_t;

typedef struct DDS_WaitSet_st WaitSet_t;
typedef struct DDS_Condition_st Condition_t;
struct DDS_Condition_st {
	WaitSet_t		*waitset;	/* Parent waitset. */
	ConditionClass_t	class;		/* Class of condition. */
	Condition_t		*next;		/* Next condition in waitset. */
	Condition_t		*e_next;	/* Next condition for entity. */
	int			deferred;	/* In deferred check list. */
};

struct DDS_WaitSet_st {
	unsigned	nconditions;
	Condition_t	*first;
	Condition_t	*last;
	lock_t		lock;
	cond_t		wakeup;
	int		nwaiting;
};

typedef struct DDS_StatusCondition_st StatusCondition_t;
struct DDS_StatusCondition_st {
	Condition_t		c;
	Entity_t		*entity;
	DDS_StatusMask		enabled;
};

typedef struct DDS_ReadCondition_st ReadCondition_t;
struct DDS_ReadCondition_st {
	Condition_t		c;
	Reader_t		*rp;
	DDS_SampleStateMask	sample_states;
	DDS_ViewStateMask	view_states;
	DDS_InstanceStateMask	instance_states;
};

typedef struct DDS_QueryCondition_st QueryCondition_t;
struct DDS_QueryCondition_st {
	ReadCondition_t		rc;
	String_t		*expression;
	Strings_t		*expression_pars;
	BCProgram		filter;
	BCProgram		order;
	void			*cache;
};

typedef struct DDS_GuardCondition_st GuardCondition_t;
struct DDS_GuardCondition_st {
	Condition_t		c;
	int			value;
};

typedef struct dcps_config_st {
	POOL_LIMITS	sampleinfos;	/* Sample info instances. */
	POOL_LIMITS	waitsets;	/* Waitsets. */
	POOL_LIMITS	statusconds;	/* StatusConditions. */
	POOL_LIMITS	readconds;	/* ReadConditions. */
	POOL_LIMITS	queryconds;	/* ReadConditions. */
	POOL_LIMITS	guardconds;	/* GuardConditions. */
	POOL_LIMITS	topicwaits;	/* TopicWait clients. */
} DCPS_CONFIG;


/* Overall control.
   ---------------- */

DDS_ReturnCode_t dcps_init (const DCPS_CONFIG *cp);

/* Initialize the DCPS layer of DDS. */

void dcps_final (void);

/* Finalize the DCPS layer of DDS. */

void duration2timespec (DDS_Duration_t *timeout, struct timespec *ts);

/* Convert a duration to a timespec. */


/* Status notifications.
   --------------------- */

void dcps_inconsistent_topic (Topic_t *tp);

/* Notify an inconsistent topic. */

void dcps_offered_deadline_missed (Writer_t *wp, DDS_InstanceHandle_t handle);

/* Offered deadline missed. */

void dcps_requested_deadline_missed (Reader_t *rp, DDS_InstanceHandle_t handle);

/* Requested deadline missed. */

void dcps_offered_incompatible_qos (Writer_t *wp, DDS_QosPolicyId_t policy_id);

/* Incompatible QoS offered. */

void dcps_requested_incompatible_qos (Reader_t *wp, DDS_QosPolicyId_t policy_id);

/* Incompatible QoS requested. */

void dcps_samples_lost (Reader_t *rp, unsigned nsamples);

/* Samples were lost. */

void dcps_sample_rejected (Reader_t                     *rp,
			   DDS_SampleRejectedStatusKind kind,
			   DDS_InstanceHandle_t         handle);

/* Sample was rejected for the given reason. */

void dcps_liveliness_lost (Writer_t *wp);

/* Reader liveliness was lost. */

#define	DLI_ADD		1	/* For a new writer. */
#define	DLI_EXISTS	0	/* For an existing writer. */
#define	DLI_REMOVE	-1	/* Existing writer removed. */

void dcps_liveliness_change (Reader_t             *rp,
			     int                  mode,
			     int                  alive,
			     DDS_InstanceHandle_t handle);

/* A liveliness change occurred. */

void dcps_publication_match (Writer_t         *wp,
			     int              add, 
			     const Endpoint_t *ep);

/* Notify a change in the # of reader proxies of a writer. */

void dcps_subscription_match (Reader_t         *rp,
			      int              add, 
			      const Endpoint_t *ep);

/* Notify a change in the # of writer proxies of a reader. */

void dcps_deferred_waitset_check (void *e, void *cp);

/* Check WaitSet condition. */


/* Unmarshalling.
   -------------- */

void *dcps_get_cdata (void                *bufp,
		      Change_t            *cp,
		      const TypeSupport_t *ts,
		      int                 dynamic,
		      DDS_ReturnCode_t    *error,
		      void                **auxp);

/* Get cache data in a user buffer. */


/* DataWriter primitive functions.
   ------------------------------- */

DDS_InstanceHandle_t dcps_register_instance (DDS_DataWriter   w,
					     const void       *data,
					     int              dynamic,
					     const FTime_t    *time);

DDS_ReturnCode_t dcps_unregister_instance (DDS_DataWriter             w,
					   const void                 *data,
					   int                        dynamic,
					   const DDS_InstanceHandle_t handle,
					   const FTime_t              *time,
					   DDS_InstanceHandleSeq      *dests);

DDS_ReturnCode_t dcps_write (DDS_DataWriter             w,
			     const void                 *data,
			     int                        dynamic,
			     const DDS_InstanceHandle_t handle,
			     const FTime_t              *time,
			     DDS_InstanceHandleSeq      *dests);

DDS_ReturnCode_t dcps_dispose (DDS_DataWriter             w,
			       const void                 *data,
			       int                        dynamic,
			       const DDS_InstanceHandle_t handle,
			       const FTime_t              *time,
			       DDS_InstanceHandleSeq      *dests);


/* DataReader primitive functions.
   ------------------------------- */

DDS_ReturnCode_t dcps_reader_get (Reader_t              *rp,
				  DDS_VoidPtrSeq        *data,
				  int                   dynamic,
				  DDS_SampleInfoSeq     *info_seq,
				  unsigned              max_samples,
				  DDS_SampleStateMask   sample_states,
				  DDS_ViewStateMask     view_states,
				  DDS_InstanceStateMask instance_states,
				  QueryCondition_t      *qcp,
				  DDS_InstanceHandle_t  handle,
				  int                   next,
				  int                   take);

DDS_ReturnCode_t dcps_return_loan (Reader_t          *r,
				   DDS_VoidPtrSeq    *data,
				   int               dynamic,
				   DDS_SampleInfoSeq *info_seq);

HCI handle_get (Topic_t          *tp,
		Cache_t          *cp,
		const void       *data,
		int              dynamic,
		int              secure,
		InstanceHandle   *h,
		DDS_ReturnCode_t *ret);

/* Debug functionality.
   -------------------- */

void dcps_endpoints_dump (void);

/* Display the registered endpoints. */

void dcps_cache_dump (Endpoint_t *ep);

/* Dump the cache of a single endpoint. */

void dcps_pool_dump (size_t sizes []);

/* Display some pool statistics. */

void dcps_readcondition_dump (ReadCondition_t *rp);

/* Display a ReadCondition. */

void dcps_querycondition_dump (QueryCondition_t *cp);

/* Display a QueryCondition. */

#endif /* __dcps_h_ */
