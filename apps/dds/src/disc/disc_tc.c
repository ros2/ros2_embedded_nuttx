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

/* disc_tc.h -- Implements the Typecode functions as used in Discovery. */

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif
#include "sys.h"
#include "log.h"
#include "str.h"
#include "error.h"
#ifdef DDS_TYPECODE
#include "vtc.h"
#include "dds_data.h"

/* tc_typesupport -- When a new type is discovered, this function must be used
		     to create the typesupport in order to have a real type. */
		     
TypeSupport_t *tc_typesupport (unsigned char *tc, const char *name)
{
	TypeSupport_t	*ts;
	char		*cp;
	unsigned	name_len;
	VTC_Header_t	*vh;

	name_len = strlen (name) + 1;
	ts = xmalloc (sizeof (TypeSupport_t) + name_len);
	if (!ts)
		return (NULL);

	memset (ts, 0, sizeof (TypeSupport_t));
	cp = (char *) (ts + 1);
	memcpy (cp, name, name_len);
	ts->ts_name = cp;
	ts->ts_prefer = MODE_V_TC;
	ts->ts_origin = TSO_Typecode;
	ts->ts_users = 1;
	vh = (VTC_Header_t *) tc;
	vh->nrefs_ext++;
	ts->ts_vtc = vh;
	return (ts);
}

/*#define LOG_TC_UNIQUE*/
#ifdef LOG_TC_UNIQUE
#define	tcu_print(s)		dbg_printf (s)
#define	tcu_print1(s,a1)	dbg_printf (s,a1)
#define	tcu_print2(s,a1,a2)	dbg_printf (s,a1,a2)
#define	tcu_print3(s,a1,a2,a3)	dbg_printf (s,a1,a2,a3)
#else
#define	tcu_print(s)
#define	tcu_print1(s,a1)
#define	tcu_print2(s,a1,a2)
#define	tcu_print3(s,a1,a2,a3)
#endif

/* tc_unique -- Return unique typecode data by comparing the proposed data
		with the data of other topic endpoints.  If alternative
		typecode data is found, the proposed data is released and the
		existing data is reused. */

unsigned char *tc_unique (Topic_t       *tp,
			  Endpoint_t    *ep, 
			  unsigned char *tc,
			  int           *incompatible)
{
	VTC_Header_t	*hp;
	unsigned char	*ntc, *xtc;
	Endpoint_t	*xep;
	int		same;

	ntc = NULL;
	tcu_print2 ("tc_unique(@%p, %p):", ep, tc);
	*incompatible = 0;
	do {
		if (tp->type->type_support) {
			if (tp->type->type_support->ts_prefer >= MODE_V_TC) {
				tcu_print2 ("{T:%p*%u}", tp->type->type_support->ts_vtc,
						 tp->type->type_support->ts_vtc->nrefs_ext);
				if (vtc_equal ((unsigned char *) tp->type->type_support->ts_vtc, tc)) {
					ntc = (unsigned char *) tp->type->type_support->ts_vtc;
					break;
				}
			}
			else {
				if (!vtc_compatible (tp->type->type_support,
						     tc,
						     &same))
					*incompatible = 1;
				else if (same) {
					tcu_print (" use ~0!\r\n");
					xfree (tc);
					return (TC_IS_TS);
				}
			}
		}
		for (xep = tp->writers; xep; xep = xep->next) {
			if (xep == ep || !entity_discovered (xep->entity.flags))
				continue;

			xtc = ((DiscoveredWriter_t *) xep)->dw_tc;
			if (!xtc || xtc == TC_IS_TS)
				continue;

			tcu_print3 ("{W%p:%p*%u}", xep, xtc, ((VTC_Header_t *) xtc)->nrefs_ext);
			if (vtc_equal (xtc, tc)) {
				ntc = xtc;
				break;
			}
		}
		if (ntc)
			break;

		for (xep = tp->readers; xep; xep = xep->next) {
			if (xep == ep || !entity_discovered (xep->entity.flags))
				continue;

			xtc = ((DiscoveredReader_t *) xep)->dr_tc;
			if (!xtc || xtc == TC_IS_TS)
				continue;

			tcu_print3 ("{R%p:%p*%u}", xep, xtc, ((VTC_Header_t *) xtc)->nrefs_ext);
			if (vtc_equal (xtc, tc)) {
				ntc = xtc;
				break;
			}
		}
	}
	while (0);
	if (ntc) {
		xtc = ntc;
		hp = (VTC_Header_t *) ntc;
		hp->nrefs_ext++;
		xfree (tc);
	}
	else
		xtc = tc;
	tcu_print1 (" use %p!\r\n", xtc);
	return (xtc);
}

/* tc_update -- Attempts to update the Typecode of a Discovered Reader. */

int tc_update (Endpoint_t    *ep,
	       unsigned char **ep_tc,
	       unsigned char **new_tc,
	       int           *incompatible)
{
	TopicType_t	*ttp = ep->topic->type;
	TypeSupport_t	*tsp = (TypeSupport_t *) ttp->type_support;

	tcu_print2 ("tc_update(@%p,%p)", *ep, *new_tc);
	*incompatible = 0;
	if (!tsp) {
		tsp = tc_typesupport (*ep_tc, str_ptr (ttp->type_name));
		if (!tsp)
			return (0);

		*ep_tc = *new_tc;
		tcu_print (" - new TS\r\n");
	}
	else if (!*ep_tc || *ep_tc == TC_IS_TS)
		*ep_tc = tc_unique (ep->topic, ep, *new_tc, incompatible);
	else if (vtc_equal (*ep_tc, *new_tc)) {	/* Same as previous -- just reuse previous. */
		xfree (*new_tc);
		tcu_print (" - same\r\n");
	}
	else if (tsp->ts_prefer >= MODE_V_TC &&	/* Typesupport *is* endpoint type? */
	         tsp->ts_vtc == (VTC_Header_t *) *ep_tc) {
		if ((tsp->ts_vtc->nrefs_ext & NRE_NREFS) == 2) { /* No one else using it? */
			xfree (*ep_tc);	/* -> then just update it in both locations. */
			*ep_tc = NULL;
			tsp->ts_vtc = NULL;
			tcu_print (" - replace\r\n");
			*ep_tc = tc_unique (ep->topic, ep, *new_tc, incompatible);
			tsp->ts_vtc = (VTC_Header_t *) *ep_tc;
			tsp->ts_vtc->nrefs_ext++;
		}
		else {				/* Multiple endpoints using old typecode! */
			tsp->ts_vtc->nrefs_ext--;
			*ep_tc = NULL;
			tcu_print (" - update(1)\r\n");
			*ep_tc = tc_unique (ep->topic, ep, *new_tc, incompatible);
		}
	}
	else {
		vtc_free (*ep_tc);
		*ep_tc = NULL;
		tcu_print (" - update(2)\r\n");
		*ep_tc = tc_unique (ep->topic, ep, *new_tc, incompatible);
	}
	*new_tc = NULL;
	return (1);
}

#endif /* DDS_TYPECODE */


