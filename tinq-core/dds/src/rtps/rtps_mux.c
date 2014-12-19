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

/* rtps_mux.c -- RTPS transport multiplexer.  Makes sure that messages can be
                 transported over different kinds of media, such as:

			- IPv4
			- IPv6
			- Shared memory
			- Unix sockets
			  etc.

		  To simplify the interface to the various transport systems,
		  all message transfers are done using the standardized message
		  format.
 */

#include <stdio.h>
#include "pool.h"
#include "log.h"
#include "error.h"
#include "config.h"
#ifdef DDS_FORWARD
#include "rtps_fwd.h"
#endif
#ifdef DDS_TCP
#include "ri_tcp.h"
#endif
#include "rtps_mux.h"

#define	MAX_TRANSPORTS	16	/* Should be plenty! */

static RMRXF		rtps_mux_rxfct;		/* Receiver function. */
static RMNOTF		rtps_mux_notify;	/* Notification function. */
static MEM_DESC		rtps_mux_msg_md;	/* Descriptor for messages. */
static MEM_DESC		rtps_mux_el_md;		/* Descriptor for elements. */
static MEM_DESC		rtps_mux_ref_md;	/* Descriptor for references. */
static const RTPS_TRANSPORT *rtps_transports [MAX_TRANSPORTS]; /* Transports. */
static const void	*rtps_transport_pars [MAX_TRANSPORTS]; /* Parameters. */

unsigned		rtps_forward;		/* Forwarding mode. */
LocatorKind_t		rtps_mux_mode = -1;	/* Default transport medium. */
const RTPS_UDP_PARS	rtps_udp_def_pars = {	/* Default RTPS/UDP pars. */
				RTPS_UDP_PB,
				RTPS_UDP_DG,
				RTPS_UDP_PG, 
				RTPS_UDP_D0,
				RTPS_UDP_D1,
				RTPS_UDP_D2,
				RTPS_UDP_D3
			};

/* rtps_mux_init -- Initialize the RTPS mux with the parameters needed for the
		    various transport protocol handlers. */

void rtps_mux_init (RMRXF    rxfct,
		    RMNOTF   notifyfct,
		    MEM_DESC msg_hdr,
		    MEM_DESC msg_elem,
		    MEM_DESC msg_ref)
{
	const RTPS_TRANSPORT	*tp;
	unsigned		i;

	/* Remember parameters for transport protocols. */
	rtps_mux_rxfct = rxfct;
	rtps_mux_notify = notifyfct;
	rtps_mux_msg_md = msg_hdr;
	rtps_mux_el_md = msg_elem;
	rtps_mux_ref_md = msg_ref;

#ifdef DDS_FORWARD
	rtps_forward = config_get_number (DC_Forward, 0);
	if (rtps_forward)
		rtps_mux_rxfct = rfwd_init (rxfct, rtps_mux_msg_md, rtps_mux_el_md);
#endif

	/* Initialize all registered transports. */
	for (i = LOCATOR_KIND_RESERVED; i < MAX_TRANSPORTS; i++)
		if ((tp = rtps_transports [i]) != NULL)
			(*tp->init_fct) (rxfct,
					 rtps_mux_msg_md,
					 rtps_mux_el_md);
}

/* rtps_mux_final -- Finalize the RTPS transport multiplexer. */

void rtps_mux_final (void)
{
	const RTPS_TRANSPORT	*tp;
	unsigned		i;

#ifdef DDS_FORWARD
	if (rtps_forward)
		rfwd_final ();
#endif
	/* Finalize all registered transports. */
	for (i = LOCATOR_KIND_RESERVED; i < MAX_TRANSPORTS; i++)
		if ((tp = rtps_transports [i]) != NULL)
			(*tp->final_fct) ();
}

/* rtps_transport_add -- Add a transport subsystem to the RTPS multiplexer. */

int rtps_transport_add (const RTPS_TRANSPORT *tp)
{
	int	error;

	if (tp->kind >= MAX_TRANSPORTS)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (rtps_transports [tp->kind])
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	rtps_transports [tp->kind] = tp;
	if (rtps_mux_msg_md)	/* Mux is initialized? */
		error = (*tp->init_fct) (rtps_mux_rxfct,
					 rtps_mux_msg_md,
					 rtps_mux_el_md);
	else
		error = DDS_RETCODE_OK;
	if (!error && rtps_transport_pars [tp->kind])
		error = (*tp->pars_set_fct) (tp->kind, rtps_transport_pars [tp->kind]);
	return (error);
}

/* rtps_transport_remove -- Remove a transport subsystem from the RTPS multiplexer. */

void rtps_transport_remove (const RTPS_TRANSPORT *tp)
{
	if (tp->kind >= MAX_TRANSPORTS || !rtps_transports [tp->kind])
		return;

	(*tp->loc_upd_fct) (tp->kind, NULL, 0);
	(*tp->loc_upd_fct) (tp->kind, NULL, 1);
	(*tp->final_fct) ();
	rtps_transports [tp->kind] = NULL;
}

/* rtps_transport_lookup -- Return a transport context for the given locator
			    kind or return NULL and set *error if not found. */

const RTPS_TRANSPORT *rtps_transport_lookup (LocatorKind_t kind, int *error)
{
	const RTPS_TRANSPORT	*tp = NULL;

	if (kind >= MAX_TRANSPORTS) {
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	if ((tp = rtps_transports [kind]) == NULL)
		*error = DDS_RETCODE_PRECONDITION_NOT_MET;
	else
		*error = DDS_RETCODE_OK;
	return (tp);
}

/* rtps_transport_locators -- Retrieve protocol specific locators for the
			      specified use case in uc/mc. */

void rtps_transport_locators (DomainId_t    domain_id,
			      unsigned      participant_id,
			      RTPS_LOC_TYPE type,
			      LocatorList_t *uc,
			      LocatorList_t *mc,
			      LocatorList_t *dst)
{
	const RTPS_TRANSPORT	*tp;
	unsigned		i;

	if (uc)
		locator_list_init (*uc);
	if (mc)
		locator_list_init (*mc);
	for (i = LOCATOR_KIND_RESERVED; i < MAX_TRANSPORTS; i++)
		if ((tp = rtps_transports [i]) != NULL)
			(*tp->loc_get_fct) (domain_id,
					    participant_id,
					    type, uc, mc, dst);
}

/* rtps_parameters_set -- Set transport parameters. */

int rtps_parameters_set (LocatorKind_t kind, const void *pars)
{
	const RTPS_TRANSPORT	*tp;
	int			res;

	if (kind >= MAX_TRANSPORTS)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if ((tp = rtps_transports [kind]) != NULL) {
		res = (*tp->pars_set_fct) (kind, pars);
		if (res)
			return (res);
	}
	rtps_transport_pars [kind] = pars;
	return (DDS_RETCODE_OK);
}

/* rtps_parameters_get -- Get transport parameters. */

int rtps_parameters_get (LocatorKind_t kind, void *pars, size_t msize)
{
	const RTPS_TRANSPORT	*tp;
	const void		*sp;
	int			res;
	RTPS_UDP_PARS		up;

	if (kind >= MAX_TRANSPORTS)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if ((tp = rtps_transports [kind]) == NULL) {
		if ((kind == LOCATOR_KIND_UDPv4 || 
		     kind == LOCATOR_KIND_UDPv6) &&
		    msize >= sizeof (RTPS_UDP_PARS)) {
			if (rtps_transport_pars [kind])
				sp = rtps_transport_pars [kind];
			else {
				up = rtps_udp_def_pars;
				up.pb = config_get_number (DC_UDP_PB, up.pb);
				up.dg = config_get_number (DC_UDP_DG, up.dg);
				up.pg = config_get_number (DC_UDP_PG, up.pg);
				up.d0 = config_get_number (DC_UDP_D0, up.d0);
				up.d1 = config_get_number (DC_UDP_D1, up.d1);
				up.d2 = config_get_number (DC_UDP_D2, up.d2);
				up.d3 = config_get_number (DC_UDP_D3, up.d3);
				sp = &up;
			}
			memcpy (pars, sp, sizeof (RTPS_UDP_PARS));

			return (DDS_RETCODE_OK);
		}
		return (DDS_RETCODE_PRECONDITION_NOT_MET);
	}
	res = (*tp->pars_get_fct) (kind, pars, msize);
	return (res);
}

/* rtps_locators_update -- Start updating locator lists. */

int rtps_locators_update (DomainId_t domain_id, unsigned id)
{
#ifdef DDS_FORWARD
	int	error;

	if (rtps_forward) {
		error = rfwd_locators_update (domain_id, id);
		if (error)
			return (error);
	}
#else
	ARG_NOT_USED (domain_id)
	ARG_NOT_USED (id)
#endif
	return (DDS_RETCODE_OK);
}

/* rtps_locator_add -- Add an active locator. */

int rtps_locator_add (DomainId_t    domain_id,
		      LocatorNode_t *locator,
		      unsigned      id,
		      int           serve)
{
	const RTPS_TRANSPORT	*tp;
	int			error;

	if ((tp = rtps_transport_lookup (locator->locator.kind, &error)) == NULL)
		return (error);

#ifdef DDS_FORWARD
	if (rtps_forward) {
		error = rfwd_locator_add (domain_id, locator, id, serve);
		if (error)
			return (error);
	}
#endif
	if (tp->loc_add_fct)
		error = (*tp->loc_add_fct) (domain_id, locator, id, serve);
	else
		error = DDS_RETCODE_OK;

	return (error);
}

/* rtps_locator_remove -- Remove an active locator. */

void rtps_locator_remove (unsigned id, LocatorNode_t *locator)
{
	const RTPS_TRANSPORT	*tp;
	int			error;

	if ((tp = rtps_transport_lookup (locator->locator.kind, &error)) == NULL)
		return;

	if (tp->loc_rem_fct)
		(*tp->loc_rem_fct) (id, locator);

#ifdef DDS_FORWARD
	if (rtps_forward)
		rfwd_locator_remove (id, locator);
#endif
}

/* rtps_update_begin -- Begin updating locators. */

unsigned rtps_update_begin (Domain_t *dp)
{
	const RTPS_TRANSPORT	*tp;
	unsigned		i, present = 0;

	for (i = 0; i < MAX_TRANSPORTS; i++)
		if ((tp = rtps_transports [i]) != NULL)
			present += (*tp->loc_upd_fct) (tp->kind, dp, 0);

	return (present);
}

/* rtps_update_end -- Updating locators has finished. */

unsigned rtps_update_end (Domain_t *dp)
{
	const RTPS_TRANSPORT	*tp;
	unsigned		i, present = 0;

	for (i = 0; i < MAX_TRANSPORTS; i++)
		if ((tp = rtps_transports [i]) != NULL)
			present += (*tp->loc_upd_fct) (tp->kind, dp, 1);

	return (present);
}

/* rtps_free_elements -- Free an element list. */

void rtps_free_elements (RME *mep)
{
	RME	*next_mep;

	for (; mep; mep = next_mep) {

		if (mep->db)
			db_free_data (mep->db);

		next_mep = mep->next;
		while (next_mep && (next_mep->flags & RME_CONTAINED) != 0) {
			 if (next_mep->db) 
				 db_free_data (next_mep->db);

			next_mep = next_mep->next;
		}

		if ((mep->flags & RME_CONTAINED) != 0)
			continue;

		mds_pool_free (rtps_mux_el_md, mep);
	}
}

/* rtps_free_messages -- Free a list of messages. */

void rtps_free_messages (RMBUF *mp)
{
	RMBUF	*next_mp;
	RME	*mep;

	for (; mp; mp = next_mp) {
		next_mp = mp->next;
		if (--mp->users)
			continue;

		for (mep = mp->first; mep; mep = mep->next)
			if ((mep->flags & RME_NOTIFY) != 0)
				(*rtps_mux_notify) ((NOTIF_DATA *) mep->d);

		rtps_free_elements (mp->first);
		mds_pool_free (rtps_mux_msg_md, mp);
	}
}

/* rtps_copy_message -- Make a copy of a message. */

RMBUF *rtps_copy_message (RMBUF *mp)
{
	RMBUF		*new_mp;
	RME		*new_mep, *mep, *prev_mep;
	NOTIF_DATA	*ndp;
	Change_t	*cp;
	String_t	*sp;
	size_t		xlen, ofs;

	new_mp = mds_pool_alloc (rtps_mux_msg_md);
	if (!new_mp)
		return (NULL);

	new_mp->next = NULL;
	new_mp->size = mp->size;
	new_mp->prio = mp->prio;
	new_mp->users = 1;
	new_mp->header = mp->header;
	new_mp->first = new_mp->last = NULL;

	for (mep = mp->first, prev_mep = NULL;
	     mep;
	     prev_mep = mep, mep = mep->next) {
		if ((mep->flags & RME_CONTAINED) != 0) {
			if (!prev_mep)
				new_mep = &new_mp->element;
			else {
				ofs = (unsigned char *) mep - (unsigned char *) prev_mep;
				new_mep = (RME *) ((unsigned char *) new_mp->last + ofs);
			}
		}
		else {
			new_mep = mds_pool_alloc (rtps_mux_el_md);
			if (!new_mep) {
				rtps_free_messages (new_mp);
				return (NULL);
			}
		}
		xlen = RME_HDRSIZE - sizeof (unsigned char *) * 2;
		new_mep->next = NULL;
		if (mep->data == mep->d) {
			new_mep->data = new_mep->d;
			xlen += mep->length;
		}
		else {
			new_mep->data = mep->data;
			if ((mep->flags & RME_NOTIFY) != 0) {
				xlen += sizeof (NOTIF_DATA);
				ndp = (NOTIF_DATA *) mep->d;
				if (ndp->type == NT_CACHE_FREE) {
					cp = (Change_t *) ndp->arg;
					rcl_access (cp);
					cp->c_nrefs++;
					rcl_done (cp);
				}
				else if (ndp->type == NT_STR_FREE) {
					sp = (String_t *) ndp->arg;
					str_ref (sp);
				}
			}
		}
		memcpy (&new_mep->db, &mep->db, xlen);
		if (new_mep->db) {
			rcl_access (new_mep->db);
			new_mep->db->nrefs++;
			rcl_done (new_mep->db);
		}
		if (new_mp->first)
			new_mp->last->next = new_mep;
		else
			new_mp->first = new_mep;
		new_mp->last = new_mep;
	}
	return (new_mp);
}

/* rtps_copy_messages -- Make an exact copy of a list of messages. */

RMBUF *rtps_copy_messages (RMBUF *msgs)
{
	RMBUF	*mp, *new_mp, *head, *tail;

	head = tail = NULL;
	for (mp = msgs; mp; mp = mp->next) {
		new_mp = rtps_copy_message (mp);
		if (!new_mp)
			break;

		if (head)
			tail->next = new_mp;
		else
			head = new_mp;
		tail = new_mp;
	}
	if (head)
		tail->next = NULL;
	return (head);
}

/* rtps_ref_message -- Make a new reference to a message. */

RMREF *rtps_ref_message (RMBUF *mp)
{
	RMREF		*new_rp;

	new_rp = mds_pool_alloc (rtps_mux_ref_md);
	if (!new_rp)
		return (NULL);

	mp->users++;
	new_rp->next = NULL;
	new_rp->message = mp;
	return (new_rp);
}

/* rtps_ref_messages -- Make a message reference list from a message list. */

RMREF *rtps_ref_messages (RMBUF *msgs)
{
	RMBUF		*mp;
	RMREF		*rp, *head, *tail;

	head = tail = NULL;
	for (mp = msgs; mp; mp = mp->next) {
		rp = rtps_ref_message (mp);
		if (!rp) {
			if (head)
				rtps_unref_messages (head);
			return (NULL);
		}
		if (head)
			tail->next = rp;
		else
			head = rp;
		tail = rp;
	}
	return (head);
}

/* rtps_unref_message -- No more need for a message reference. */

void rtps_unref_message (RMREF *rp)
{
	if (rp->message) {
		if (rp->message->users == 1) {
			rp->message->next = NULL;
			rtps_free_messages (rp->message);
		}
		else
			rp->message->users--;
		rp->message = NULL;
	}
	mds_pool_free (rtps_mux_ref_md, rp);
}

/* rtps_unref_messages -- No more need for a message reference list. */

void rtps_unref_messages (RMREF *list)
{
	RMREF	*rp, *next_rp;

	for (rp = list; rp; rp = next_rp) {
		next_rp = rp->next;
		rtps_unref_message (rp);
	}
}

/* rtps_locator_send_ll -- Send messages to lower layers directly without
			   involving the forwarder and without freeing them. */

void rtps_locator_send_ll (unsigned id, void *dest, int dlist, RMBUF *msgs)
{
	const RTPS_TRANSPORT	*tp;
	LocatorList_t		listp;
	Locator_t		*lp;

	if (dlist) {
		listp = (LocatorList_t) dest;
		while (listp) {
			lp = &listp->data->locator;
			if (lp->kind < MAX_TRANSPORTS &&
			    (tp = rtps_transports [lp->kind]) != NULL)
				(*tp->send_fct) (id, &listp, 1, msgs);
			else
				listp = listp->next;
		}
	}
	else {
		lp = (Locator_t *) dest;
		if (lp->kind < MAX_TRANSPORTS &&
		    (tp = rtps_transports [lp->kind]) != NULL)
			(*tp->send_fct) (id, lp, 0, msgs);
	}
}

/* rtps_locator_send -- Send a number of messages to the specified locator. */

void rtps_locator_send (unsigned id, void *dest, int dlist, RMBUF *msgs)
{

#ifdef DDS_FORWARD
	if (rtps_forward)
		rfwd_send (id, dest, dlist, msgs);
	else
#endif
	     {
 		rtps_locator_send_ll (id, dest, dlist, msgs);
		rtps_free_messages (msgs);
	}
}

/* rtps_local_node-- Returns a non-0 value if the Participant is directly
		     reachable, i.e. without passing thru a relay.
		     Note: the source is expected to be a meta unicast. */

int rtps_local_node (Participant_t *p, Locator_t *src)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
#ifdef DDS_TCP
	GuidPrefix_t	prefix;
	GuidPrefix_t    normalize;

	if ((src->kind & (LOCATOR_KIND_TCPv4 | LOCATOR_KIND_TCPv6)) == 0) {
#endif
		foreach_locator (p->p_meta_ucast, rp, np)
			if (locator_addr_equal (&np->locator, src))
				return (1);
#ifdef DDS_TCP
	}
	else if (!rtps_tcp_peer (src->handle,
				 0,
				 0,
				 &prefix)) {
		normalize = p->p_guid_prefix;
		guid_normalise (&normalize);

		if (!memcmp (&normalize.prefix,
					&prefix.prefix,
					GUIDPREFIX_SIZE - 1))   /* Ignore the count field! */
			return (1);
	}
#endif
	return (0);
}

#ifdef DDS_DEBUG

/* db_log_data -- Log the contents of a data buffer. */

void db_log_data (DB *dbp, unsigned char *data, size_t ofs, size_t length)
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
				data += ofs;
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
		log_print_region (RTPS_ID, 0, data, n, 0, 0);
		length -= n;
		if (length) {
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

/* rtps_log_message -- Log a single message. */

void rtps_log_message (unsigned id,
		       unsigned level,
		       RMBUF    *mp,
		       char     dir,
		       int      data)
{
	RME		*mep;
	unsigned	i;
	char		buf [32], c;
	static const char *id_str [] = {
		NULL, "PAD", NULL, NULL, NULL, NULL, "ACKNACK", "HEARTBEAT",
		"GAP   ", "INFO_TS", NULL, NULL, "INFO_SRC", "INFO_REPLY4",
		"INFO_DST", "INFO_REPLY", NULL, NULL, "NACK_FRAG", "HB_FRAG",
		NULL, "DATA ", "DATA_FRAG"
	};

	if (!protocol_valid (mp->header.protocol)) {
		log_printf (id, level, "Invalid protocol message!\r\n");
		return;
	}

	/* Dump message header. */
	log_printf (id, level, "RTPS: %c - %4lu: ", dir, (unsigned long) mp->size);
	log_printf (id, level, "%s - v%u.%u, vendor=%u.%u\r\n", 
		guid_prefix_str (&mp->header.guid_prefix, buf),
		mp->header.version [0], 
		mp->header.version [1], 
		mp->header.vendor_id [0], 
		mp->header.vendor_id [1]);

	/* Dump submessages. */
	for (mep = mp->first; mep; mep = mep->next) {
		if ((mep->flags & RME_HEADER) != 0) {
			if (mep->header.id <= ST_DATA_FRAG &&
			    id_str [mep->header.id])
				log_printf (id, level, "    %s\t-", id_str [mep->header.id]);
			else
				log_printf (id, level, "    ?0x%x\t-", mep->header.id);
			log_printf (id, level, "%4u ", mep->header.length);
			if ((mep->header.flags & SMF_KEY) != 0)
				log_printf (id, level, "K");
			if ((mep->header.flags & SMF_DATA) != 0) {
				if (mep->header.id == ST_DATA)
					c = 'D';
				else if (mep->header.id == ST_DATA_FRAG)
					c = 'K';
				else
					c = 'L';
				log_printf (id, level, "%c", c);
			}
			if ((mep->header.flags & SMF_FINAL) != 0) {
				if (mep->header.id == ST_INFO_TS)
					c = 'I';
				else if (mep->header.id == ST_INFO_REPLY ||
					 mep->header.id == ST_INFO_REPLY_IP4)
					c = 'M';
				else if (mep->header.id == ST_DATA ||
					 mep->header.id == ST_DATA_FRAG)
					c = 'Q';
				else
					c = 'F';
				log_printf (id, level, "%c", c);
			}
			if ((mep->header.flags & SMF_ENDIAN) != 0)
				log_printf (id, level, "E");
			if (mep->header.length)
				switch (mep->header.id) {
					case ST_INFO_SRC: {
						InfoSourceSMsg *ip;

						if (mep->header.length != mep->length ||
						    mep->header.length < sizeof (InfoSourceSMsg))
							goto dump_data;

						ip = (InfoSourceSMsg *) mep->data;
						log_printf (id, level, "\t%s - v%u.%u, vendor=%u.%u\r\n", 
							guid_prefix_str (&ip->guid_prefix, buf),
							ip->version [0], 
							ip->version [1], 
							ip->vendor [0], 
							ip->vendor [1]);
						continue;
					}
					case ST_INFO_REPLY:
						goto dump_data;

					case ST_INFO_DST: {
						GuidPrefix_t *gp;

						if (mep->length < 12)
							goto dump_data;

						gp = (GuidPrefix_t *) mep->data;
						log_printf (id, level, "\t%s\r\n", guid_prefix_str (gp, buf));
						continue;
					}
					case ST_HEARTBEAT: {
						HeartbeatSMsg *hp;

						if (mep->header.length != mep->length ||
						    mep->header.length < sizeof (HeartbeatSMsg))
							goto dump_data;

						hp = (HeartbeatSMsg *) mep->data;
						log_printf (id, level, "\tW%x->R%x (%u.%u-%u.%u) [%u]\r\n",
								hp->writer_id.w, hp->reader_id.w,
								hp->first_sn.high, hp->first_sn.low,
								hp->last_sn.high, hp->last_sn.low,
								hp->count);
						continue;
					}
					case ST_ACKNACK: {
						AckNackSMsg	*ap;
						unsigned	i, n;

						if (mep->header.length != mep->length ||
						    mep->header.length < MIN_ACKNACK_SIZE ||
						    mep->header.length > MAX_ACKNACK_SIZE)
							goto dump_data;

						ap = (AckNackSMsg *) mep->data;
						log_printf (id, level, "\tR%x->W%x (%u.%u) ",
								ap->reader_id.w, ap->writer_id.w,
								ap->reader_sn_state.base.high,
								ap->reader_sn_state.base.low);
						if (ap->reader_sn_state.numbits) {
							log_printf (id, level, "%u: ", 
								ap->reader_sn_state.numbits);
							n = (ap->reader_sn_state.numbits + 31) >> 5;
							for (i = 0; i < n; i++) {
								if (i)
									log_printf (id, level, ":");
								log_printf (id, level, "%08x", ap->reader_sn_state.bitmap [i]);
							}
						}
						else
							n = 0;
						log_printf (id, level, " [%u]\r\n", ap->reader_sn_state.bitmap [n]);
						continue;
					}
					case ST_DATA:
					case ST_DATA_FRAG: {
						DataSMsg	*dp;

						if (mep->length < sizeof (DataSMsg))
							goto dump_data;

						dp = (DataSMsg *) mep->data;
						log_printf (id, level, "\tW%x->R%x (%u.%u) qofs=%u",
								dp->writer_id.w, dp->reader_id.w,
								dp->writer_sn.high, dp->writer_sn.low,
								dp->inline_qos_ofs);
						break;
					}
					case ST_GAP: {
						GapSMsg		*gp;
						unsigned	i, n;

						if (mep->header.length != mep->length ||
						    mep->header.length < MIN_GAP_SIZE ||
						    mep->header.length > MAX_GAP_SIZE)
							goto dump_data;

						gp = (GapSMsg *) mep->data;
						log_printf (id, level, "\tW%x->R%x (%u.%u, %u.%u ",
								gp->writer_id.w, gp->reader_id.w,
								gp->gap_start.high,
								gp->gap_start.low,
								gp->gap_list.base.high,
								gp->gap_list.base.low);
						if (gp->gap_list.numbits) {
							log_printf (id, level, "%u: ", 
								gp->gap_list.numbits);
							n = (gp->gap_list.numbits + 31) >> 5;
							for (i = 0; i < n; i++) {
								if (i)
									log_printf (id, level, ":");
								log_printf (id, level, "%08x", gp->gap_list.bitmap [i]);
							}
						}
						else
							n = 0;
						log_printf (id, level, ") [%u]\r\n", gp->gap_list.bitmap [n]);
						continue;						
					}
					default:

					    dump_data:
						if (mep->data && mep->length) {
							if (mep->length <= 12) {
								log_printf (RTPS_ID, 0, " :");
								for (i = 0; i < mep->length; i++)
									log_printf (RTPS_ID, 0, " %02x", mep->data [i]);
							}
							else {
								log_printf (RTPS_ID, 0, "\r\n");
								log_print_region (RTPS_ID, 0, mep->data, (mep->length > 48) ? 48 : mep->length, 1, 1);
								/*if (mep->header.length > 48 || mep->header.length > mep->length)
									log_printf (RTPS_ID, 0, " ..");*/
							}
							/* log_printf (id, level, "  \t%u bytes", mep->header.length); */
						}
						break;
			}
		}
		if (!mep->length)
			continue;

		if (data) {
			log_printf (id, level, ":\r\n");
			if (mep->data != mep->d && mep->db)
				db_log_data (mep->db, mep->data, 0, mep->length - mep->pad);
			else
				log_print_region (id, level, mep->data, mep->length - mep->pad, 0, 0);
		}
		else
			log_printf (id, level, "\r\n");
	}
}

/* rtps_log_messages -- Log a list of messages. */

void rtps_log_messages (unsigned id,
		        unsigned level,
		        RMBUF    *msgs,
			char     dir,
			int      data)
{
	RMBUF	*msg;

	for (msg = msgs; msg; msg = msg->next)
		rtps_log_message (id, level, msg, dir, data);
}

#endif

