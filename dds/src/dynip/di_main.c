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

/* di_main.c -- Dynamic IP handler for various Operating Systems. */

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include "win.h"
#include "Ws2IpDef.h"
#include "Ws2tcpip.h"
#include "Iphlpapi.h"
#else
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#endif
#include "thread.h"
#include "pool.h"
#include "libx.h"
#include "config.h"
#include "log.h"
#include "error.h"
#include "timer.h"
#include "dds/dds_error.h"
#include "di_data.h"
#include "dynip.h"

/*#define TRACE_DYNIP_NOTIFY   ** Trace DYNIP notify calls. */

#define	MAX_IFTABLE	8

struct ip_addr_st {
#ifndef DDS_IPV6
	uint32_t	a_ipv4;
#else
	union {
	  uint32_t	ipv4;
	  unsigned char	ipv6 [16];
	}		u;
#define	a_ipv4		u.ipv4
#define	a_ipv6		u.ipv6
#endif
	Scope_t		scope;
	IP_Addr_t	*next;
};

struct ip_intf_st {
	unsigned	id;
	int		up;
	int		allow;
	int		allow6;
	char		name [16];
	IP_Addr_t	*addr;
#ifdef DDS_IPV6
	IP_Addr_t	*addr6;
#endif
	IP_Intf_t	*next;
};

typedef struct nl_proto_st {
	unsigned char	*own;
	unsigned	*num_own;
	unsigned	max_own;
	DI_NOTIFY	notify;
	unsigned	n_changes;
	Scope_t		min_scope;
	Scope_t		max_scope;
} NL_Proto;

static IP_Intf_t	*intf_table [MAX_IFTABLE];
static unsigned		n_proto;
static int		init;
static NL_Proto		ipv4;
#ifdef DDS_IPV6
static NL_Proto		ipv6;
#endif
static Timer_t		sched_timer;
static Timer_t		notify_timer;

IP_Intf_t *di_intf_lookup (unsigned id)
{
	IP_Intf_t	*ifp;
	unsigned	h = id & (MAX_IFTABLE - 1);

	for (ifp = intf_table [h]; ifp; ifp = ifp->next)
		if (ifp->id == id)
			return (ifp);

	return (NULL);
}

IP_Intf_t *di_intf_new (unsigned id)
{
	IP_Intf_t	*ifp;
	unsigned	h = id & (MAX_IFTABLE - 1);
	const char	*ifs;

	ifp = xmalloc (sizeof (IP_Intf_t));
	if (!ifp)
		return (NULL);

	ifp->id = id;
	ifp->up = -1;			/* Unknown status. */
	if_indextoname (id, ifp->name);
	if (config_defined (DC_IP_Intf)) {
		ifs = config_get_string (DC_IP_Intf, NULL);
		ifp->allow = slist_match (ifs, ifp->name, ':');
	}
	else
		ifp->allow = 1;
	ifp->addr = NULL;
#ifdef DDS_IPV6
	if (config_defined (DC_IPv6_Intf)) {
		ifs = config_get_string (DC_IPv6_Intf, NULL);
		ifp->allow6 = slist_match (ifs, ifp->name, ':');
	}
	else
		ifp->allow6 = 1;
	ifp->addr6 = NULL;
#else
	ifp->allow6 = 0;
#endif
	ifp->next = intf_table [h];
	intf_table [h] = ifp;
	return (ifp);
}

static void di_intf_addr_delete (IP_Intf_t *ifp, int ipv6, IP_Addr_t *ap)
{
	IP_Addr_t	*prev, *xap;

#ifdef DDS_IPV6
	if (ipv6)
		xap = ifp->addr6;
	else
#else
	ARG_NOT_USED (ipv6)
#endif
		xap = ifp->addr;
	for (prev = NULL;
	     xap && xap != ap;
	     prev = xap, xap = xap->next)
		;
	if (!xap)
		return;

	if (prev)
		prev->next = ap->next;
	else
#ifdef DDS_IPV6
		if (ipv6)
			ifp->addr6 = ap->next;
		else
#endif
			ifp->addr = ap->next;
	xfree (ap);
}

static void di_intf_delete (IP_Intf_t *ifp)
{
	IP_Intf_t	*prev, *xifp;
	unsigned	h = ifp->id & (MAX_IFTABLE - 1);

	for (prev = NULL, xifp = intf_table [h];
	     xifp && xifp != ifp;
	     prev = xifp, xifp = ifp->next)
		;

	if (!xifp)
		return;

	if (prev)
		prev->next = ifp->next;
	else
		intf_table [h] = ifp->next;
	while (ifp->addr)
		di_intf_addr_delete (ifp, 0, ifp->addr);
#ifdef DDS_IPV6
	while (ifp->addr6)
		di_intf_addr_delete (ifp, 1, ifp->addr6);
#endif
	xfree (ifp);
}

static void di_intf_change (Config_t c)
{
	ARG_NOT_USED (c)

	di_filter_update ();
}

int di_init (void)
{
	tmr_init (&sched_timer, "D-IP");
	tmr_init (&notify_timer, "D-IP notify");
	n_proto = 0;
	config_notify (DC_IP_Intf, di_intf_change);
#ifdef DDS_IPV6
	config_notify (DC_IPv6_Intf, di_intf_change);
#endif
	init = 1;
	return (di_sys_init ());
}

void di_final (void)
{
	unsigned	i;

	if (!init)
		return;

	if (tmr_active (&sched_timer))
		tmr_stop (&sched_timer);
	if (tmr_active (&notify_timer))
		tmr_stop (&notify_timer);

	if (ipv4.own)
		di_detach (AF_INET);
#ifdef DDS_IPV6
	if (ipv6.own)
		di_detach (AF_INET6);
#endif
	di_sys_final ();
	for (i = 0; i < MAX_IFTABLE; i++)
		while (intf_table [i])
			di_intf_delete (intf_table [i]);
	init = 0;
}

static void di_ipv4_new (unsigned char *ipa, Scope_t scope)
{
	unsigned char	*p;
	unsigned	i;
	/*uint32_t	addr;*/
	uint32_t	scope_u;

	for (i = 0, p = ipv4.own; i < *ipv4.num_own; i++, p += OWN_IPV4_SIZE)
		if (!memcmp (p, ipa, 4))
			return;	/* Already in table. */

	/*addr = ntohl (*(uint32_t *) ipa);
	dbg_printf (" ==> New IPv4: %u.%u.%u.%u\r\n",
		addr >> 24,
		(addr >> 16) & 0xff,
		(addr >> 8) & 0xff,
		addr & 0xff);*/

	if (*ipv4.num_own >= ipv4.max_own) {
		warn_printf ("DynIP: IPv4 address table is full!");
		return;
	}
	memcpy (p, ipa, 4);
	scope_u = scope;
	memcpy (p + 4, &scope_u, 4);
	(*ipv4.num_own)++;
	ipv4.n_changes++;
}

static void di_ipv4_remove (unsigned char *ipa)
{
	unsigned char	*p;
	unsigned	i;
	/*uint32_t	addr;*/

	for (i = 0, p = ipv4.own; i < *ipv4.num_own; i++, p += OWN_IPV4_SIZE)
		if (!memcmp (p, ipa, 4))
			break;	/* Found it. */

	if (i >= *ipv4.num_own)
		return;	/* Not in table --> ignore. */

	/*addr = ntohl (*(uint32_t *) p);
	dbg_printf (" ==> Remove IPv4: %u.%u.%u.%u\r\n",
		addr >> 24,
		(addr >> 16) & 0xff,
		(addr >> 8) & 0xff,
		addr & 0xff);*/

	if (i < *ipv4.num_own - 1)
		memmove (p, p + OWN_IPV4_SIZE, (*ipv4.num_own - i - 1) * OWN_IPV4_SIZE);
	(*ipv4.num_own)--;
	ipv4.n_changes++;
}

#ifdef DDS_IPV6

static void di_ipv6_new (unsigned char *ipa,
			 uint32_t scope_id,
			 Scope_t scope)
{
	unsigned char	*p;
	unsigned	i;
	uint32_t	scope_u;
	/*char		buf [100];*/

	if (!ipv6.own)
		return;

	for (i = 0, p = ipv6.own; i < *ipv6.num_own; i++, p += OWN_IPV6_SIZE)
		if (!memcmp (p, ipa, 16))
			return;	/* Already in table. */

	/*inet_ntop (AF_INET6, ipa, buf, sizeof (buf));
	dbg_printf (" ==> New IPv6: %s\r\n", buf);*/

	if (*ipv6.num_own >= ipv6.max_own) {
		warn_printf ("IPv6 address table is full!");
		return;
	}
	memcpy (p, ipa, 16);
	memcpy (p + OWN_IPV6_SCOPE_ID_OFS, &scope_id, 4);
	scope_u = scope;
	memcpy (p + OWN_IPV6_SCOPE_OFS, &scope_u, 4);
	(*ipv6.num_own)++;
	ipv6.n_changes++;
}

static void di_ipv6_remove (unsigned char *ipa)
{
	unsigned char	*p;
	unsigned	i;
	/*char		buf [100];*/

	if (!ipv6.own)
		return;

	for (i = 0, p = ipv6.own; i < *ipv6.num_own; i++, p += OWN_IPV6_SIZE)
		if (!memcmp (p, ipa, 16))
			break;	/* Found it. */

	if (i >= *ipv6.num_own)
		return;	/* Not in table --> ignore. */

	/*inet_ntop (AF_INET6, ipa, buf, sizeof (buf));
	dbg_printf (" ==> Remove IPv6: %s\r\n", buf);*/

	if (i < *ipv6.num_own - 1)
		memmove (p, p + OWN_IPV6_SIZE, (*ipv6.num_own - i - 1) * OWN_IPV6_SIZE);
	(*ipv6.num_own)--;
	ipv6.n_changes++;
}

#endif

void di_add_addr (IP_Intf_t *ifp, unsigned family, unsigned char *ipa)
{
	IP_Addr_t	*ap;
	uint32_t	ipaddr;
#ifdef DDS_IPV6
	char		buf [100];
#endif

	if (family == AF_INET) {
		ipaddr = *((uint32_t *) ipa);
		ipaddr = htonl (ipaddr);
		for (ap = ifp->addr; ap; ap = ap->next)
			if (ap->a_ipv4 == ipaddr)

				/* Already exists - nothing to do. */
				return;

		ap = xmalloc (sizeof (IP_Addr_t));
		if (!ap) {
			warn_printf ("di_add_addr: not enough memory for IP address!");
			return;
		}
		ap->a_ipv4 = ipaddr;
		ap->scope = sys_ipv4_scope (ipa);
		ap->next = ifp->addr;
		ifp->addr = ap;
		log_printf (IP_ID, 0, "DynIP: %s: %u.%u.%u.%u - new address (scope=%s).\r\n",
				ifp->name,
				ipaddr >> 24,
				(ipaddr >> 16) & 0xff,
				(ipaddr >> 8) & 0xff,
				ipaddr & 0xff,
				sys_scope_str (ap->scope));
		if (ifp->up &&
		    ifp->allow &&
		    ap->scope >= ipv4.min_scope &&
		    ap->scope <= ipv4.max_scope) {
			if (!strcmp (ifp->name, "lo"))
				return;
#ifdef FILT_VMWARE
			if (!memcmp (ifp->name, "vmnet", 5))
				return;
#endif
			di_ipv4_new (ipa, ap->scope);
		}
	}
#ifdef DDS_IPV6
	else if (family == AF_INET6) {
		for (ap = ifp->addr6; ap; ap = ap->next)
			if (!memcmp (ap->a_ipv6, ipa, 16))

				/* Already exists - nothing to do. */
				return;

		ap = xmalloc (sizeof (IP_Addr_t));
		if (!ap) {
			warn_printf ("di_add_addr: not enough memory for IPv6 address!");
			return;
		}
		memcpy (ap->a_ipv6, ipa, 16);
		ap->scope = sys_ipv6_scope (ipa);
		ap->next = ifp->addr6;
		ifp->addr6 = ap;
		inet_ntop (AF_INET6, ipa, buf, sizeof (buf));
		log_printf (IP_ID, 0, "DynIP: %s: %s - new address (scope=%s).\r\n",
				ifp->name, buf, sys_scope_str (ap->scope));
		if (ifp->up &&
		    ifp->allow6 &&
		    ap->scope >= ipv6.min_scope &&
		    ap->scope <= ipv6.max_scope) {
			if (!strcmp (ifp->name, "lo"))
				return;
#ifdef FILT_VMWARE
			if (!memcmp (ifp->name, "vmnet", 5))
				return;
#endif
			di_ipv6_new (ipa, 0, ap->scope);
		}
	}
#endif
	else
		return;
}

void di_remove_addr (IP_Intf_t *ifp, unsigned family, unsigned char *ipa)
{
	IP_Addr_t	*ap, *prev;
	uint32_t	ipaddr;
#ifdef DDS_IPV6
	char		buf [100];
#endif

	if (family == AF_INET) {
		ipaddr = *((uint32_t *) ipa);
		ipaddr = htonl (ipaddr);
		for (prev = NULL, ap = ifp->addr; ap; prev = ap, ap = ap->next)
			if (ap->a_ipv4 == ipaddr)
				break;

		if (!ap)
			return;

		if (prev)
			prev->next = ap->next;
		else
			ifp->addr = ap->next;

		log_printf (IP_ID, 0, "DynIP: %s: %u.%u.%u.%u - address removed.\r\n",
				ifp->name,
				ipaddr >> 24,
				(ipaddr >> 16) & 0xff,
				(ipaddr >> 8) & 0xff,
				ipaddr & 0xff);
		if (ifp->up) {
			if (!strcmp (ifp->name, "lo"))
				return;
#ifdef FILT_VMWARE
			if (!memcmp (ifp->name, "vmnet", 5))
				return;
#endif
			di_ipv4_remove (ipa);
		}
		xfree (ap);
	}
#ifdef DDS_IPV6
	else if (family == AF_INET6) {
		for (prev = NULL, ap = ifp->addr6; ap; prev = ap, ap = ap->next)
			if (!memcmp (ap->a_ipv6, ipa, 16))
				break;

		if (!ap)
			return;

		if (prev)
			prev->next = ap->next;
		else
			ifp->addr6 = ap->next;

		inet_ntop (AF_INET6, ipa, buf, sizeof (buf));
		log_printf (IP_ID, 0, "DynIP: %s: %s - address removed.\r\n",
			ifp->name, buf);
		if (ifp->up) {
			if (!strcmp (ifp->name, "lo"))
				return;
#ifdef FILT_VMWARE
			if (!memcmp (ifp->name, "vmnet", 5))
				return;
#endif
			di_ipv6_remove (ipa);
		}
		xfree (ap);
	}
#endif
	else
		return;
}

void di_intf_update (IP_Intf_t *ifp,
		     int       up,
		     int       allow,
		     int       allow6)
{
	IP_Addr_t	*ap;
	int		old_up, old_allow;
#ifdef DDS_IPV6
	int             old_allow6;
#endif
	uint32_t	ipa;

	if (!strcmp (ifp->name, "lo"))
		return;

#ifdef FILT_VMWARE
	if (!memcmp (ifp->name, "vmnet", 5))
		return;
#endif
	if (allow < 0)
		allow = ifp->allow;
	if (allow6 < 0)
		allow6 = ifp->allow6;
	if (ifp->up == up && ifp->allow == allow && ifp->allow6 == allow6)
		return;

	old_up = ifp->up != 0;
	ifp->up = up != 0;
	old_allow = ifp->allow;
	ifp->allow = allow;
	if (ifp->up != old_up)
		log_printf (IP_ID, 0, "DynIP: %s - state is %s!\r\n", ifp->name, (up) ? "up" : "down");
	if ((up && !old_up && allow) ||
	    (!up && old_up && old_allow) ||
	    (up && old_up && allow != old_allow)) {
		for (ap = ifp->addr; ap; ap = ap->next) {
			ipa = htonl (ap->a_ipv4);
			if (ap->scope < ipv4.min_scope ||
			    ap->scope > ipv4.max_scope)
				continue;

			if ((!old_up && up) ||
			    (old_up && up && allow))
				di_ipv4_new ((unsigned char *) &ipa, ap->scope);
			else
				di_ipv4_remove ((unsigned char *) &ipa);
		}
	}
#ifdef DDS_IPV6
	old_allow6 = ifp->allow6;
	ifp->allow6 = allow6;
	if ((up && !old_up && allow6) ||
	    (!up && old_up && old_allow6) ||
	    (up && old_up && allow6 != old_allow6)) {
		for (ap = ifp->addr6; ap; ap = ap->next)
			if (ap->scope < ipv6.min_scope ||
			    ap->scope > ipv6.max_scope) {
				continue;

			if ((!old_up && up) ||
			    (old_up && up && allow6))
				di_ipv6_new (ap->a_ipv6, 0, ap->scope);
			else
				di_ipv6_remove (ap->a_ipv6);
		}
	}
#endif
}

void di_intf_removed (IP_Intf_t *ifp)
{
	log_printf (IP_ID, 0, "DynIP: %s - removed!\r\n", ifp->name);
	if (ifp->up)
		di_intf_update (ifp, 0, ifp->allow, ifp->allow6);
	di_intf_delete (ifp);
}

void di_evh_begin (void)
{
	ipv4.n_changes = 0;
#ifdef DDS_IPV6
	ipv6.n_changes = 0;
#endif

}

static int notify_retry_ipv4 = 0;
#ifdef DDS_IPV6
static int notify_retry_ipv6 = 0;
#endif
void di_evh_end_run (unsigned int count);
 
void di_evh_end_timer (uintptr_t user)
{
	unsigned int count = (unsigned int) user;
#ifdef TRACE_DYNIP_NOTIFY
	log_printf (IP_ID, 0, "DynIP: di_evh_end_timer: %d\r\n", count);
#endif
	di_evh_end_run (count);
}

void di_evh_end_run (unsigned int count)
{
	int ok = 1;

	if (ipv4.n_changes || notify_retry_ipv4) {
#ifdef TRACE_DYNIP_NOTIFY
	    log_printf (IP_ID, 0, "DynIP: di_evh_end: ipv4.notify()\r\n");
#endif
		if ((*ipv4.notify) () == DDS_RETCODE_OK) {
			/* ok */
			notify_retry_ipv4 = 0;
		}
		else {
			ok = 0;
			notify_retry_ipv4 = 1;
			warn_printf ("DynIP: IPv4 notification failure");
		}
	}
#ifdef DDS_IPV6
	if (ipv6.n_changes || notify_retry_ipv6) {
#ifdef TRACE_DYNIP_NOTIFY
	    log_printf (IP_ID, 0, "DynIP: di_evh_end: ipv6.notify()\r\n");
#endif
		if ((*ipv6.notify) () == DDS_RETCODE_OK) {
			/* ok */
			notify_retry_ipv6 = 0;
		}
		else {
			notify_retry_ipv6 = 1;
			ok = 0;
			warn_printf ("DynIP: IPv6 notification failure");
		}
	}
#endif

	if (!ok) {
		count++;
		if (count <= 2) {
			/* retry */ 
		    log_printf (IP_ID, 0, "DynIP: di_evh_end: start retry timer: %d\r\n", count);
			tmr_start (&notify_timer, 200, count, di_evh_end_timer);
		}
		else {
			warn_printf ("DynIP: notification failure -- giving up");
		}
	}
}

void di_evh_end	(void)
{
	di_evh_end_run (0);
}

void di_update_intf_filters (uintptr_t user)
{
	IP_Intf_t	*ifp;
	const char	*ifs;
	unsigned	i;
	int		allow, allow6;

	ARG_NOT_USED (user)

	for (i = 0; i < MAX_IFTABLE; i++) {
		ifp = intf_table [i];
		if (!ifp)
			continue;

		if (config_defined (DC_IP_Intf)) {
			ifs = config_get_string (DC_IP_Intf, NULL);
			allow = slist_match (ifs, ifp->name, ':');
		}
		else
			allow = 1;
#ifdef DDS_IPV6
		if (config_defined (DC_IPv6_Intf)) {
			ifs = config_get_string (DC_IPv6_Intf, NULL);
			allow6 = slist_match (ifs, ifp->name, ':');
		}
		else
			allow6 = 1;
#else
		allow6 = 0;
#endif
		di_intf_update (ifp, ifp->up, allow, allow6);
	}
}

/* Restarting a timer is currently BROKEN.
 * Therefore we protect ourselves from multiple timer starts 
 * with this variable.
 * Initially we wanted to do this with tmr_active or always stopping the 
 * timer first with tmr_stop but this did not work (OMG).
 * Therefore we resorted to this variable...
 */
static int _di_filter_update_called;
void di_filter_update (void)
{
    if (_di_filter_update_called == 0){
        tmr_start (&sched_timer, 1, 0, di_update_intf_filters);
        ++_di_filter_update_called;
    }
}

int di_attach (unsigned      family,
	       unsigned char *ipa,
	       unsigned      *n,
	       unsigned      max,
	       Scope_t       min_scope,
	       Scope_t       max_scope,
	       DI_NOTIFY     fct)
{
	NL_Proto	*pp;

	if (!init)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	if (!ipa || !fct || !max)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (family == AF_INET)
		pp = &ipv4;
#ifdef DDS_IPV6
	else if (family == AF_INET6)
		pp = &ipv6;
#endif
	else
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!pp->own)
		n_proto++;
	pp->own = ipa;
	pp->num_own = n;
	pp->max_own = max;
	pp->notify = fct;
	pp->min_scope = min_scope;
	pp->max_scope = max_scope;

	di_sys_attach (family, ipa, n, max, min_scope, max_scope, fct);

	return (DDS_RETCODE_OK);
}

void di_detach (unsigned family)
{
	if (!init)
		return;

	if (family == AF_INET) {
		if (ipv4.own)
			n_proto--;
		ipv4.notify = NULL;
		ipv4.own = NULL;
	}
#ifdef DDS_IPV6
	else if (family == AF_INET6) {
		if (ipv6.own)
			n_proto--;
		ipv6.notify = NULL;
		ipv6.own = NULL;
	}
#endif
	di_sys_detach (family);
}

#ifdef DDS_IPV6

/* di_ipv6_intf -- Return an IPv6 interface index from an IPv6 address. */

unsigned di_ipv6_intf (unsigned char *addr)
{
	IP_Intf_t	*ifp;
	IP_Addr_t	*ap;
	unsigned	i;

	for (i = 0; i < MAX_IFTABLE; i++)
		for (ifp = intf_table [i]; ifp; ifp = ifp->next)
			for (ap = ifp->addr6; ap; ap = ap->next)
				if (!memcmp (ap->a_ipv6, addr, 16))
					return (ifp->id);
	return (0);
}

#endif
#ifdef DDS_DEBUG

static void di_intf_dump (IP_Intf_t *ifp)
{
	IP_Addr_t	*ap;
	int		comma;
#ifdef DDS_IPV6
	char		buf [100];
#endif

	dbg_printf ("%s:\t", ifp->name);
	if (ifp->up < 0)
		dbg_printf ("?   ");
	else if (ifp->up == 0)
		dbg_printf ("down");
	else
		dbg_printf ("up  ");
	if (ifp->allow)
		dbg_printf (" IP");
	else
		dbg_printf ("   ");
	if (ifp->allow6)
		dbg_printf (" IPv6");
	else
		dbg_printf ("     ");
	dbg_printf ("\t");
	comma = 0;
	for (ap = ifp->addr; ap; ap = ap->next) {
		if (comma)
			dbg_printf (", ");
		dbg_printf ("%u.%u.%u.%u",
				ap->a_ipv4 >> 24,
				(ap->a_ipv4 >> 16) & 0xff,
				(ap->a_ipv4 >> 8) & 0xff,
				ap->a_ipv4 & 0xff);
		comma = 1;
	}
#ifdef DDS_IPV6
	for (ap = ifp->addr6; ap; ap = ap->next) {
		if (comma)
			dbg_printf (", ");
		inet_ntop (AF_INET6, ap->a_ipv6, buf, sizeof (buf));
		dbg_printf ("%s", buf);
		comma = 1;
	}
#endif
	dbg_printf ("\r\n");
}

void di_dump (void)
{
	IP_Intf_t	*ifp;
	unsigned	i;

	for (i = 0; i < MAX_IFTABLE; i++)
		for (ifp = intf_table [i]; ifp; ifp = ifp->next)
			di_intf_dump (ifp);
}

#endif
