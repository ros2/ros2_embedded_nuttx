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

/* rtps_data_h -- Defines common data types as used between the various RTPS
                  subsystems. */

#ifndef __rtps_data_h_
#define __rtps_data_h_

#include <stdint.h>
#include "sys.h"
#include "db.h"
#include "rtps.h"
#include "cache.h"

/* SubmessageId types: */
#define	ST_PAD			0x01	/* Pad */
#define	ST_ACKNACK		0x06	/* AckNack */
#define	ST_HEARTBEAT		0x07	/* Heartbeat */
#define	ST_GAP			0x08	/* Gap */
#define	ST_INFO_TS		0x09	/* InfoTimestamp */
#define	ST_INFO_SRC		0x0c	/* InfoSource */
#define	ST_INFO_REPLY_IP4	0x0d	/* InfoReplyIp4 */
#define	ST_INFO_DST		0x0e	/* InfoDestination */
#define	ST_INFO_REPLY		0x0f	/* InfoReply */
#define	ST_NACK_FRAG		0x12	/* NackFrag */
#define	ST_HEARTBEAT_FRAG	0x13	/* HeartbeatFrag */
#define	ST_DATA			0x15	/* Data */
#define	ST_DATA_FRAG		0x16	/* DataFrag */
#define	ST_SECURE		0x30	/* Secure */

typedef uint32_t FragmentNumber_t;

typedef struct msg_header_st {
	ProtocolId_t		protocol;
	ProtocolVersion_t	version;
	VendorId_t		vendor_id;
	GuidPrefix_t		guid_prefix;
} MsgHeader;

typedef unsigned char SubmessageKind;

#define	SMF_ENDIAN	0x01
#define	SMF_BIG		ENDIAN_BIG
#define	SMF_LITTLE	ENDIAN_LITTLE
#define	SMF_CPU_ENDIAN	ENDIAN_CPU
#define	SMF_FINAL	0x02	/* In AckNack, Heartbeat */
#define	SMF_INLINE_QOS	0x02	/* In Data, DataFrag */
#define	SMF_MULTICAST	0x02	/* In InfoReply */
#define	SMF_INVALIDATE	0x02	/* In InfoTimestamp */
#define	SMF_SINGLE	0x02	/* In Secure */
#define	SMF_LIVELINESS	0x04	/* In Heartbeat */
#define	SMF_KEY_DF	0x04	/* In DataFrag */
#define	SMF_DATA	0x04	/* In Data */
#define	SMF_KEY		0x08	/* In Data */

#define	SMF_VENDOR	0x80	/* Vendor-specific submessage. */

typedef struct submsg_header_st {
	SubmessageKind		id;
	unsigned char		flags;
	uint16_t		length;
} SubmsgHeader;

typedef struct fragnr_set_st {
	FragmentNumber_t	base;
	uint32_t		numbits;
	uint32_t		bitmap [1];	/* Up to 8 -> 256 bits. */
} FragmentNumberSet;

typedef struct acknack_smsg_st {
	EntityId_t		reader_id;
	EntityId_t		writer_id;
	SequenceNumberSet	reader_sn_state;
	Count_t			count;
#ifdef RTPS_PROXY_INST_TX
	Count_t			instance_id;	/* Optional. */
#endif
} AckNackSMsg;

#ifdef RTPS_PROXY_INST_TX
#define MIN_ACKNACK_SIZE	(sizeof (AckNackSMsg) - sizeof (uint32_t))
#else
#define MIN_ACKNACK_SIZE	sizeof (AckNackSMsg)
#endif
#define MAX_ACKNACK_SIZE	(sizeof (AckNackSMsg) + sizeof (uint32_t) * 7)

typedef struct data_smsg_st {
	uint16_t		extra_flags;
	uint16_t		inline_qos_ofs;
	EntityId_t		reader_id;
	EntityId_t		writer_id;
	SequenceNumber_t	writer_sn;

	/* Optional fields:
		1. SMF_INLINE_QOS -> inline_qos: { PARAMETER }
		2. SMF_DATA -> serialized_payload: unsigned char []
		3. SMF_KEY  -> serialized payload: unsigned char [] */
} DataSMsg;

typedef struct data_frag_smsg_st {
	uint16_t		extra_flags;
	uint16_t		inline_qos_ofs;
	EntityId_t		reader_id;
	EntityId_t		writer_id;
	SequenceNumber_t	writer_sn;
	FragmentNumber_t	frag_start;
	uint16_t		num_fragments;
	uint16_t		frag_size;
	uint32_t		sample_size;

	/* Optional fields:
		1. SMF_INLINE_QOS -> inline_qos: { PARAMETER }
		2. SMF_DATA -> serialized_payload: unsigned char []
		3. SMF_KEY  -> serialized payload: unsigned char [] */
} DataFragSMsg;

typedef struct gap_smsg_st {
	EntityId_t		reader_id;
	EntityId_t		writer_id;
	SequenceNumber_t	gap_start;
	SequenceNumberSet	gap_list;
} GapSMsg;

#define	MIN_GAP_SIZE	(sizeof (GapSMsg) - sizeof (uint32_t))
#define	MAX_GAP_SIZE	(sizeof (GapSMsg) + sizeof (uint32_t) * 7)

typedef struct heartbeat_smsg_st {
	EntityId_t		reader_id;
	EntityId_t		writer_id;
	SequenceNumber_t	first_sn;
	SequenceNumber_t	last_sn;
	Count_t			count;
#ifdef RTPS_PROXY_INST_TX
	Count_t			instance_id;	/* Optional */
#endif
} HeartbeatSMsg;

#ifdef RTPS_PROXY_INST_TX
#define	DEF_HB_SIZE	(sizeof (HeartbeatSMsg) - sizeof (uint32_t))
#define	MAX_HB_SIZE	sizeof (HeartbeatSMsg)
#else
#define	DEF_HB_SIZE	sizeof (HeartbeatSMsg)
#endif

typedef struct heartbeat_frag_smsg_st {
	EntityId_t		reader_id;
	EntityId_t		writer_id;
	SequenceNumber_t	writer_sn;
	FragmentNumber_t	last_frag;
	Count_t			count;
} HeartbeatFragSMsg;

typedef struct info_destination_smsg_st {
	GuidPrefix_t		guid_prefix;
} InfoDestinationSMsg;

typedef struct info_reply_smsg_st {
	LocatorList_t		uc_locators;
	LocatorList_t 		mc_locators; /* Optional (SMF_Multicast set) */
} InfoReplySMsg;

typedef struct info_source_smsg_st {
	int32_t			unused;
	ProtocolVersion_t	version;
	VendorId_t		vendor;
	GuidPrefix_t		guid_prefix;
} InfoSourceSMsg;

typedef struct info_timestamp_smsg_st {
	int32_t			seconds;	/* Only if !SMF_INVALIDATE */
	uint32_t		fraction;	/* Only if !SMF_INVALIDATE */
} InfoTimestampSMsg;

#define MIN_NACKFRAG_SIZE	sizeof (NackFragSMsg)
#define MAX_NACKFRAG_SIZE	(sizeof (NackFragSMsg) + sizeof (uint32_t) * 7)

typedef struct nack_frag_smsg_st {
	EntityId_t		reader_id;
	EntityId_t		writer_id;
	SequenceNumber_t	writer_sn;
	FragmentNumberSet	frag_nr_state;
	Count_t			count;
} NackFragSMsg;

typedef struct secure_smsg_st {
	uint32_t		transform_kind;	  /* Type of transformation. */
	unsigned char		transform_id [8]; /* Transformation Id. */
} SecureSMsg;

typedef struct submessage_st {
	SubmsgHeader		shdr;
	union {
	  AckNackSMsg		acknack;
	  DataSMsg		data;
	  DataFragSMsg		data_frag;
	  GapSMsg		gap;
	  HeartbeatSMsg		heartbeat;
	  HeartbeatFragSMsg	heartbeat_frag;
	  InfoDestinationSMsg	info_dest;
	  InfoReplySMsg		info_reply;
	  InfoSourceSMsg	info_source;
	  InfoTimestampSMsg	info_timestamp;
	  NackFragSMsg		nack_frag;
	  SecureSMsg		secure;
	  /* PAD : no data. */
	} d;
} Submessage;

typedef struct message_st {
	MsgHeader	header;
	Submessage	submsg [1];
} Message;

#define	MAX_SEID_OFS	ST_DATA_FRAG

extern int rtps_seid_offsets [];


/* Message buffer data structures.
   -------------------------------
   To transport data efficiently between the various DDS layers and to avoid
   copying data when possible the following data representation was chosen.
   An RTPS message consists of a single RMBUF, containing the RTPS message
   header and a pointer to a list of submessage or data reference containers.
   Each submessage is stored in an RME. The first RME of a submessage contains
   the header of the submessage.  If the data of the submessage is small enough to
   be stored in the RME itself, this should be done.
   If the data chunk is too large, it is typically stored separately in an
   RMDATA block, with the RME referencing the block.
   In both cases, the data/length fields point to the actual data area.
   Since multiple RMEs can point to the same RMDATA block, the number of
   users referencing the data must be kept correct at all times.
*/

/* Note 1: don't change the order of the following data structure fields, since
	   some lower transport mechanisms depend on them! 
   Note 2: These numbers are not random. They are derived from the minimal size
	   of a header consisting of non-alive data submessage containing hash
	   key, status info, end pid, and up to MAX_DW_DEST directed write
	   destinations. */

#if WORDSIZE == 64
#define	MAX_ELEMENT_DATA	128U
#else
#define	MAX_ELEMENT_DATA	96U
#endif

/* Note: this size was chosen to allow an additional embedded RME with limited
         size able to carry either a utility structure, such as a notification
	 context, or contain an additional indirect data reference (via db/data/
	 length). */

typedef struct rtps_msg_buf_st RMBUF;
typedef struct rtps_msg_element_buf_st RME;
typedef struct rtps_msg_ref_st RMREF;

#define NT_CACHE_FREE	0	/* Cache change disposal. */
#define	NT_DATA_FREE	1	/* Data buffer disposal. */
#define	NT_STR_FREE	2	/* String disposal. */

typedef struct rtps_nofif_data_st {
	unsigned	type;	/* Type of notification. */
	void		*arg;	/* Arguments data. */
} NOTIF_DATA;

#define	RME_HDRSIZE	(sizeof (RME) - MAX_ELEMENT_DATA)

#define	RME_SWAP	1		/* Data needs to be swapped. */
#define	RME_CONTAINED	2		/* Not separately allocated. */
#define	RME_HEADER	4		/* Header is used if set. */
#define	RME_NOTIFY	8		/* Notify function should be called.
					   NOTIF_DATA is stored in d []. */
#define	RME_MCAST	16		/* Message requires multicast.
					   Valid in first element only! */
#define	RME_USER	32		/* User data - idem. */
#define	RME_TRACE	64		/* Trace message - idem. */

struct rtps_msg_element_buf_st { /* 64-bit: 160(32+128), 32-bit: 116(20+96). */
	RME		*next;		/* Next element. */
	unsigned char	*data;		/* Data chunk pointer. */
	DB		*db;		/* Data block pointer (if non-NULL). */
	uint16_t	length;		/* Length of data chunk. */
	unsigned char	pad;		/* Amount the data was padded. */
	unsigned char 	flags;		/* Various message element flags. */
	SubmsgHeader	header;		/* Submessage header. */
	unsigned char	d [MAX_ELEMENT_DATA]; /* Optional data storage. */
};

struct rtps_msg_buf_st {
	RMBUF		*next;		/* Next message. */
	RME		*first;		/* First message element. */
	RME		*last;		/* Last message element. */
	size_t		size;		/* Message size (in bytes). */
	unsigned	prio;		/* Message priority. */
	unsigned	users;		/* Message use count. */
	MsgHeader	header;		/* Message header. */
	RME		element;	/* Single element (always needs 1). */
};

struct rtps_msg_ref_st {
	RMREF		*next;		/* Next message reference. */
	RMBUF		*message;	/* Message pointer. */
};

extern size_t		rtps_max_msg_size;	/* RTPS maximum message size. */

extern size_t		rtps_frag_size;		/* Size of RTPS fragments. */
extern unsigned		rtps_frag_burst;	/* # of fragments w.o. delay. */
extern unsigned		rtps_frag_delay;	/* Delay between bursts (us). */

int rtps_reader_cache_get (Reader_t *r,
		           unsigned *nchanges,
		           Change_t *cache_entry [],
		           int      rem);

/* Get (i.e. read or take) a number of cache entries from a reader.
   The *nchanges parameter specifies how many entries may be handled.
   On return, *nchanges will be set to the actual number of changes delivered.
   Note that the reader must be capable of handling cache entries correctly
   and that the appropriate rtps_reader_cache_*() functions are used properly.*/

int rtps_reader_cache_done (Reader_t r,
			    unsigned nchanges,
			    Change_t *cache_entry []);

/* The cache entries are processed completely and can finally be disposed. */


extern ProtocolId_t 	 rtps_protocol_id;
extern ProtocolVersion_t rtps_protocol_version;
extern VendorId_t 	 rtps_vendor_id;

#endif /* !__rtps_data_h_ */

