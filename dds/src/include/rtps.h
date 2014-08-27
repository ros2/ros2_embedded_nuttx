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

/* rtps.h -- Defines the interface to the RTPS protocol.  This interface is
	     intended to be used by the DCPS layer of DDS. */

#ifndef __rtps_h_
#define	__rtps_h_

#include "domain.h"
#include "dds/dds_error.h"
#include "typecode.h"
#include "cache.h"
#include "rtps_cfg.h"

typedef struct rtps_config_st {
	POOL_LIMITS	readers;	/* RTPS Reader instances. */
	POOL_LIMITS	writers;	/* RTPS Writer instances. */
	POOL_LIMITS	rreaders;	/* ReaderLocator/Proxy Readers. */
	POOL_LIMITS	rwriters;	/* Proxy Writer instances. */
	POOL_LIMITS	ccrefs;		/* RTPS Cache Change references. */
	POOL_LIMITS	messages;	/* Message buffers. */
	POOL_LIMITS	msgelements;	/* Message elements. */
	POOL_LIMITS	msgrefs;	/* Message references. */
} RTPS_CONFIG;

extern int rtps_used;
extern int rtps_rx_active;

int rtps_init (const RTPS_CONFIG *limits);

/* Initialize the RTPS layer with the given configuration parameters. */

void rtps_final (void);

/* Finalize the RTPS layer. */


typedef unsigned char ProtocolId_t [4];

#define	PROTOCOL_RTPS	{ 'R', 'T', 'P', 'S' }

/*#define protocol_valid(p)	*((uint32_t *) p) == *((uint32_t *) rtps_protocol_id)*/
#define protocol_valid(p)	!memcmp (p, rtps_protocol_id, sizeof (ProtocolId_t))
/*#define protocol_set(p)	*((uint32_t *) p) = *((uint32_t *) rtps_protocol_id) */
#define	protocol_copy(dp,sp)	memcpy (dp, sp, sizeof (ProtocolId_t))
#define	protocol_set(p)		protocol_copy (p, rtps_protocol_id)

extern ProtocolId_t		rtps_protocol_id;
extern ProtocolVersion_t	rtps_protocol_version;
extern VendorId_t		rtps_vendor_id;

extern const EntityId_t rtps_builtin_eids [];	   /* Builtin Endpoint ids. */
extern const char *rtps_builtin_endpoint_names []; /* Builtin Endpoint names. */
extern const char *rtps_builtin_topic_names [];	   /* Builtin Topic names. */
extern EntityId_t rtps_entity_id_unknown;	   /* Unknown Endpoint Id. */


/* 1. Participant operations.
   -------------------------- */

int rtps_participant_create (Domain_t *domain);

/* Create a new domain participant. */

int rtps_participant_update (Domain_t *domain);

/* Domain locators have been updated. */

int rtps_participant_delete (Domain_t *domain);

/* Delete a domain participant and all entities contained within. */

typedef enum {
	NO_KEY = 1,
	WITH_KEY
} TopicKind_t;

typedef enum {
	BEST_EFFORT = 1,
	RELIABLE = 3
} ReliabilityKind_t;


/* 2. Writer operations.
   --------------------- */

#define RTPSWriter_t Endpoint_t

int rtps_writer_create (Writer_t          *w,
			int               push_mode,
			int		  stateful,
			const Duration_t  *heartbeat,
			const Duration_t  *nack_rsp,
			const Duration_t  *nack_supp,
			const Duration_t  *resend_per);

/* Create a writer entity with the given parameters.
   If entity_id == ENTITYID_UNKNOWN, a valid value will be used.
   On return, *entity_id and *w will be filled in. */

int rtps_writer_delete (Writer_t *w);

/* Delete a writer entity. */

int rtps_reader_locator_add (Writer_t      *w,
			     LocatorNode_t *lp,
			     int           exp_inline_qos,
			     int           marshall);

/* Add a destination to a stateless writer. */

void rtps_reader_locator_remove (Writer_t *w, const Locator_t *lp);

/* Remove a destination from a stateless writer. */

int rtps_matched_reader_add (Writer_t *w, DiscoveredReader_t *dr);

/* Add a matched, i.e. proxy reader to a stateful writer. */

int rtps_matched_reader_remove (Writer_t *w, DiscoveredReader_t *dr);

/* Remove a matched, i.e. proxy reader from a stateful writer. */

int rtps_matched_reader_restart (Writer_t *w, DiscoveredReader_t *dr);

/* Restart a matched, i.e. proxy reader for a stateful writer. */

int rtps_writer_matches (Writer_t *w, DiscoveredReader_t *dr);

/* Return a non-0 result if the local Writer matches the remote reader. */

unsigned rtps_matched_reader_count (Writer_t *w);

/* Return the number of matched readers. */

int rtps_matched_reader_restart (Writer_t *w, DiscoveredReader_t *dr);

/* Restart a stateful reliable matched reader context. */

int rtps_writer_write (Writer_t       *w,
		       const void     *data,
		       size_t         length,
		       InstanceHandle h,
		       HCI            hci,
		       const FTime_t  *time,
		       handle_t       dests [],
		       unsigned       ndests);

/* Add data to a writer. */

int rtps_writer_dispose (Writer_t       *w,
			 InstanceHandle h,
			 HCI            hci,
			 const FTime_t  *t,
			 handle_t       dests [],
			 unsigned       ndests);

/* Dispose data from a writer.
   Note: Topic type must have a key in order to do this. */

int rtps_writer_unregister (Writer_t       *w,
			    InstanceHandle h,
			    HCI            hci,
			    const FTime_t  *t,
			    handle_t       dests [],
			    unsigned       ndests);

/* Unregister data from a writer.
   Note: Topic type must have a key in order to do this. */

int rtps_stateless_resend (Writer_t *w);

/* Resend changes on a stateless writer. */

int rtps_stateless_update (Writer_t *w, GuidPrefix_t *prefix);

/* An entry in the stateless writer cache was updated: resend it. */


/* 3. Reader operations.
   --------------------- */

int rtps_reader_create (Reader_t          *r,
			int		  stateful,
			const Duration_t  *heartbeat_resp,
			const Duration_t  *heartbeat_supp);

/* Create a reader entity. */

int rtps_reader_delete (Reader_t *r);

/* Delete a reader entity. */

int rtps_matched_writer_add (Reader_t *r, DiscoveredWriter_t *dw);

/* Add a matched, i.e. proxy writer to a stateful reader. */

int rtps_matched_writer_remove (Reader_t *r, DiscoveredWriter_t *dw);

/* Remove a matched, i.e. proxy writer from a stateful reader. */

int rtps_matched_writer_restart (Reader_t *r, DiscoveredWriter_t *dw);

/* Restart a matched, i.e. proxy writer for a stateful reader. */

int rtps_reader_matches (Reader_t *r, DiscoveredWriter_t *dw);

/* Return a non-0 result if the local Reader matches the remote writer. */

unsigned rtps_matched_writer_count (Reader_t *r);

/* Return the number of matched writers. */


/* 4. Some common endpoint-specific functions.
   ------------------------------------------- */

int rtps_endpoint_add_locator (LocalEndpoint_t *r, int mcast, const Locator_t *loc);

/* Add a specific locator to an endpoint. */

int rtps_endpoint_remove_locator (LocalEndpoint_t *r, int mcast, const Locator_t *loc);

/* Remove a specific locator from an endpoint. */

#define	rtps_endpoint_remove_all_locators(r,mcast) \
					rtps_reader_remove_locator(r,mcast,NULL)

/* Remove all locators of the given type from an endpoint. */

void rtps_endpoint_locators_update (Endpoint_t *r, int mcast);

/* Update the locators of the given remote endpoint. */

void rtps_endpoint_locality_update (Endpoint_t *r, int local);

/* Update the locality of the given remote endpoint. */

void rtps_endpoint_time_filter_update (Endpoint_t *r);

/* Update the remote endpoint time-based filter. */

void rtps_endpoint_content_filter_update (Endpoint_t *r);

/* Update the remote endpoint content filter. */

typedef enum {
	EM_START,		/* Start endpoint operation. */
	EM_SEND,		/* Send data. */
	EM_NEW_CHANGE,		/* Enqueue a change. */
	EM_REM_CHANGE,		/* Remove a change. */
	EM_RESEND_TO,		/* Resend changes timeout. */
	EM_ALIVE_TO,		/* Alive timeout. */
	EM_HEARTBEAT_TO,	/* Send HEARTBEAT timeout. */
	EM_NACKRSP_TO,		/* Send ACKNACK timeout. */
	EM_DATA,		/* DATA received. */
	EM_GAP,			/* GAP received. */
	EM_HEARTBEAT,		/* HEARTBEAT received. */
	EM_ACKNACK,		/* ACKNACK received. */
	EM_FINISH		/* Finish endpoint operation. */
} EndpointMarker_t;

void rtps_endpoint_markers_set (LocalEndpoint_t *r, unsigned markers);

/* Set a number of markers (1 << EM_*) on an endpoint. */

unsigned rtps_endpoint_markers_get (LocalEndpoint_t *r);

/* Get the current endpoint markers. */

typedef void (*RMNTFFCT) (LocalEndpoint_t *r, EndpointMarker_t m, const void *data);

/* Endpoint marker callback. */

void rtps_endpoint_marker_notify (unsigned markers, RMNTFFCT f);

/* Install a marker notification function.  If f == NULL, the function is set
   to the default marker notification (i.e. log to the output). */


/* 5. Auxiliary functions.
   ----------------------- */

void rtps_send_changes (void);

/* Send queued messages. */

int rtps_wait_data (Reader_t *rp, const Duration_t *wait);

/* Wait for historical data. */

void rtps_msg_pools (MEM_DESC *hdrs, MEM_DESC *elements);

/* Returns the message construction pool descriptors. */

typedef enum {
	R_NO_ERROR,	/* No error. */
	R_TOO_SHORT,	/* Submessage too short. */
	R_INV_SUBMSG,	/* Invalid submessage. */
	R_INV_QOS,	/* Invalid inline QoS contents. */
	R_NO_BUFS,	/* Out of receive buffers. */
	R_UNKN_DEST,	/* Unknown destination. */
	R_INV_MARSHALL	/* Invalid marshalling type. */
} RcvError_t;

void rtps_rx_error (RcvError_t e, unsigned char *cp, size_t len);

/* Signal an RTPS reception error. */

void rtps_relay_add (Participant_t *pp);

/* Add a local relay to the set of local relays in a domain.
   Updates all the proxy context locators to take the relays into account. */

void rtps_relay_remove (Participant_t *pp);

/* Remove a local relay from the set of local relays in a domain.
   Updates all the proxy context locators as a result of this. */

void rtps_relay_update (Participant_t *pp);

/* Update a relay from the set of local relays in a domain since its locators
   were updated.  Updates all the proxy context locators as a result of this. */

void rtps_peer_reader_crypto_set (Writer_t *rp, DiscoveredReader_t *dw, unsigned h);

/* Set a peer reader crypto handle. */

void rtps_peer_writer_crypto_set (Reader_t *rp, DiscoveredWriter_t *dw, unsigned h);

/* Set a peer writer crypto handle. */

unsigned rtps_peer_reader_crypto_get (Writer_t *rp, DiscoveredReader_t *dw);

/* Get a peer reader crypto handle. */

unsigned rtps_peer_writer_crypto_get (Reader_t *rp, DiscoveredWriter_t *dw);

/* Get a peer writer crypto handle. */


/* 6. Debug functions.
   ------------------- */

void rtps_pool_dump (size_t sizes []);

/* Display RTPS pool statistics. */

void rtps_endpoints_dump (void);

/* Display all endpoints. */

void rtps_cache_dump (Endpoint_t *e);

/* Display the cache contents of an endpoint. */

void rtps_proxy_dump (Endpoint_t *e);

/* Display the proxy contents of an endpoint. */

void rtps_proxy_restart (Endpoint_t *e);

/* Restart a proxy of an endpoint. */

void rtps_receiver_dump (void);

/* Display the contents of the RTPS receiver. */

void rtps_transmitter_dump (void);

/* Display the contents of the RTPS transmitter. */

int rtps_endpoint_assert (LocalEndpoint_t *e);

/* Assert the validity of an endpoint and return the result as an error code. */


/* 6. On-line trace functionality.
   ------------------------------- */

int rtps_trace_set (Endpoint_t *r, unsigned mode);

/* Update the tracing mode of either a single endpoint (r != 
   DDS_TRACE_ALL_ENDPOINTS) or all endpoints (r == DDS_TRACE_ALL_ENDPOINTS). 
   If mode == DDS_TRACE_MODE_TOGGLE, the tracing mode is simply toggled. */

int rtps_trace_get (Endpoint_t *r, unsigned *mode);

/* Get the tracing mode of an endpoint. */

void rtps_dtrace_set (unsigned mode);

/* Update the default tracing mode of new endpoints.  If mode is set to
   DDS_TRACE_MODE_TOGGLE, the default tracing mode is toggled. */

void rtps_dtrace_get (unsigned *mode);

/* Get the default trace mode. */

#endif /* !__rtps_h_ */

