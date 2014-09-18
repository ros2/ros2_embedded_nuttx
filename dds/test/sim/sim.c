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

/* sim.c -- Simulation function library. */

#include <stdio.h>
#include "error.h"
#include "pid.h"
#include "set.h"
#include "rtps_mux.h"
#include "rtps_udpv4.h"
#include "sim.h"

/* Support functions for Received message injection: */
/* ------------------------------------------------- */

static Participant_t	*pp;
static Endpoint_t	*src, *dst;
static int		mcast;
static RMBUF		*rx_mp;
static RMBUF		*tx_mp;
static RME		*tx_mep;
static MEM_DESC		mhdr_pool, melem_pool;
static Locator_t	locator;

/* sim_new_msg -- Start a new message between the two endpoints. */

void sim_new_msg (Participant_t *p, Endpoint_t *s, Endpoint_t *d, int mc)
{
	Domain_t	*dp;

	if (!mhdr_pool)
		rtps_msg_pools (&mhdr_pool, &melem_pool);
	if (rx_mp)
		sim_rx_msg ();
	pp = p;
	src = s;
	dst = d;
	mcast = mc;
	dp = domain_lookup (0);
	if (mc)
		locator = dp->participant.p_def_mcast->data->locator;
	else
		locator = dp->participant.p_def_ucast->data->locator;
}

/* sim_add_element -- Add a message element to an existing message. */

static RME *sim_add_element (void)
{
	RME	*mep;

	if (!rx_mp) {
		rx_mp = mds_pool_alloc (mhdr_pool);
		if (!rx_mp)
			return (NULL);

		rx_mp->next = NULL;
		mep = &rx_mp->element;
		mep->flags = RME_CONTAINED;
		rx_mp->first = mep;

		/* Initialize message header. */
		protocol_set (rx_mp->header.protocol);
		version_init (rx_mp->header.version);
		vendor_id_init (rx_mp->header.vendor_id);
		memcpy (&rx_mp->header.guid_prefix,
			&pp->p_guid_prefix,
			sizeof (GuidPrefix_t));
		mep->next = NULL;
	}
	else {
		mep = mds_pool_alloc (melem_pool);
		if (!mep)
			return (NULL);

		rx_mp->last->next = mep;
		mep->flags = 0;
		mep->next = NULL;
	}
	rx_mp->last = mep;
	return (mep);
}

/* sim_add_info_ts -- Add an INFO_TIMESTAMP message element to the message. */

void sim_add_info_ts (Time_t *time, int mcast)
{
	RME			*mep;
	InfoTimestampSMsg	*tp;

	mep = sim_add_element ();
	if (!mep)
		fatal_printf ("sim_add_info_ts: out of submessage elements!");

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_INFO_TS;
	mep->header.flags = SMF_CPU_ENDIAN;

	/* Add extra INFO_TIMESTAMP submessage fields. */
	mep->data = mep->d;
	tp = (InfoTimestampSMsg *) mep->d;
	mep->length = mep->header.length = sizeof (InfoTimestampSMsg);
	time2ftime (time, &tp->timestamp);
}

/* sim_add_data -- Add a DATA message element to the message. */

void sim_add_data (SequenceNumber_t *snr,
		   KeyHash_t        *hp,
		   StatusInfo_t     *sp,
		   String_t         *kp,
		   unsigned char    *dp,
		   unsigned         length)
{
	RME		*mep;
	DataSMsg	*dsp;
	unsigned	remain, ofs;
	ssize_t		n;

	mep = sim_add_element ();
	if (!mep)
		fatal_printf ("sim_add_data: out of submessage elements!");

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_DATA;
	mep->header.flags = SMF_CPU_ENDIAN;

	/* Add extra DATA submessage fields. */
	mep->data = mep->d;
	dsp = (DataSMsg *) mep->d;
	mep->length = sizeof (DataSMsg);
	dsp->extra_flags = 0;
	dsp->inline_qos_ofs = sizeof (DataSMsg) - 4;
	dsp->reader_id = dst->entity_id;
	dsp->writer_id = src->entity_id;
	dsp->writer_sn = *snr;
	remain = MAX_ELEMENT_DATA - sizeof (DataSMsg);
	ofs = sizeof (DataSMsg);

	/* Add extra InlineQos parameters if these are needed. */
	if (hp || sp) {
		if (hp) {
			n = pid_add (mep->d + ofs, PID_KEY_HASH, hp);
			if (n <= 0)
				fatal_printf ("sim_add_data: Can't add hash!");

			ofs += n;
			remain -= n;
		}
		if (sp) {
			n = pid_add (mep->d + ofs, PID_STATUS_INFO, sp);
			if (n <= 0)
				fatal_printf ("sim_add_data: Can't add status info!");

			ofs += n;
			remain -= n;
		}
		n = pid_finish (mep->d + ofs);
		if (n <= 0)
			fatal_printf ("sim_add_data: Can't add sentinel!");

		ofs += n;
		remain -= n;

		mep->header.flags |= SMF_INLINE_QOS;
	}
	mep->length = mep->header.length = ofs;
	if (dp) {

		/* Add serializedPayload SubmessageElement data. */
		mep->header.flags |= SMF_DATA;

		/* Check if change data fits directly in rest of buffer. */
		if (length > remain)
			fatal_printf ("sim_add_data: no room for data!");

		/* Just copy change data. */
		memcpy (mep->d + ofs, dp, length);
		mep->length += length;
	}
	else if (kp) {
	
		/* Add Key SubmessageElement data. */
		mep->header.flags |= SMF_KEY;

		/* Check if key data fits directly in rest of buffer. */
		if (str_len (kp) > remain)
			fatal_printf ("sim_add_data: no room for key!");

		/* Just copy change data. */
		memcpy (mep->d + ofs, str_ptr (kp), str_len (kp));
		mep->length += str_len (kp);
	}
}

/* sim_add_info_dest -- Add an INFO_DESTINATION message element to the
			message. */

void sim_add_info_dest (GuidPrefix_t *prefix)
{
	Domain_t		*dp;
	RME			*mep;
	InfoDestinationSMsg	*idp;

	if (!prefix) {
		dp = domain_lookup (0);
		prefix = &dp->participant.p_guid_prefix;
	}
	mep = sim_add_element ();
	if (!mep)
		fatal_printf ("sim_add_info_dest: out of submessage elements!");

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_INFO_DST;
	mep->header.flags = SMF_CPU_ENDIAN;

	/* Add extra INFO_DESTINATION submessage fields. */
	mep->data = mep->d;
	idp = (InfoDestinationSMsg *) mep->d;
	mep->length = mep->header.length = sizeof (InfoDestinationSMsg);
	idp->guid_prefix = *prefix;
}

/* sim_add_gap -- Add a GAP message element to the message. */

void sim_add_gap (SequenceNumber_t *start,
		  SequenceNumber_t *base,
		  unsigned         nbits,
		  uint32_t         *bits)
{
	fatal_printf ("sim_add_gap: function not yet implemented!");
}

/* sim_add_heartbeat -- Add a HEARTBEAT message element to the message. */

void sim_add_heartbeat (unsigned         flags,
			SequenceNumber_t *first,
			SequenceNumber_t *last,
			unsigned         count)
{
	RME		*mep;
	HeartbeatSMsg	*hp;

	mep = sim_add_element ();
	if (!mep)
		fatal_printf ("sim_add_heartbeat: out of submessage elements!");

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_HEARTBEAT;
	mep->header.flags = SMF_CPU_ENDIAN;

	/* Add extra HEARTBEAT submessage fields. */
	mep->data = mep->d;
	hp = (HeartbeatSMsg *) mep->d;
	mep->length = mep->header.length = sizeof (HeartbeatSMsg);
	hp->reader_id = dst->entity_id;
	hp->writer_id = src->entity_id;
	hp->first_sn = *first;
	hp->last_sn = *last;
	hp->count = count;
}

/* sim_add_acknack -- Add an ACKNACK message element to the message. */

void sim_add_acknack (int              final,
		      SequenceNumber_t *base,
		      unsigned         nbits,
		      uint32_t         *bits,
		      unsigned         count)
{
	RME		*mep;
	AckNackSMsg	*dp;
	unsigned	n;

	/* Always preceed with INFO_DESTINATION. */
	sim_add_info_dest (NULL);

	/* Add new submessage element. */
	mep = sim_add_element ();
	if (!mep)
		fatal_printf ("sim_add_acknack: out of submessage elements!");

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_ACKNACK;
	mep->header.flags = SMF_CPU_ENDIAN;
	if (final)
		mep->header.flags |= SMF_FINAL;

	/* Add extra ACKNACK submessage fields. */
	mep->data = mep->d;
	dp = (AckNackSMsg *) mep->d;
	n = ((nbits + 31) >> 5) << 2;	/* # of bytes in bitmap. */
	mep->length = mep->header.length = sizeof (AckNackSMsg) + n - 4;
	dp->reader_id = src->entity_id;
	dp->writer_id = dst->entity_id;
	dp->reader_sn_state.base = *base;
	dp->reader_sn_state.numbits = nbits;
	memcpy (dp->reader_sn_state.bitmap, bits, n);
	dp->reader_sn_state.bitmap [n >> 2] = count;
}

/* sim_rx_msg -- Inject the constructed message in the receiver logic. */

void sim_rx_msg (void)
{
	if (rx_mp) {
		rtps_udpv4_receive_add (&locator, rx_mp);
		rx_mp = NULL;
		DDS_schedule (0);
	}
}

/* sim_create_bitmap -- Utility function to make it easier to create a bitmap
			as used in ACKNACK and GAP messages. */

void sim_create_bitmap (unsigned base_l, unsigned nbits, const char *mask,
			SequenceNumber_t *base, uint32_t bits [8])
{
	unsigned	i;

	base->high = 0;
	base->low = base_l;
	memset (bits, 0, ((nbits + 31) >> 5) << 2);
	for (i = 0; i < nbits; i++) {
		if (!mask [i])
			return;

		if (mask [i] == '1')
			SET_ADD (bits, i);
	}
}


/* Support functions for Transmitted message parsing: */
/* -------------------------------------------------- */

static void sim_wait_msg (unsigned sleep_time)
{
	int 	error;

	while (!tx_mp) {
		error = rtps_udpv4_send_next (&locator, &tx_mp);
		if (error)
			DDS_schedule (sleep_time);
	}
}

static const char *type_str [] = {
	"?(0)", "PAD", "?(2)", "?(3)",
	"?(4)", "?(5)", "ACKNACK", "HEARTBEAT",
	"GAP", "INFO_TS", "?(10)", "?(11)",
	"INFO_SRC", "INFO_REPLY_V4", "INFO_DEST", "INFO_REPLY",
	"?(16)", "?(17)", "NACK_FRAG", "HEARTBEAT_FRAG",
	"?(20)", "DATA", "DATA_FRAG"
};

static RME *sim_wait_mep (unsigned sleep_time)
{
	RME	*mep;

	printf ("<<sim_wait_mep>>");
	for (;;) {
		/* Get a new Message element. */
		if (!tx_mp) {
			sim_wait_msg (sleep_time);
			if (tx_mp)
				tx_mep = tx_mp->first;
			else
				continue;
		}

		/* tx_mep now points to a new element. */
		mep = tx_mep;
		tx_mep = tx_mep->next;
		if (!tx_mep) {
			rtps_free_messages (tx_mp);
			tx_mp = NULL;
		}
		if ((mep->flags & RME_HEADER) != 0)
			printf ("Sim: Rx(%s)\r\n", type_str [mep->header.id]);
		else
			printf ("Sim!\r\n");
		return (mep);
	}
}

/* sim_wait_data -- Wait until a DATA message element is sent with the given
		    sequence number. */

RME *sim_wait_data (SequenceNumber_t *snr, DataSMsg **info)
{
	fatal_printf ("sim_wait_data: function not yet implemented!");
}

/* sim_wait_info_dest -- Wait until an INFO_DESTINATION message element is sent
			 with the prefix. */

RME *sim_wait_info_dest (GuidPrefix_t *prefix)
{
	fatal_printf ("sim_wait_info_dest: function not yet implemented!");
}

/* sim_wait_gap -- Wait until a GAP message element is sent with the given start
		   and base sequence numbers. */

RME *sim_wait_gap (SequenceNumber_t *start,
		   SequenceNumber_t *base,
		   GapSMsg          **info)
{
	fatal_printf ("sim_wait_gap: function not yet implemented!");
}

/* sim_wait_heartbeat -- Wait until a HEARTBEAT message element is sent with the
			 given first and last sequence numbers. */

RME *sim_wait_heartbeat (SequenceNumber_t *first,
			 SequenceNumber_t *last,
			 HeartbeatSMsg    **info)
{
	Locator_t	loc;
	RME		*mep;
	HeartbeatSMsg	*hp;

	for (;;) {
		mep = sim_wait_mep (200);
		if (mep->header.id != ST_HEARTBEAT)
			continue;

		hp = (HeartbeatSMsg *) mep->d;
		if (first && !SEQNR_EQ (*first, hp->first_sn))
			continue;

		if (last && !SEQNR_EQ (*last, hp->last_sn))
			continue;

		if (info)
			*info = hp;
		return (mep);
	}
}

/* sim_wait_acknack -- Wait until an ACKNACK message element is sent with the
		       given base sequence number and # of bits specified. */

RME *sim_wait_acknack (SequenceNumber_t *base,
		       unsigned         nbits,
		       AckNackSMsg      **info)
{
	Locator_t	loc;
	RME		*mep;
	AckNackSMsg	*ap;

	for (;;) {
		mep = sim_wait_mep (200);
		if (mep->header.id != ST_ACKNACK)
			continue;

		ap = (AckNackSMsg *) mep->d;
		if (base &&
		    (!SEQNR_EQ (*base, ap->reader_sn_state.base) ||
		     nbits != ap->reader_sn_state.numbits))
			continue;

		if (info)
			*info = ap;
		return (mep);
	}
}

