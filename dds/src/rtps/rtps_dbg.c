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

/* rtps_dbg.c -- RTPS Debug functionality. */

#ifdef DDS_DEBUG

#include "list.h"
#include "log.h"
#include "prof.h"
#include "error.h"
#include "debug.h"
#include "rtps_cfg.h"
#include "rtps_data.h"
#include "rtps_priv.h"

/* rtps_pool_dump -- Display some pool statistics. */

void rtps_pool_dump (size_t sizes [])
{
	print_pool_table (rtps_mem_blocks, (unsigned) MB_END, sizes);
}

#ifdef DDS_NATIVE_SECURITY

static char *encrypt_str (int submsg, int payload, unsigned crypto)
{
	const char	*s, *encrypt_s [] = {
		"none",
		"HMAC_SHA1", "HMAC_SHA256",
		"AES128_HMAC_SHA1", "AES256_HMAC_SHA256"
	};
	static char	buf [32];

	if (submsg || payload) {
		if (submsg)
			s = "submessage";
		else
			s = "payload";
		if (crypto && crypto <= 4)
			sprintf (buf, "%s(%s)", s, encrypt_s [crypto]);
		else
			sprintf (buf, "%s(%u?)", s, crypto);
	}
	else
		sprintf (buf, "none");
	return (buf);
}
#endif

static int endpoint_dump_fct (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) node;
	LocalEndpoint_t	*lep;
	Domain_t	*dp = (Domain_t *) arg;
	ENDPOINT	*rep;
	READER		*rp;
	WRITER		*wp;
	RemReader_t	*rrp;
	RemWriter_t	*rwp;
	unsigned	n;
	int		ni;
	const char	*names [2];

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	ep = *epp;
	if (!ep->rtps)
		return (1);

	rep = (ENDPOINT *) ep->rtps;
	dbg_printf ("    %u/%02x%02x%02x-%02x %c{%u}\t", 
			dp->domain_id,
			ep->entity_id.id [0], 
			ep->entity_id.id [1],
			ep->entity_id.id [2],
			ep->entity_id.id [3],
			(rep->is_reader) ? 'R' : 'W',
			ep->entity.handle);
	n = 0;
	if (rep->is_reader) {
		rp = (READER *) rep;
		wp = NULL;
		LIST_FOREACH (rp->rem_writers, rwp)
			n += rwp->rw_changes.nchanges;
	}
	else {
		wp = (WRITER *) rep;
		rp = NULL;
		LIST_FOREACH (wp->rem_readers, rrp)
			n += rrp->rr_changes.nchanges;
	}
	dbg_printf ("%u", n);
	lep = (LocalEndpoint_t *) ep;
	if ((ni = hc_total_instances (lep->cache)) >= 0)
		dbg_printf ("/%d", ni);
	endpoint_names (rep, names);
	dbg_printf ("\t%s/%s\r\n", names [0], names [1]);
#ifdef DDS_NATIVE_SECURITY
	if (lep->disc_prot || lep->submsg_prot || lep->payload_prot)
		dbg_printf ("\t%sAccess control, %s discovery, Encryption: %s\r\n",
			(!lep->access_prot) ? "No " : "",
			(lep->disc_prot) ? "Secure" : "Normal",
			encrypt_str (lep->submsg_prot, lep->payload_prot, lep->crypto_type));
#endif
	if (rep->is_reader) {
		LIST_FOREACH (rp->rem_writers, rwp) {
			dbg_printf ("\t\t\t    ->\tGUID: ");
			dbg_print_guid_prefix (&rwp->rw_endpoint->u.participant->p_guid_prefix);
			dbg_printf ("-");
			dbg_print_entity_id (NULL, &rwp->rw_endpoint->entity_id);
			dbg_printf (" {%u}\r\n", rwp->rw_endpoint->entity.handle);
		}
	}
	else {
		LIST_FOREACH (wp->rem_readers, rrp) {
			dbg_printf ("\t\t\t    ->\t");
			if (wp->endpoint.stateful) {
				dbg_printf ("GUID: ");
				dbg_print_guid_prefix (&rrp->rr_endpoint->u.participant->p_guid_prefix);
				dbg_printf ("-");
				dbg_print_entity_id (NULL, &rrp->rr_endpoint->entity_id);
			}
			else {
				dbg_printf ("Locator: ");
				dbg_print_locator (&rrp->rr_locator->locator);
			}
			dbg_printf (" {%u}\r\n", rrp->rr_endpoint->entity.handle);
		}
	}
#ifdef RTPS_OPT_MCAST
	if (rep->mc_locators) {
		dbg_printf ("\t\t\t    ->\tMulticast locators: ");
		locator_list_dump (rep->mc_locators);
		dbg_printf ("\r\n");
	}
#endif
	return (1);
}

/* rtps_endpoints_dump -- Display the registered endpoints. */

void rtps_endpoints_dump (void)
{
	Domain_t	*dp;
	unsigned	index, n;

	n = domain_count ();
	index = 0;
	for (;;) {
		dp = domain_next (&index, NULL);
		if (!dp)
			break;

		if (n > 1) {
			dbg_print_guid_prefix (&dp->participant.p_guid_prefix);
			dbg_printf (":\r\n");
		}
		sl_walk (&dp->participant.p_endpoints, endpoint_dump_fct, dp);
	}
}

static void rtps_dump_change (Change_t *cp, int newline)
{
	static const char *kind_str [] = { "ALIVE", "DISPOSED", 
					   "UNREGISTERED", "ZOMBIE"};

	dbg_printf ("\t[%p*%u:%s:h=%u,%d.%u]", (void *) cp,
			cp->c_nrefs, kind_str [cp->c_kind],
			cp->c_handle,
			cp->c_seqnr.high,
			cp->c_seqnr.low);
	if (newline)
		dbg_printf ("\r\n");
}

static void rtps_dump_gap (SequenceNumber_t *first, SequenceNumber_t *last)
{
	dbg_printf ("%u.%u..%u.%u]", first->high, first->low, last->high, last->low);
}

static void rtps_dump_ref (CCREF *rp, CCREF *unsp, CCREF *reqp)
{
#ifdef RTPS_FRAGMENTS
	FragInfo_t	*fip;
	unsigned	i;
#endif
	static const char *state_str [] = { "NEW", "REQ", "UNSENT", "UNDERWAY",
					    "UNACKED", "ACKED", "MISSING",
					    "RCVD", "LOST" };

	dbg_printf ("\r\n\t\t{%s:%u", state_str [rp->state], rp->relevant);
#ifdef RTPS_FRAGMENTS
	if ((fip = rp->fragments) != NULL) {
		dbg_printf ("\r\n\t\t  FI:(*%u,%u*%lu->%lu@%p,na:%u,fi:%u\r\t\t     ", 
				fip->nrefs, fip->total,
				(unsigned long) fip->fsize,
				(unsigned long) fip->length,
				(void *) fip->data,
				fip->num_na, fip->first_na);
		for (i = 0; i < (fip->total + 31) >> 5; i++)
			dbg_printf (" %08x", fip->bitmap [i]);
		dbg_printf (")\r\n\t\t");
	}
#endif
	if (rp->relevant)
		rtps_dump_change (rp->u.c.change, 0);
	else
		rtps_dump_gap (&rp->u.range.first, &rp->u.range.last);
	dbg_printf ("}");
	if (rp == unsp)
		dbg_printf (" (N)");
	else if (rp == reqp)
		dbg_printf (" (R)");
}

static void rtps_dump_cclist (const char *s, CCLIST *lp, CCREF *unsp, CCREF *reqp)
{
	CCREF	*rp;

	dbg_printf ("%s: (%u)", s, lp->nchanges);
	LIST_FOREACH (*lp, rp)
		rtps_dump_ref (rp, unsp, reqp);
	dbg_printf ("\r\n");
}

static void dump_locators (const char *name, const LocatorList_t lp, int ir)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;

	dbg_printf ("\t%s%s: ", name, (ir) ? " (from InfoReply)" : "");
	if (!lp) {
		dbg_printf ("(none)\r\n");
		return;
	}
	dbg_printf ("\r\n");
	foreach_locator (lp, rp, np) {
		dbg_printf ("\t\t");
		dbg_print_locator (&np->locator);
		dbg_printf ("\r\n");
	}
}

static void endpoint_header (Domain_t *dp, Endpoint_t *ep)
{
	ENDPOINT	*rep;
	const char	*names [2];

	rep = (ENDPOINT *) ep->rtps;
	dbg_printf ("%u/%02x%02x%02x-%02x %c{%u}", 
			dp->domain_id,
			ep->entity_id.id [0], 
			ep->entity_id.id [1],
			ep->entity_id.id [2],
			ep->entity_id.id [3],
			(rep->is_reader) ? 'R' : 'W',
			ep->entity.handle);
	endpoint_names (rep, names);
	dbg_printf (" - %s/%s:\r\n", names [0], names [1]);
}

/* rtps_writer_proxy_dump -- Dump the ReaderLocator/Proxy contexts of a writer. */

void rtps_writer_proxy_dump (WRITER *wp, DiscoveredReader_t *drp)
{
	RemReader_t	*rrp;
	GUID_t		guid;

	if (!wp->rem_readers.head) {
		dbg_printf ("No Reader%s contexts!\r\n", wp->endpoint.stateful ? " Proxy" : "Locator");
		return;
	}
	LIST_FOREACH (wp->rem_readers, rrp) {
		if (drp) {
			if (drp != (DiscoveredReader_t *) rrp->rr_endpoint)
				continue;

			endpoint_header (rrp->rr_endpoint->u.participant->p_domain, &wp->endpoint.endpoint->ep);
		}
		if (wp->endpoint.stateful) {
			dbg_printf ("    GUID: ");
			guid.prefix = rrp->rr_endpoint->u.participant->p_guid_prefix;
			guid.entity_id = rrp->rr_endpoint->entity_id;
			dbg_print_guid (&guid);
			dbg_printf (" {%u}", rrp->rr_endpoint->entity.handle);

		}
		else {
			dbg_printf ("    ");
			dbg_print_locator (&rrp->rr_locator->locator);
		}
		dbg_printf (" - \r\n\tInlineQoS=%d, Reliable=%d, Active=%d, Marshall=%d, MCast=%d",
				rrp->rr_inline_qos, rrp->rr_reliable,
				rrp->rr_active, rrp->rr_marshall,
				!rrp->rr_no_mcast);
#ifdef DDS_SECURITY
		dbg_printf (", Tunnel=%d", rrp->rr_tunnel);
#endif
		dbg_printf ("\r\n\tStates (Control/Tx/Ack): %s/%s/%s\r\n",
				rtps_rr_cstate_str [rrp->rr_cstate],
				rtps_rr_tstate_str [rrp->rr_tstate],
				rtps_rr_astate_str [rrp->rr_astate]);
		rtps_dump_cclist ("\tChanges: ", &rrp->rr_changes,
						 rrp->rr_unsent_changes,
						 rrp->rr_requested_changes);
		if (wp->endpoint.stateful) {
			dbg_printf ("\tLastAck=%u\r\n", rrp->rr_last_ack);
			if (rrp->rr_uc_locs)
				dump_locators ("Unicast locators", rrp->rr_uc_locs, rrp->rr_ir_locs);
			if (rrp->rr_mc_locs)
				dump_locators ("Multicast locators", rrp->rr_mc_locs, rrp->rr_ir_locs);
		}
		if (rrp->rr_reliable) {
			dbg_printf ("\tDirect reply locator:");
			if (rrp->rr_uc_dreply) {
				dbg_printf ("\r\n\t\t");
				dbg_print_locator (&rrp->rr_uc_dreply->locator);
				dbg_printf ("\r\n");
			}
			else
				dbg_printf (" not set\r\n");
			dbg_printf ("\tLast: %u.%u", rrp->rr_new_snr.high, rrp->rr_new_snr.low);
			if (rrp->rr_nack_timer && tmr_active (rrp->rr_nack_timer)) {
				dbg_printf (", Nack-timer: ");
				dbg_print_timer (rrp->rr_nack_timer);
			}
#ifdef RTPS_PROXY_INST
			dbg_printf (", local=%u, remote=%u",
					rrp->rr_loc_inst, rrp->rr_rem_inst);
#endif
			dbg_printf ("\r\n");
		}
#ifdef EXTRA_STATS
		dbg_printf ("\t# of DATA messages sent:     %u\r\n", rrp->rr_ndata);
		if (rrp->rr_reliable) {
			dbg_printf ("\t# of GAPs sent:              %u\r\n", rrp->rr_ngap);
			dbg_printf ("\t# of HEARTBEATs sent:        %u\r\n", rrp->rr_nheartbeat);
			dbg_printf ("\t# of ACKNACKs received:      %u\r\n", rrp->rr_nacknack);
		}
#ifdef RTPS_FRAGMENTS
		dbg_printf ("\t# of DATAFRAG messages sent: %u\r\n", rrp->rr_ndatafrags);
		if (rrp->rr_reliable) {
			dbg_printf ("\t# of HEARTBEATFRAGs sent:    %u\r\n", rrp->rr_nheartbeatfrags);
			dbg_printf ("\t# of NACKFRAGs received:     %u\r\n", rrp->rr_nnackfrags);
		}
#endif
#endif
		dbg_printf ("\tNext GUID: %p\r\n", (void *) rrp->rr_next_guid);
	}
}

/* rtps_reader_proxy_dump -- Dump the Writer Proxy contexts of a reader. */

static void rtps_reader_proxy_dump (READER *rp, DiscoveredWriter_t *dwp)
{
	RemWriter_t	*rwp;
	GUID_t		guid;

	if (!rp->endpoint.stateful) {
		dbg_printf ("Stateless Reader - no proxy writer contexts!\r\n");
		return;
	}
	LIST_FOREACH (rp->rem_writers, rwp) {
		if (dwp) {
			if (dwp != (DiscoveredWriter_t *) rwp->rw_endpoint)
				continue;

			endpoint_header (rwp->rw_endpoint->u.participant->p_domain, &rp->endpoint.endpoint->ep);
		}
		dbg_printf ("    GUID: "); 
		guid.prefix = rwp->rw_endpoint->u.participant->p_guid_prefix;
		guid.entity_id = rwp->rw_endpoint->entity_id;
		dbg_print_guid (&guid);
		dbg_printf (" {%u}", rwp->rw_endpoint->entity.handle);
		dbg_printf (" - \r\n\tReliable=%d, Active=%d, MCast=%d", 
				rwp->rw_reliable, rwp->rw_active,
				!rwp->rw_no_mcast);
#ifdef DDS_SECURITY
		dbg_printf (", Tunnel=%d", rwp->rw_tunnel);
#endif
		dbg_printf ("\r\n\tStates (Control/Ack): %s/%s\r\n",
				rtps_rw_cstate_str [rwp->rw_cstate],
				rtps_rw_astate_str [rwp->rw_astate]);
		rtps_dump_cclist ("\tChanges: ", &rwp->rw_changes, NULL, NULL);
		if (rp->endpoint.stateful)
			dbg_printf ("\tLastHeartbeat=%u\r\n",
				rwp->rw_last_hb);
		if (rwp->rw_uc_locs)
			dump_locators ("Unicast locators", rwp->rw_uc_locs, rwp->rw_ir_locs);
		if (rwp->rw_mc_locs)
			dump_locators ("Multicast locators", rwp->rw_mc_locs, rwp->rw_ir_locs);
		if (rwp->rw_reliable) {
			dbg_printf ("\tDirect reply locator:");
			if (rwp->rw_uc_dreply) {
				dbg_printf ("\r\n\t\t");
				dbg_print_locator (&rwp->rw_uc_dreply->locator);
				dbg_printf ("\r\n");
			}
			else
				dbg_printf (" not set\r\n");
			dbg_printf ("\tNext: %u.%u", rwp->rw_seqnr_next.high, rwp->rw_seqnr_next.low);
			if (rwp->rw_hbrsp_timer && tmr_active (rwp->rw_hbrsp_timer)) {
				dbg_printf (", Heartbeat-timer: ");
				dbg_print_timer (rwp->rw_hbrsp_timer);
			}
#ifdef RTPS_PROXY_INST
			dbg_printf (", LocalInst=%u, RemoteInst=%u",
					rwp->rw_loc_inst, rwp->rw_rem_inst);
#endif
			dbg_printf ("\r\n");
		}
#ifdef EXTRA_STATS
		dbg_printf ("\t# of DATA messages received: %u\r\n", rwp->rw_ndata);
		if (rwp->rw_reliable) {
			dbg_printf ("\t# of GAPs received:          %u\r\n", rwp->rw_ngap);
			dbg_printf ("\t# of HEARTBEATs received:    %u\r\n", rwp->rw_nheartbeat);
			dbg_printf ("\t# of ACKNACKs sent:          %u\r\n", rwp->rw_nacknack);
		}
#ifdef RTPS_FRAGMENTS
		dbg_printf ("\t# of DATAFRAG messages recvd:%u\r\n", rwp->rw_ndatafrags);
		if (rwp->rw_reliable) {
			dbg_printf ("\t# of HEARTBEATFRAGs received:%u\r\n", rwp->rw_nheartbeatfrags);
			dbg_printf ("\t# of NACKFRAGs sent:         %u\r\n", rwp->rw_nnackfrags);
		}
#endif
#endif
		dbg_printf ("\tNext GUID: %p\r\n", (void *) rwp->rw_next_guid);
	}
}

typedef struct proxy_attr_st {
	Domain_t	*domain;
	Endpoint_t	*rem_ep;
} ProxyAttr_t;

static int proxy_dump_fct (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) node;
	ProxyAttr_t	*pa = (ProxyAttr_t *) arg;
	ENDPOINT	*rep;

	ARG_NOT_USED (list)

	ep = *epp;
	if (!ep->rtps)
		return (1);

	rep = (ENDPOINT *) ep->rtps;
	if (rep->is_reader && !rep->stateful)
		return (1);

	if (!pa->rem_ep)
		endpoint_header (pa->domain, ep);
	if (rep->is_reader)
		rtps_reader_proxy_dump ((READER *) rep, (DiscoveredWriter_t *) pa->rem_ep);
	else
		rtps_writer_proxy_dump ((WRITER *) rep, (DiscoveredReader_t *) pa->rem_ep);
	return (1);
}

/* rtps_proxy_dump -- Display the proxy contexts of an endpoint. */

void rtps_proxy_dump (Endpoint_t *e)
{
	ENDPOINT	*ep;
	Domain_t	*dp;
	unsigned	index;
	ProxyAttr_t	pa;

	if (!e) {
		index = 0;
		for (;;) {
			dp = domain_next (&index, NULL);
			if (!dp)
				break;

			pa.domain = dp;
			pa.rem_ep = NULL;
			sl_walk (&dp->participant.p_endpoints, proxy_dump_fct, &pa);
		}
	}
	else if (!entity_discovered (e->entity.flags)) {
		ep = e->rtps;
		if (!ep) {
			dbg_printf ("No such endpoint!\r\n");
			return;
		}
		if (ep->is_reader) {
			endpoint_header (e->u.subscriber->domain, e);
			rtps_reader_proxy_dump ((READER *) ep, NULL);
		}
		else {
			endpoint_header (e->u.publisher->domain, e);
			rtps_writer_proxy_dump ((WRITER *) ep, NULL);
		}
	}
	else {
		index = 0;
		for (;;) {
			dp = domain_next (&index, NULL);
			if (!dp)
				break;

			pa.domain = dp;
			pa.rem_ep = e;
			sl_walk (&dp->participant.p_endpoints, proxy_dump_fct, &pa);
		}
	}
}

/* rtps_writer_proxy_restart -- Restart the Proxy contexts of a writer. */

void rtps_writer_proxy_restart (WRITER *wp, DiscoveredReader_t *drp)
{
	RemReader_t	*rrp;

	if (!wp->endpoint.stateful)
		return;

	if (!wp->rem_readers.head) {
		dbg_printf ("No Reader%s contexts!\r\n", wp->endpoint.stateful ? " Proxy" : "Locator");
		return;
	}
	LIST_FOREACH (wp->rem_readers, rrp) {
		if (drp && drp != (DiscoveredReader_t *) rrp->rr_endpoint)
			continue;

		if (rrp->rr_reliable) {
			if (drp) {
				dbg_printf ("Restart ");
				endpoint_header (rrp->rr_endpoint->u.participant->p_domain, &wp->endpoint.endpoint->ep);
			}
			dbg_printf ("\t\t\t    ->\tGUID: ");
			dbg_print_guid_prefix (&rrp->rr_endpoint->u.participant->p_guid_prefix);
			dbg_printf ("-");
			dbg_print_entity_id (NULL, &rrp->rr_endpoint->entity_id);
			dbg_printf (" {%u}\r\n", rrp->rr_endpoint->entity.handle);
			rtps_matched_reader_restart ((Writer_t *) wp->endpoint.endpoint,
						     (DiscoveredReader_t *) rrp->rr_endpoint);
		}
	}
}

/* rtps_reader_proxy_restart -- Restart the Proxy contexts of a reader. */

void rtps_reader_proxy_restart (READER *rp, DiscoveredWriter_t *dwp)
{
	RemWriter_t	*rwp;

	if (!rp->endpoint.stateful)
		return;

	if (!rp->rem_writers.head) {
		dbg_printf ("No Reader proxy contexts!\r\n");
		return;
	}
	LIST_FOREACH (rp->rem_writers, rwp) {
		if (dwp && dwp != (DiscoveredWriter_t *) rwp->rw_endpoint)
			continue;

		if (rwp->rw_reliable) {
			if (dwp) {
				dbg_printf ("Restart ");
				endpoint_header (rwp->rw_endpoint->u.participant->p_domain, &rp->endpoint.endpoint->ep);
			}
			dbg_printf ("\t\t\t    ->\tGUID: ");
			dbg_print_guid_prefix (&rwp->rw_endpoint->u.participant->p_guid_prefix);
			dbg_printf ("-");
			dbg_print_entity_id (NULL, &rwp->rw_endpoint->entity_id);
			dbg_printf (" {%u}\r\n", rwp->rw_endpoint->entity.handle);
			rtps_matched_writer_restart ((Reader_t *) rp->endpoint.endpoint,
						     (DiscoveredWriter_t *) rwp->rw_endpoint);
		}
	}
}

#ifdef RTPS_PROXY_INST

static int proxy_restart_fct (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) node;
	ProxyAttr_t	*pa = (ProxyAttr_t *) arg;
	ENDPOINT	*rep;

	ARG_NOT_USED (list)

	ep = *epp;
	if (!ep->rtps)
		return (1);

	rep = (ENDPOINT *) ep->rtps;
	if (!rep->stateful)
		return (1);

	if (!pa->rem_ep) {
		dbg_printf ("Restart ");
		endpoint_header (ep->u.subscriber->domain, ep);
	}
	if (rep->is_reader)
		rtps_reader_proxy_restart ((READER *) rep, (DiscoveredWriter_t *) pa->rem_ep);
	else
		rtps_writer_proxy_restart ((WRITER *) rep, (DiscoveredReader_t *) pa->rem_ep);
	return (1);
}

#endif

/* rtps_proxy_restart -- Restart the proxy of an endpoint. */

void rtps_proxy_restart (Endpoint_t *e)
{
#ifdef RTPS_PROXY_INST
	ENDPOINT	*ep;
	Domain_t	*dp;
	unsigned	index;
	ProxyAttr_t	pa;

	if (!e) {
		index = 0;
		for (;;) {
			dp = domain_next (&index, NULL);
			if (!dp)
				break;

			pa.domain = dp;
			pa.rem_ep = NULL;
			sl_walk (&dp->participant.p_endpoints, proxy_restart_fct, &pa);
		}
	}
	else if (!entity_discovered (e->entity.flags)) {
		ep = e->rtps;
		if (!ep || !ep->stateful) {
			dbg_printf ("No such stateful reliable endpoint!\r\n");
			return;
		}
		dbg_printf ("Restart ");
		if (ep->is_reader) {
			endpoint_header (e->u.subscriber->domain, e);
			rtps_reader_proxy_restart ((READER *) ep, NULL);
		}
		else {
			endpoint_header (e->u.publisher->domain, e);
			rtps_writer_proxy_restart ((WRITER *) ep, NULL);
		}
	}
	else {
		index = 0;
		for (;;) {
			dp = domain_next (&index, NULL);
			if (!dp)
				break;

			pa.domain = dp;
			pa.rem_ep = e;
			sl_walk (&dp->participant.p_endpoints, proxy_restart_fct, &pa);
		}
	}
#else
	ARG_NOT_USED (e)

	dbg_printf ("Proxy restart is not supported in this build!\r\n");
#endif
}

void rtps_cache_dump (Endpoint_t *e)
{
	ENDPOINT	*ep;

	ep = e->rtps;
	if (!ep) {
		dbg_printf ("No such endpoint!\r\n");
		return;
	}
	hc_cache_dump (ep->endpoint->cache);
}

void rtps_receiver_dump (void)
{
	unsigned	i;
	Locator_t	*locp;
	static const char *rx_err_str [] = {
		"No error",
		"Submessage too short",
		"Invalid submessage",
		"Invalid QoS data",
		"Out of buffers",
		"Unknown destination",
		"Invalid marshalling"
	};

        dbg_print_indent (0, "Protocol version");
	dbg_print_uclist (2, rtps_receiver.src_version, 0);
	dbg_printf ("\r\n");
        dbg_print_indent (0, "Src. Vendor Id");
	dbg_print_uclist (2, rtps_receiver.src_vendor, 1);
        dbg_printf ("\r\n");
        dbg_print_indent (0, "Src. GUID Prefix");
	dbg_print_guid_prefix (&rtps_receiver.src_guid_prefix);
	dbg_printf ("\r\n");
        dbg_print_indent (0, "Dst. GUID Prefix");
	dbg_print_guid_prefix (&rtps_receiver.dst_guid_prefix);
	dbg_printf ("\r\n");
	if (rtps_receiver.have_timestamp) {
		dbg_print_indent (0, "Timestamp");
		dbg_printf ("%d.%us", FTIME_SEC (rtps_receiver.timestamp),
				      FTIME_FRACT (rtps_receiver.timestamp));
		dbg_printf ("\r\n");
	}
	if (rtps_receiver.n_uc_replies) {
		dbg_print_indent (0, "Unicast locators");
		for (i = 0, locp = (Locator_t *) rtps_receiver.reply_locs;
		     i < rtps_receiver.n_uc_replies;
		     i++, locp = (Locator_t *)((char *) locp + MSG_LOCATOR_SIZE)) {
			dbg_printf ("\r\n\t\t");
			dbg_print_locator (locp);
		}
		dbg_printf ("\r\n");
	}
	if (rtps_receiver.n_mc_replies) {
		dbg_print_indent (0, "Multicast locators");
		for (i = 0, locp = (Locator_t *) ((char *) rtps_receiver.reply_locs + rtps_receiver.n_uc_replies * MSG_LOCATOR_SIZE);
		     i < rtps_receiver.n_mc_replies;
		     i++, locp = (Locator_t *)((char *) locp + MSG_LOCATOR_SIZE)) {
			dbg_printf ("\r\n\t\t");
			dbg_print_locator (locp);
		}
		dbg_printf ("\r\n");
	}
	dbg_print_indent (0, "Domain");
	dbg_printf ("%p\r\n", (void *) rtps_receiver.domain);
	dbg_print_indent (0, "# of invalid submessages");
	dbg_printf ("%lu\r\n", rtps_receiver.inv_submsgs);
	dbg_print_indent (0, "# of too short submessages");
	dbg_printf ("%lu\r\n", rtps_receiver.submsg_too_short);
	dbg_print_indent (0, "# of invalid QoS errors");
	dbg_printf ("%lu\r\n", rtps_receiver.inv_qos);
	dbg_print_indent (0, "# of out-of-memory errors");
	dbg_printf ("%lu\r\n", rtps_receiver.no_bufs);
	dbg_print_indent (0, "# of unknown destination errors");
	dbg_printf ("%lu\r\n", rtps_receiver.unkn_dest);
	dbg_print_indent (0, "# of invalid marshalling data");
	dbg_printf ("%lu\r\n", rtps_receiver.inv_marshall);
	if (rtps_receiver.last_error == R_NO_ERROR)
		return;

	dbg_print_indent (0, "Last error");
	dbg_printf ("%s\r\n", rx_err_str [rtps_receiver.last_error]);
	dbg_print_indent (0, "Last MEP:\r\n");
	dbg_print_region (&rtps_receiver.last_mep, sizeof (RME), 0, 0);
	dbg_print_indent (0, "Last domain");
	dbg_printf ("%u\r\n", rtps_receiver.domain->domain_id);
	if (rtps_receiver.msg_size) {
		dbg_print_indent (0, "Last msg:\r\n");
		dbg_print_region (rtps_receiver.msg_buffer,
				  rtps_receiver.msg_size, 0, 0);
	}
}

void rtps_transmitter_dump (void)
{
	static const char *tx_err_str [] = {
		"No error",
		"No destination (writer)",
		"No destination (reader)",
		"Out of memory"
	};

	dbg_print_indent (0, "# of locator transmits");
	dbg_printf ("%lu\r\n", rtps_transmitter.nlocsend);
	dbg_print_indent (0, "# of missing locator events");
	dbg_printf ("%lu\r\n", rtps_transmitter.no_locator);
	dbg_print_indent (0, "# of out-of-memory conditions");
	dbg_printf ("%lu\r\n", rtps_transmitter.no_memory);
	if (rtps_transmitter.last_error == T_NO_ERROR)
		return;

	dbg_print_indent (0, "Last error");
	dbg_printf ("%s\r\n", tx_err_str [rtps_transmitter.last_error]);
	if (rtps_transmitter.msg_size) {
		dbg_print_indent (0, "Last msg:\r\n");
		dbg_print_region (rtps_transmitter.msg_buffer,
				  rtps_transmitter.msg_size, 0, 0);
	}
}

#else
int avoid_emtpy_translation_unit_rtps_dbg_c;
#endif

