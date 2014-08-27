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

/* rtps_msg.h -- Defines the interface to RTPS message construction and parsing
                 functionality. */

#ifndef __rtps_msg_h_
#define __rtps_msg_h_

#include "rtps_cfg.h"
#include "rtps_data.h"
#include "rtps_priv.h"

int rtps_msg_add_data (RemReader_t        *rrp,
		       const GuidPrefix_t *prefix,
		       const EntityId_t   *eid,
		       Change_t           *cp,
		       const HCI          hci,
		       int                push
#ifdef RTPS_FRAGMENTS
		     , unsigned           first,
		       unsigned           *nfrags
#endif
			                         );

/* Add either a DATA submessage element or multiple DATAFRAG submessage
   elements (depending on fragment size) to an existing message. */

int rtps_msg_add_gap (RemReader_t              *rrp,
		      const DiscoveredReader_t *reader,
		      const SequenceNumber_t   *start,
		      const SequenceNumber_t   *base,
		      unsigned                 n_bits,
		      uint32_t                 *bits,
		      int                      push);

/* Add a GAP submessage element to an existing message. */

int rtps_msg_add_heartbeat (RemReader_t              *rrp,
			    const DiscoveredReader_t *reader,
			    unsigned                 flags,
			    const SequenceNumber_t   *min_seqnr,
			    const SequenceNumber_t   *max_seqnr);

/* Add a HEARTBEAT submessage to an RTPS message. */

int rtps_msg_add_acknack (RemWriter_t              *rwp,
			  const DiscoveredWriter_t *writer,
			  int                      final,
			  const SequenceNumber_t   *base,
			  unsigned                 nbits,
			  const uint32_t           bitmaps []);

/* Add an ACKNACK submessage to an RTPS message. */

int rtps_msg_add_heartbeat_frag (RemReader_t              *rrp,
				 const DiscoveredReader_t *reader,
				 const SequenceNumber_t   *seqnr,
				 unsigned                 last_frag);

/* Add a HEARTBEAT_FRAG submessage to an RTPS msg. */

int rtps_msg_add_nack_frag (RemWriter_t              *rwp,
			    const DiscoveredWriter_t *writer,
			    const SequenceNumber_t   *seqnr,
			    FragmentNumber_t         base,
			    unsigned                 nbits,
			    const uint32_t           bitmaps []);

/* Add a NACK_FRAG submessage to an RTPS msg. */

#define	SUBMSG_DATA(mep,ofs,len,buf)	(mep->db) ?			      \
					   submsg_data_ptr(mep,ofs,len,buf) : \
					   &mep->d [ofs]

#define	SWAP_UINT16(s,t)	memcswap16(&t,&s); s = t
#define	SWAP_UINT32(u,t)	memcswap32(&t,&u); u = t
#define	SWAP_SEQNR(snr,t)	SWAP_UINT32(snr.high,t); SWAP_UINT32(snr.low,t)
#define	SWAP_TS(ts,t)		SWAP_UINT32((ts).seconds,t); SWAP_UINT32((ts).fraction,t)


void *submsg_data_ptr (RME      *mep,	/* Subelement structure. */
           	       unsigned ofs,	/* Offset in subelement data. */
           	       unsigned len,	/* Length of data region. */
           	       void     *buf);	/* Buffer when scattered. */

/* Return a linear submessage data field from a possibly scattered submessage
   element.  If the specified range is already linear, a straight pointer to
   the original subelement field is returned.  If it is scattered, the data is
   copied to the given data buffer and a pointer to that buffer is returned. */

void rtps_receive (unsigned        id,		/* Participant id. */
		   RMBUF           *msgs,	/* RTPS messages. */
		   const Locator_t *src);	/* Source locator. */

/* Parse a received RTPS message from some transport system. */

#endif /* !__rtps_msg_h_ */

