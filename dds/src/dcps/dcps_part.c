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

/* dcps_part.c -- DCPS API - DomainParticipant methods. */

#include "sys.h"
#include "ctrace.h"
#include "prof.h"
#include "log.h"
#include "pool.h"
#include "str.h"
#include "error.h"
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "dds_data.h"
#include "dds.h"
#include "guard.h"
#include "parse.h"
#include "dcps.h"
#include "dcps_priv.h"
#include "domain.h"
#include "disc.h"
#include "dcps_dpfact.h"
#include "dcps_event.h"
#include "dcps_builtin.h"
#include "dcps_pub.h"
#include "dcps_sub.h"
#ifdef DDS_SECURITY
#include "security.h"
#endif
#include "xtypes.h"

/* Deprecated function:: applications should use DDS_TypeSupport_get_type_name(). */

const char *DDS_DomainParticipant_get_type_name (DDS_TypeSupport ts)
{
	return (DDS_TypeSupport_get_type_name (ts));
}

#ifdef DDS_TYPECODE

typedef struct tptc_st {
	TopicType_t		*type;
	const TypeSupport_t	*ts;
	unsigned char		*eq_tc;
	unsigned		ntopics;
} TPTC_t;

static void topic_type_promote_tc_eps (Endpoint_t *ep, TPTC_t *tp, int dwriter)
{
	unsigned char	**tcp;

	for (; ep; ep = ep->next) {
		if (!entity_discovered (ep->entity.flags))
			continue;

		if (dwriter)
			tcp = &((DiscoveredWriter_t *) ep)->dw_tc;
		else
			tcp = &((DiscoveredReader_t *) ep)->dr_tc;
		if (!*tcp || *tcp == (unsigned char *) ~0UL)
			continue;

		if ((tp->eq_tc && 
		     (*tcp == tp->eq_tc || vtc_equal (tp->eq_tc, *tcp))) ||
		    vtc_identical (tp->ts, *tcp)) {
			vtc_free (*tcp);
			*tcp = (unsigned char *) ~0UL;
		}
	}
}

static int topic_type_promote_tc_fct (Skiplist_t *list, void *node, void *arg)
{
	Topic_t		*tp, **tpp = (Topic_t **) node;
	TPTC_t		*tptc = (TPTC_t *) arg;

	ARG_NOT_USED (list)

	tp = *tpp;
	if (tp->nlrefs && tp->type == tptc->type) {
		topic_type_promote_tc_eps (tp->writers, tptc, 1);
		topic_type_promote_tc_eps (tp->readers, tptc, 0);
		tptc->ntopics--;
	}
	return (tptc->ntopics != 0);
}

static void type_promote_tc (Domain_t            *dp,
			     TopicType_t         *typep,
			     const TypeSupport_t *ts,
			     unsigned char       *tc)
{
	TPTC_t	tptc;

	tptc.type = typep;
	tptc.ts = ts;
	tptc.eq_tc = tc;
	tptc.ntopics = typep->nrefs;
	sl_walk (&dp->participant.p_topics, topic_type_promote_tc_fct, &tptc);
}
#endif

DDS_ReturnCode_t DDS_DomainParticipant_register_type (DDS_DomainParticipant dp,
						      DDS_TypeSupport       ts,
						      const char            *type_name)
{
	TopicType_t		*typep;
	TypeSupport_t		*pts;
	DDS_ReturnCode_t	ret;
#ifdef DDS_TYPECODE
	unsigned char		*vtc;
#endif

	ctrc_begind (DCPS_ID, DCPS_DP_R_TYPE, &dp, sizeof (dp));
	ctrc_contd (&ts, sizeof (ts));
	ctrc_contd (type_name, strlen (type_name));
	ctrc_endd ();

	prof_start (dcps_reg_type);

	/* Validate some required arguments. */
	if (!ts)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if (!type_name &&
	    (type_name = DDS_TypeSupport_get_type_name (ts)) == NULL)
		return (DDS_RETCODE_BAD_PARAMETER);

	/* Search type name in participant type list. */
	typep = type_lookup (dp, type_name);
	if (typep) {
		if ((pts = typep->type_support) != NULL) {
#ifdef DDS_TYPECODE
			/* Type already learned from remote peers? */
			if (pts->ts_prefer >= MODE_V_TC) {
				vtc = (unsigned char *) pts->ts_vtc;
				if (!vtc_identical (ts, vtc)) {

					/* Oops: already present in domain but 
					   incompatible - notify later when
					   a topic is created. */
					typep->flags |= EF_INC_TYPE;
					type_promote_tc (dp, typep, ts, NULL);
				}
				else {
					typep->flags &= ~EF_INC_TYPE;
					type_promote_tc (dp, typep, ts, vtc);
				}

				/* Replace with real type support.*/
				xfree (typep->type_support);
				typep->type_support = (TypeSupport_t *) ts;
				vtc_free (vtc);
			}
			else

			/* Already defined: check compatibility. */
			if (ts->ts_prefer   != pts->ts_prefer ||
			    ts->ts_keys     != pts->ts_keys ||
			    ts->ts_fksize   != pts->ts_fksize ||
			    ts->ts_length   != pts->ts_length ||
			    ts->ts_mkeysize != pts->ts_mkeysize)
				return (DDS_RETCODE_PRECONDITION_NOT_MET);

			else if (ts->ts_prefer == MODE_CDR) {
				if (!xt_type_equal (ts->ts_cdr, pts->ts_cdr))
					return (DDS_RETCODE_PRECONDITION_NOT_MET);
			}
			else if (ts->ts_prefer == MODE_PL_CDR) {
				if (pts->ts_pl->builtin ||
				    ts->ts_pl->builtin ||
				    !xt_type_equal (ts->ts_pl->xtype, pts->ts_pl->xtype))
					return (DDS_RETCODE_PRECONDITION_NOT_MET);
			}
			else
#else
			if (pts != ts)
#endif
				return (DDS_RETCODE_PRECONDITION_NOT_MET);
		}
		else {
			typep->type_support = (TypeSupport_t *) ts;
			typep->type_support->ts_users++;
		}
	}
	else {
		/* Doesn't exist yet -- allocate new type context. */
		typep = type_create (dp, type_name, NULL);
		if (!typep) {
			warn_printf ("create_topic_type (%s): out of memory for topic type!\r\n", type_name);
			lock_release (dp->lock);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		typep->type_support = (TypeSupport_t *) ts;
		typep->type_support->ts_users++;
	}
	typep->flags |= EF_LOCAL;
	typep->nrefs++;
	typep->nlrefs++;
	lock_release (dp->lock);
	prof_stop (dcps_reg_type, 1);
	return (DDS_RETCODE_OK);
}

#ifdef XTYPES_USED

DDS_ReturnCode_t DDS_DynamicTypeSupport_register_type (DDS_DynamicTypeSupport dts,
						       DDS_DomainParticipant  p,
						       const DDS_ObjectName   name)
{
	TypeSupport_t	*ts = (TypeSupport_t *) dts;

	if (!p || !dts
#ifdef DDS_TYPECODE
	    || ts->ts_origin != TSO_Dynamic
#endif
	                                   )
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_DomainParticipant_register_type (p, ts, name));
}

#endif

struct type_lookup_st {
	TopicType_t	*type;
	unsigned	nusers;
};

static int topic_type_check_fct (Skiplist_t *list, void *node, void *arg)
{
	Topic_t			*tp, **tpp = (Topic_t **) node;
	struct type_lookup_st	*lookup = (struct type_lookup_st *) arg;

	ARG_NOT_USED (list)

	tp = *tpp;
	if (tp->nlrefs && tp->type == lookup->type) {
		lookup->nusers++;
	}
	return (1);
}

DDS_ReturnCode_t DDS_DomainParticipant_unregister_type (DDS_DomainParticipant dp,
							DDS_TypeSupport       ts,
							const char            *type_name)
{
	TopicType_t		*typep;
	struct type_lookup_st	lookup;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_U_TYPE, &dp, sizeof (dp));
	ctrc_contd (&ts, sizeof (ts));
	ctrc_contd (type_name, strlen (type_name));
	ctrc_endd ();

	prof_start (dcps_unreg_type);

	/* Validate some required arguments. */
	if (!ts)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	/* Search type name in participant type list. */
	typep = type_lookup (dp, type_name);
	if (!typep ||
#if !defined DDS_TYPECODE
	    typep->type_support != ts ||
#endif
	    (typep->flags & EF_LOCAL) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}

	/* Type exists and is locally created.
	   Check if there are local topics that are still using the type. */
	lookup.type = typep;
	lookup.nusers = 0;
	sl_walk (&dp->participant.p_topics, topic_type_check_fct, &lookup);
	if (lookup.nusers) {
		if (typep->nrefs > lookup.nusers + 1) {
			ret = DDS_RETCODE_OK;
			typep->nrefs--;
			if (--typep->nlrefs == 0) {
				typep->flags &= ~EF_LOCAL;
				DDS_TypeSupport_delete(typep->type_support);
			}
		} 
		else 
			ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		lock_release (dp->lock);	/* Still in use! */
		return (ret);
	}
	if (--typep->nlrefs == 0) {
		typep->flags &= ~EF_LOCAL;
		DDS_TypeSupport_delete(typep->type_support);
		typep->type_support = NULL;
	}
	type_delete (dp, typep);
	lock_release (dp->lock);
	prof_stop (dcps_unreg_type, 1);
	return (DDS_RETCODE_OK);
}

#ifdef XTYPES_USED

DDS_ReturnCode_t DDS_DynamicTypeSupport_unregister_type (DDS_DynamicTypeSupport dts,
						         DDS_DomainParticipant  p,
						         const DDS_ObjectName   name)
{
	TypeSupport_t	*ts = (TypeSupport_t *) dts;

	if (!p || !dts
#ifdef DDS_TYPECODE
	    || ts->ts_origin != TSO_Dynamic
#endif
	                                   )
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_DomainParticipant_unregister_type (p, ts, name));
}

#endif

struct type_sup_rem_state_st {
	DDS_DomainParticipant	part;
	TypeSupport_t		*ts;
	DDS_ReturnCode_t	ret;
};

static int delete_type_support (Skiplist_t *list, void *node, void *arg)
{
	TopicType_t			*typep, **typepp = (TopicType_t **) node;
	struct type_sup_rem_state_st	*state = (struct type_sup_rem_state_st *) arg;

	ARG_NOT_USED (list)

	typep = *typepp;
	if ((typep->flags & EF_LOCAL) != 0 &&
	    typep->type_support == state->ts) {
		state->ret = DDS_DomainParticipant_unregister_type (state->part,
					    state->ts,
					    (void *) str_ptr (typep->type_name));
		return (0);
	}
	return (1);
}

DDS_ReturnCode_t DDS_DomainParticipant_delete_typesupport (DDS_DomainParticipant dp,
							   DDS_TypeSupport ts)
{
	struct type_sup_rem_state_st	state;
	DDS_ReturnCode_t		ret;

	ctrc_begind (DCPS_ID, DCPS_DP_D_TS, &dp, sizeof (dp));
	ctrc_contd (&ts, sizeof (ts));
	ctrc_endd ();

	/* Validate some required arguments. */
	if (!ts)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	/* Search type name in participant type list. */
	state.part = dp;
	state.ts = ts;
	state.ret = DDS_RETCODE_ALREADY_DELETED;
	sl_walk (&dp->types, delete_type_support, &state);
	lock_release (dp->lock);
	return (state.ret);
}

DDS_ReturnCode_t DDS_DomainParticipant_get_qos (DDS_DomainParticipant dp,
						DDS_DomainParticipantQos *qos)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_G_QOS, &dp, sizeof (dp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!qos)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	qos_str2octets (dp->participant.p_user_data, &qos->user_data.value);
	qos->entity_factory.autoenable_created_entities = dp->autoenable;
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DomainParticipant_set_qos (DDS_DomainParticipant dp,
						DDS_DomainParticipantQos *qos)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_S_QOS, &dp, sizeof (dp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if (qos == DDS_PARTICIPANT_QOS_DEFAULT)
		qos = &dcps_def_participant_qos;
	else if (!qos_valid_participant_qos (qos)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	if (dp->participant.p_user_data)
		str_unref (dp->participant.p_user_data);
	dp->participant.p_user_data = qos_octets2str (&qos->user_data.value);
	dp->autoenable = qos->entity_factory.autoenable_created_entities;
	if ((dp->participant.p_flags & EF_ENABLED) != 0)
		ret = disc_participant_update (dp);

    done:
	lock_release (dp->lock);
	return (ret);
}

DDS_DomainParticipantListener *DDS_DomainParticipant_get_listener (DDS_DomainParticipant dp)
{
	DDS_DomainParticipantListener	*listener;

	ctrc_printd (DCPS_ID, DCPS_DP_G_LIS, &dp, sizeof (dp));

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, NULL)) {
		log_printf (DCPS_ID, 0, "get_listener: domain doesn't exist!\r\n");
		return (NULL);
	}
	listener = &dp->listener;
	lock_release (dp->lock);
	return (listener);
}

DDS_ReturnCode_t DDS_DomainParticipant_set_listener (DDS_DomainParticipant dp,
						     DDS_DomainParticipantListener *listener,
						     DDS_StatusMask mask)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_S_LIS, &dp, sizeof (dp));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, &ret)) {
		log_printf (DCPS_ID, 0, "set_listener: domain doesn't exist!\r\n");
		return (ret);
	}
	dcps_update_listener ((Entity_t *) dp, &dp->lock,
			      &dp->mask, &dp->listener,
			      mask, listener);
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_StatusMask DDS_DomainParticipant_get_status_changes (DDS_DomainParticipant dp)
{
	DDS_StatusMask	m;
	
	ctrc_printd (DCPS_ID, DCPS_DP_G_STAT, &dp, sizeof (dp));
	if (!domain_ptr (dp, 1, NULL))
		return (0);

	/* No status mask on participant for now! */
	m = 0U;

	lock_release (dp->lock);
	return (m);
}

DDS_ReturnCode_t DDS_DomainParticipant_enable (DDS_DomainParticipant dp)
{
#ifdef RTPS_USED
	int			i;
#endif
#if defined (DDS_DEBUG) && defined (DDS_SERVER)
	static int		server_running = 0;
#endif
	DDS_ReturnCode_t	error;

	ctrc_printd (DCPS_ID, DCPS_DP_ENABLE, &dp, sizeof (dp));
	if (!domain_ptr (dp, 1, &error))
		return (error);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		dp->participant.p_flags |= EF_ENABLED | EF_NOT_IGNORED;

#ifdef RTPS_USED

		if (rtps_used)

			/* Create RTPS and Discovery participants. */
			for (i = 0; i < 64; i++) {
				error = rtps_participant_create (dp);
				if (error &&
				    error != DDS_RETCODE_PRECONDITION_NOT_MET) {
					lock_release (dp->lock);
					return (error);
				}
				if (!error)
					break;

				domain_used (dp);
				
				if (dp->participant_id == ILLEGAL_PID) 
					return (DDS_RETCODE_OUT_OF_RESOURCES);
			}
#endif
#if defined (DDS_DEBUG) && defined (DDS_SERVER)
		if (!server_running) {
			server_running = 1;
			DDS_Debug_server_start (2, DDS_DEBUG_PORT_DEFAULT);
		}
#endif
	}
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_StatusCondition DDS_DomainParticipant_get_statuscondition (DDS_DomainParticipant dp)
{
	StatusCondition_t	*scp;

	ctrc_printd (DCPS_ID, DCPS_DP_G_SCOND, &dp, sizeof (dp));

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, NULL)) {
		log_printf (DCPS_ID, 0, "get_statuscondition: domain doesn't exist!\r\n");
		return (NULL);
	}
	scp = (StatusCondition_t *) dp->condition;
	if (!scp) {
		scp = dcps_new_status_condition ();
		if (!scp)
			return (NULL);

		scp->entity = (Entity_t *) dp;
		dp->condition = scp;
	}
	lock_release (dp->lock);
	return ((DDS_StatusCondition) scp);
}

DDS_InstanceHandle_t DDS_DomainParticipant_get_instance_handle (DDS_DomainParticipant dp)
{
	DDS_InstanceHandle_t	handle;

	ctrc_printd (DCPS_ID, DCPS_DP_G_HANDLE, &dp, sizeof (dp));

	if (!domain_ptr (dp, 1, NULL))
		return (DDS_HANDLE_NIL);

	handle = dp->participant.p_handle;
	lock_release (dp->lock);
	return (handle);
}

DDS_ReturnCode_t DDS_DomainParticipant_get_default_topic_qos (DDS_DomainParticipant dp,
							      DDS_TopicQos          *qos)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_G_T_QOS, &dp, sizeof (dp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!qos)
		return (DDS_RETCODE_BAD_PARAMETER);

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, &ret)) {
		log_printf (DCPS_ID, 0, "get_default_topic_qos: domain doesn't exist!\r\n");
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	*qos = dp->def_topic_qos;
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DomainParticipant_set_default_topic_qos (DDS_DomainParticipant dp,
							      DDS_TopicQos          *qos)
{
	DDS_ReturnCode_t	ret = DDS_RETCODE_OK;

	ctrc_begind (DCPS_ID, DCPS_DP_S_T_QOS, &dp, sizeof (dp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, &ret)) {
		log_printf (DCPS_ID, 0, "set_default_topic_qos: domain doesn't exist!\r\n");
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (qos == DDS_TOPIC_QOS_DEFAULT)
		dp->def_topic_qos = qos_def_topic_qos;
	else if (qos_valid_topic_qos (qos))
		dp->def_topic_qos = *qos;
	else
		ret = DDS_RETCODE_INCONSISTENT_POLICY;

	lock_release (dp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DomainParticipant_set_default_subscriber_qos (DDS_DomainParticipant dp,
								   DDS_SubscriberQos *qos)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_S_S_QOS, &dp, sizeof (dp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, &ret)) {
		log_printf (DCPS_ID, 0, "set_default_subscriber_qos: domain doesn't exist!\r\n");
		return (ret);
	}
	if (qos == DDS_SUBSCRIBER_QOS_DEFAULT)
		dp->def_subscriber_qos = qos_def_subscriber_qos;
	else if (qos_valid_subscriber_qos (qos))
		dp->def_subscriber_qos = *qos;
	else
		ret = DDS_RETCODE_INCONSISTENT_POLICY;

	lock_release (dp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DomainParticipant_get_default_subscriber_qos (DDS_DomainParticipant dp,
								   DDS_SubscriberQos *qos)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_G_S_QOS, &dp, sizeof (dp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!qos)
		return (DDS_RETCODE_BAD_PARAMETER);

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, &ret)) {
		log_printf (DCPS_ID, 0, "get_default_subscriber_qos: domain doesn't exist!\r\n");
		return (ret);
	}
	*qos = dp->def_subscriber_qos;
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DomainParticipant_set_default_publisher_qos (DDS_DomainParticipant dp,
								  DDS_PublisherQos *qos)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_S_P_QOS, &dp, sizeof (dp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, &ret)) {
		log_printf (DCPS_ID, 0, "set_default_publisher_qos: domain doesn't exist!\r\n");
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (qos == DDS_PUBLISHER_QOS_DEFAULT)
		dp->def_publisher_qos = qos_def_publisher_qos;
	else if (qos_valid_publisher_qos (qos))
		dp->def_publisher_qos = *qos;
	else
		ret = DDS_RETCODE_INCONSISTENT_POLICY;

	lock_release (dp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DomainParticipant_get_default_publisher_qos (DDS_DomainParticipant dp,
								  DDS_PublisherQos *qos)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_G_P_QOS, &dp, sizeof (dp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, &ret)) {
		log_printf (DCPS_ID, 0, "get_default_publisher_qos: domain doesn't exist!\r\n");
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (!qos)
		ret = DDS_RETCODE_BAD_PARAMETER;
	else
		*qos = dp->def_publisher_qos;
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}


int dcps_delete_topic (Skiplist_t *list, void *node, void *arg)
{
	Topic_t			*tp, **tpp = (Topic_t **) node;
	DDS_DomainParticipant	p = (DDS_DomainParticipant) arg;
	unsigned		n;

	ARG_NOT_USED (list)

	tp = *tpp;
	if ((tp->entity.flags & EF_BUILTIN) == 0) {
		dtrc_printf ("participant_delete_contained_entitities: delete Topic (%s)\r\n", 
						str_ptr (tp->name));
		for (n = tp->nlrefs; n; n--)	/* Make sure we *really* delete it! */
			DDS_DomainParticipant_delete_topic (p, tp);
	}
	return (1);
}

const char *DDS_TypeSupport_get_type_name (DDS_TypeSupport ts)
{
	return (ts ? ts->ts_name : NULL);
}


static int delete_type (Skiplist_t *list, void *node, void *arg)
{
	TopicType_t		*ttp, **ttpp = (TopicType_t **) node;
	DDS_DomainParticipant	p = (DDS_DomainParticipant) arg;

	ARG_NOT_USED (list)

	ttp = *ttpp;
	if ((ttp->flags & (EF_BUILTIN | EF_LOCAL)) == EF_LOCAL) {
		dtrc_printf ("participant_delete_contained_entitities: unregister Type (%s)\r\n", 
							str_ptr (ttp->type_name));
		DDS_DomainParticipant_unregister_type (p, ttp->type_support, 
							str_ptr (ttp->type_name));
	}
	return (1);
}

DDS_ReturnCode_t DDS_DomainParticipant_delete_contained_entities (DDS_DomainParticipant dp)
{
	Publisher_t		*up;
	Subscriber_t		*sp;
	Condition_t		*cp;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_DP_D_CONT, &dp, sizeof (dp));
	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	while ((up = dp->publishers.head) != NULL) {
		delete_publisher_entities (dp, up);
                if (up->nwriters != 0) {
			lock_release (dp->lock);
                        return (DDS_RETCODE_PRECONDITION_NOT_MET);
		}
		dtrc_printf ("participant_delete_contained_entitities: delete Publisher (%p)\r\n", (void *) up);
		qos_publisher_free (&up->qos);
		if (up->condition) {
			cp = (Condition_t *) up->condition;
			if (cp->deferred)
				dds_defer_waitset_undo (up, up->condition);
			dcps_delete_status_condition (up->condition);
		}
		publisher_delete (up);
	}
	while ((sp = dp->subscribers.head) != NULL) {
		delete_subscriber_entities (dp, sp);
                if (sp->nreaders != 0) {
			lock_release (dp->lock);
                        return (DDS_RETCODE_PRECONDITION_NOT_MET);
		}
		dtrc_printf ("participant_delete_contained_entitities: delete Subscriber (%p)\r\n", (void *) sp);
		qos_subscriber_free (&sp->qos);
		if (sp->condition) {
			cp = (Condition_t *) sp->condition;
			if (cp->deferred)
				dds_defer_waitset_undo (sp, sp->condition);
			dcps_delete_status_condition (sp->condition);
		}
		subscriber_delete (sp);
	}
	sl_walk (&dp->participant.p_topics, dcps_delete_topic, dp);
	sl_walk (&dp->types, delete_type, dp);
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

int DDS_DomainParticipant_contains_entity (DDS_DomainParticipant dp,
					   DDS_InstanceHandle_t handle)
{
	Entity_t	*ep;
	int		contained = 0;

	ctrc_begind (DCPS_ID, DCPS_DP_CONT, &dp, sizeof (dp));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	if (!domain_ptr (dp, 1, NULL))
		return (0);

	ep = entity_ptr (handle);
	if (!ep)
		goto done;

	if ((ep->flags & EF_LOCAL) == 0)
		goto done;

	switch (ep->type) {
		case ET_TOPIC:
			contained = (((Topic_t *) ep)->domain == dp);
			break;

		case ET_PUBLISHER:
			contained = (((Publisher_t *) ep)->domain == dp);
			break;

		case ET_SUBSCRIBER:
			contained = (((Subscriber_t *) ep)->domain == dp);
			break;

		case ET_WRITER:
			contained = (((Writer_t *) ep)->w_publisher->domain == dp);
			break;

		case ET_READER:
			contained = (((Reader_t *) ep)->r_subscriber->domain == dp);
			break;

		case ET_PARTICIPANT:
		default:
			break;
	}

done:
	lock_release (dp->lock);
	return (contained);
}

DDS_DomainId_t DDS_DomainParticipant_get_domain_id (DDS_DomainParticipant dp)
{
	DDS_DomainId_t		id;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_DP_G_ID, &dp, sizeof (dp));
	if (!domain_ptr (dp, 1, &ret))
		return (0);

	id = dp->domain_id;
	lock_release (dp->lock);
	return (id);
}

DDS_ReturnCode_t DDS_DomainParticipant_get_current_time (DDS_DomainParticipant dp,
							 DDS_Time_t *current_time)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_G_TIME, &dp, sizeof (dp));
	ctrc_contd (current_time, sizeof (*current_time));
	ctrc_endd ();

	if (!current_time)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	sys_gettime ((Time_t *) current_time);
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_Subscriber DDS_DomainParticipant_get_builtin_subscriber (DDS_DomainParticipant dp)
{
	Subscriber_t	*sp;
	
	ctrc_printd (DCPS_ID, DCPS_DP_G_BI_SUB, &dp, sizeof (dp));

	if (!domain_ptr (dp, 1, NULL))
		return (NULL);

	if ((sp = dp->builtin_subscriber) == NULL) {
		sp = dp->builtin_subscriber = subscriber_create (dp, 1);
		if (sp) {
			qos_subscriber_new (&sp->qos, &qos_def_subscriber_qos);
			sp->def_reader_qos = qos_def_reader_qos;
		}
	}
	lock_release (dp->lock);
	return (sp);
}

DDS_ReturnCode_t DDS_DomainParticipant_ignore_participant (DDS_DomainParticipant dp,
							   DDS_InstanceHandle_t handle)
{
	Entity_t		*ep;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_IGN_PART, &dp, sizeof (dp));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	ep = entity_ptr (handle);
	if (!ep ||
	    ep->type != ET_PARTICIPANT ||
	    !entity_discovered (ep->flags)) {
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto done;
	}
	ret = disc_ignore_participant ((Participant_t *) ep);

    done:
	lock_release (dp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DomainParticipant_ignore_subscription (DDS_DomainParticipant dp,
						            DDS_InstanceHandle_t handle)
{
	Entity_t		*ep;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_IGN_SUB, &dp, sizeof (dp));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	ep = entity_ptr (handle);
	if (!ep ||
	    ep->type != ET_READER ||
	    !entity_discovered (ep->flags)) {
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto done;
	}
	ret = disc_ignore_reader ((DiscoveredReader_t *) ep);

    done:
	lock_release (dp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DomainParticipant_ignore_publication (DDS_DomainParticipant dp,
						           DDS_InstanceHandle_t handle)
{
	Entity_t		*ep;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_IGN_PUB, &dp, sizeof (dp));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	ep = entity_ptr (handle);
	if (!ep ||
	    ep->type != ET_WRITER ||
	    !entity_discovered (ep->flags)) {
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto done;
	}
	ret = disc_ignore_writer ((DiscoveredWriter_t *) ep);

    done:
	lock_release (dp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DomainParticipant_assert_liveliness (DDS_DomainParticipant dp)
{
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_DP_ASSERT, &dp, sizeof (dp));

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	liveliness_participant_assert (dp);
	lock_release (dp->lock);
	return (ret);
}

static int participant_add_handle (Skiplist_t *list, void *node, void *arg)
{
	const Participant_t	*pp, **ppp = (const Participant_t **) node;
	DDS_InstanceHandleSeq	*handles = (DDS_InstanceHandleSeq *) arg;
	DDS_InstanceHandle_t	h;

	ARG_NOT_USED (list)

	pp = *ppp;
	if (entity_ignored (pp->p_entity.flags))
		return (1);

	h = pp->p_handle;
	return (dds_seq_append (handles, &h) == 0);
}

DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_participants (
					DDS_DomainParticipant dp,
					DDS_InstanceHandleSeq *handles)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_G_DISC_P_S, &dp, sizeof (dp));
	ctrc_contd (&handles, sizeof (handles));
	ctrc_endd ();

	if (!handles)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	DDS_SEQ_INIT (*handles);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	sl_walk (&dp->peers, participant_add_handle, handles);
	lock_release (dp->lock);
	return (ret);
}

void DDS_OctetSeq__init (DDS_OctetSeq *octets)
{
	DDS_SEQ_INIT (*octets);
}

void DDS_OctetSeq__clear (DDS_OctetSeq *octets)
{
	dds_seq_cleanup (octets);
}

DDS_OctetSeq *DDS_OctetSeq__alloc (void)
{
	DDS_OctetSeq	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_OctetSeq));
	if (!p)
		return (NULL);

	DDS_OctetSeq__init (p);
	return (p);
}

void DDS_OctetSeq__free (DDS_OctetSeq *octets)
{
	if (!octets)
		return;

	DDS_OctetSeq__clear (octets);
	mm_fcts.free_ (octets);
}

void DDS_ParticipantBuiltinTopicData__init (DDS_ParticipantBuiltinTopicData *data)
{
	DDS_SEQ_INIT (data->user_data.value);
}

void DDS_ParticipantBuiltinTopicData__clear (DDS_ParticipantBuiltinTopicData *data)
{
	dds_seq_cleanup (&data->user_data.value);
}

DDS_ParticipantBuiltinTopicData *DDS_ParticipantBuiltinTopicData__alloc (void)
{
	DDS_ParticipantBuiltinTopicData	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_ParticipantBuiltinTopicData));
	if (!p)
		return (NULL);

	DDS_ParticipantBuiltinTopicData__init (p);
	return (p);
}

void DDS_ParticipantBuiltinTopicData__free (DDS_ParticipantBuiltinTopicData *data)
{
	if (!data)
		return;

	DDS_ParticipantBuiltinTopicData__clear (data);
	mm_fcts.free_ (data);
}

DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_participant_data (
					DDS_DomainParticipant           dp,
					DDS_ParticipantBuiltinTopicData *data,
					DDS_InstanceHandle_t            handle)
{
	Entity_t		*ep;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_G_DISC_P, &dp, sizeof (dp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	if (!data || !handle)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	ep = entity_ptr (handle);
	if (!ep ||
	     ep->type != ET_PARTICIPANT ||
	     !entity_discovered (ep->flags)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	if (dcps_get_builtin_participant_data (data, (Participant_t *) ep)) {
		ret = DDS_RETCODE_OUT_OF_RESOURCES;
		goto done;
	}

    done:
	lock_release (dp->lock);
	return (ret);
}

static int topic_add_handle (Skiplist_t *list, void *node, void *arg)
{
	const Topic_t		*tp, **tpp = (const Topic_t **) node;
	DDS_InstanceHandleSeq	*handles = (DDS_InstanceHandleSeq *) arg;
	DDS_InstanceHandle_t	h;

	ARG_NOT_USED (list)

	tp = *tpp;
	if (!entity_discovered (tp->entity.flags) ||
	    entity_ignored (tp->entity.flags))
		return (1);

	h = tp->entity.handle;
	return (dds_seq_append (handles, &h) == 0);
}

DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_topics (DDS_DomainParticipant dp,
							      DDS_InstanceHandleSeq *handles)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_G_DISC_T_S, &dp, sizeof (dp));
	ctrc_contd (&handles, sizeof (handles));
	ctrc_endd ();

	if (!handles)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	DDS_SEQ_INIT (*handles);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	sl_walk (&dp->participant.p_topics, topic_add_handle, handles);
	lock_release (dp->lock);
	return (ret);
}

void DDS_TopicBuiltinTopicData__init (DDS_TopicBuiltinTopicData *data)
{
	memset (data, 0, sizeof (DDS_TopicBuiltinTopicData));
	DDS_SEQ_INIT (data->topic_data.value);
}

void DDS_TopicBuiltinTopicData__clear (DDS_TopicBuiltinTopicData *data)
{
	if (data->name) {
		xfree (data->name);
		data->name = NULL;
	}
	DDS_TopicBuiltinTopicData__init (data);
}

DDS_TopicBuiltinTopicData *DDS_TopicBuiltinTopicData__alloc (void)
{
	DDS_TopicBuiltinTopicData	*p;

	p = xmalloc (sizeof (DDS_TopicBuiltinTopicData));
	if (!p)
		return (NULL);

	DDS_TopicBuiltinTopicData__init (p);
	return (p);
}

void DDS_TopicBuiltinTopicData__free (DDS_TopicBuiltinTopicData *data)
{
	if (!data)
		return;

	DDS_TopicBuiltinTopicData__clear (data);
	xfree (data);
}

DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_topic_data (
					DDS_DomainParticipant     dp,
					DDS_TopicBuiltinTopicData *data,
					DDS_InstanceHandle_t      handle)
{
	Entity_t		*ep;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_G_DISC_T, &dp, sizeof (dp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	if (!data || !handle)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	ep = entity_ptr (handle);
	if (!ep ||
	     ep->type != ET_TOPIC ||
	     !entity_discovered (ep->flags) ||
	     entity_ignored (ep->flags)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	if (dcps_get_builtin_topic_data (data, (Topic_t *) ep, 0)) {
		ret = DDS_RETCODE_OUT_OF_RESOURCES;
		goto done;
	}

    done:
	lock_release (dp->lock);
	return (ret);
}

DDS_Publisher DDS_DomainParticipant_create_publisher (DDS_DomainParticipant       dp,
						      const DDS_PublisherQos      *qos,
						      const DDS_PublisherListener *listener,
						      DDS_StatusMask              mask)
{
	Publisher_t	*up;
	int		enable;

	ctrc_begind (DCPS_ID, DCPS_DP_C_PUB, &dp, sizeof (dp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	prof_start (dcps_create_pub);

	if (!domain_ptr (dp, 1, NULL))
		return (NULL);

	if (qos == DDS_PUBLISHER_QOS_DEFAULT)
		qos = &dp->def_publisher_qos;
	else if (!qos_valid_publisher_qos (qos)) {
		up = NULL;
		goto done;
	}
	up = publisher_create (dp, 0);
	if (!up)
		goto done;

	qos_publisher_new (&up->qos, qos);
	if (listener)
		up->listener = *listener;
	up->mask = mask;
	up->def_writer_qos = qos_def_writer_qos;
	enable = dp->autoenable;
	lock_release (dp->lock);
	if (enable)
		DDS_Publisher_enable (up);
	return (up);

    done:
	lock_release (dp->lock);
	prof_stop (dcps_create_pub, 1);
	return (NULL);
}

DDS_ReturnCode_t DDS_DomainParticipant_delete_publisher (DDS_DomainParticipant dp,
							 DDS_Publisher         up)
{
	Condition_t		*cp;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_D_PUB, &dp, sizeof (dp));
	ctrc_contd (&up, sizeof (up));
	ctrc_endd ();

	prof_start (dcps_delete_pub);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if (!publisher_ptr (up, &ret))
		goto done;

	if (up->domain != dp) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	if (up->nwriters) {
		log_printf (DCPS_ID, 0, "delete_publisher(): still writers connected!\r\n");
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto done;
	}

	/* Delete StatusCondition if it exists. */
	if (up->condition) {
		cp = (Condition_t *) up->condition;
		if (cp->deferred)
			dds_defer_waitset_undo (up, up->condition);
		dcps_delete_status_condition (up->condition);
		up->condition = NULL;
	}
	qos_publisher_free (&up->qos);
	publisher_delete (up);

    done:
    	lock_release (dp->lock);
	prof_stop (dcps_delete_pub, 1);
	return (ret);
}

DDS_Subscriber DDS_DomainParticipant_create_subscriber (DDS_DomainParticipant        dp,
							const DDS_SubscriberQos      *qos,
							const DDS_SubscriberListener *listener,
							DDS_StatusMask               mask)
{
	Subscriber_t	*sp;
	int		enable;

	ctrc_begind (DCPS_ID, DCPS_DP_C_SUB, &dp, sizeof (dp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	prof_start (dcps_create_sub);

	if (!domain_ptr (dp, 1, NULL))
		return (NULL);

	if (qos == DDS_SUBSCRIBER_QOS_DEFAULT)
		qos = &dp->def_subscriber_qos;
	else if (!qos_valid_subscriber_qos (qos)) {
		sp = NULL;
		goto done;
	}
	sp = subscriber_create (dp, 0);
	if (!sp)
		goto done;

	qos_subscriber_new (&sp->qos, qos);
	if (listener)
		sp->listener = *listener;
	sp->mask = mask;
	enable = dp->autoenable;
	lock_release (dp->lock);
	if (enable)
		DDS_Subscriber_enable (sp);

	prof_stop (dcps_create_sub, 1);
	return (sp);

    done:
	lock_release (dp->lock);
	return (NULL);
}

DDS_ReturnCode_t DDS_DomainParticipant_delete_subscriber (DDS_DomainParticipant dp,
							  DDS_Subscriber        sp)
{
	Condition_t		*cp;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_D_SUB, &dp, sizeof (dp));
	ctrc_contd (&sp, sizeof (sp));
	ctrc_endd ();

	prof_start (dcps_delete_sub);

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if (!subscriber_ptr (sp, &ret))
		goto done;

	if (sp->domain != dp) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	if (sp->nreaders) {
		log_printf (DCPS_ID, 0, "delete_subscriber(): still readers connected!\r\n");
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto done;
	}
	if (!dds_purge_notifications ((Entity_t *) sp, DDS_ALL_STATUS, 1)) {
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto done;
	}
	sp->entity.flags &= ~EF_ENABLED;

	qos_subscriber_free (&sp->qos);

	/* Delete StatusCondition if it exists. */
	if (sp->condition) {
		cp = (Condition_t *) sp->condition;
		if (cp->deferred)
			dds_defer_waitset_undo (sp, sp->condition);
		dcps_delete_status_condition (sp->condition);
		sp->condition = NULL;
	}
	subscriber_delete (sp);

    done:
	lock_release (dp->lock);
	prof_stop (dcps_delete_sub, 1);
	return (ret);
}

DDS_Topic DDS_DomainParticipant_create_topic (DDS_DomainParticipant   dp,
					      const char              *topic_name,
					      const char              *type_name,
					      const DDS_TopicQos      *qos,
					      const DDS_TopicListener *listener,
					      DDS_StatusMask          mask)
{
	Topic_t		*tp;
	int		new_topic;

	ctrc_begind (DCPS_ID, DCPS_DP_C_TOP, &dp, sizeof (dp));
	ctrc_contd (topic_name, strlen (topic_name) + 1);
	ctrc_contd (type_name, strlen (type_name) + 1);
	ctrc_contd (&qos, sizeof (qos));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	prof_start (dcps_create_topic);

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, NULL)) {
		log_printf (DCPS_ID, 0, "create_topic(%s): domain doesn't exist!\r\n", topic_name);
		return (NULL);
	}
	if (qos == DDS_TOPIC_QOS_DEFAULT)
		qos = &dp->def_topic_qos;
	else if (!qos_valid_topic_qos (qos)) {
		log_printf (DCPS_ID, 0, "create_topic(%s): invalid topic QoS!\r\n", topic_name);
		lock_release (dp->lock);
		return (NULL);
	}

#ifdef DDS_SECURITY

	/* Check if security policy allows this topic. */
	if (check_create_topic (dp->participant.p_permissions, topic_name, qos)) {
		log_printf (DCPS_ID, 0, "create_topic(%s): topic create not allowed!\r\n", topic_name);
		lock_release (dp->lock);
		return (NULL);
	}
#endif

	/* New topic: create topic context. */
	tp = topic_create (&dp->participant, NULL, topic_name, type_name, &new_topic);
	if (!tp) {
		log_printf (DCPS_ID, 0, "create_topic (%s): out of memory for topic!\r\n", topic_name);
		lock_release (dp->lock);
		return (NULL);
	}

	/* Set Qos fields appropriately. */
	if (new_topic)
		tp->qos = qos_topic_new (qos);
	else if (tp->nlrefs == 1) /* First real QoS setting. */
		qos_topic_update (&tp->qos, qos);

	/* Check if the supplied listener can be set on the topic. Ok, if no
	   listener was previously set or the supplied listener is the same as
	   the previously set listener. */
	if (listener) {
		if (!new_topic && 
		    memcmp (&tp->listener, listener, sizeof (tp->listener)))
			log_printf (DCPS_ID, 0, "create_topic(%s): existing listener updated!\r\n", topic_name);
		tp->listener = *listener;
	}

	/* Check if the mask is the same. */
	if (!new_topic && mask && mask != tp->mask)
		log_printf (DCPS_ID, 0, "create_topic(%s): existing mask updated!\r\n", topic_name);

	tp->mask = mask;
	lock_release (dp->lock);

	if (dp->autoenable)
		DDS_Topic_enable ((DDS_Topic) tp);

	prof_stop (dcps_create_topic, 1);

	return (tp);
}

static int local_endpoints (Endpoint_t *list)
{
	Endpoint_t	*ep;

	for (ep = list; ep; ep = ep->next)
		if ((ep->entity.flags & EF_LOCAL) != 0)
			return (1);
	return (0);
}

DDS_ReturnCode_t DDS_DomainParticipant_delete_topic (DDS_DomainParticipant dp,
						     DDS_Topic             tp)
{
	DDS_ReturnCode_t	ret;
	Condition_t		*cp;

	ctrc_begind (DCPS_ID, DCPS_DP_D_TOP, &dp, sizeof (dp));
	ctrc_contd (&tp, sizeof (tp));
	ctrc_endd ();

	prof_start (dcps_delete_topic_p);

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, &ret)) {
		log_printf (DCPS_ID, 0, "dcps_delete_topic(): domain participant not found!\r\n");
		return (ret);
	}

	/* Get Topic descriptor. */
	if (!topic_ptr (tp, 0, &ret)) {
		lock_release (dp->lock);
		return (ret);
	}
	if ((tp->entity.flags & EF_FILTERED) != 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_PRECONDITION_NOT_MET);
	}
	if (tp->domain != dp || (!tp->nlrefs && !tp->nrrefs)) {
		log_printf (DCPS_ID, 0, "dcps_delete_topic(): invalid topic!\r\n");
		lock_release (dp->lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	lock_take (tp->lock);

	/* If not the last topic reference, just update the reference count. */
	if (tp->nlrefs > 1 || !tp->nlrefs) {
		if (tp->nlrefs)
			tp->nlrefs--;
		lock_release (tp->lock);
		lock_release (dp->lock);
		return (DDS_RETCODE_OK);
	}

	/* Check if there are still references, i.e. local readers/writers, of
	   this topic. */
	if (local_endpoints (tp->readers) || local_endpoints (tp->writers)) {
		log_printf (DCPS_ID, 0, "dcps_delete_topic(%s): still endpoints using topic!\r\n", str_ptr (tp->name));
		lock_release (tp->lock);
		lock_release (dp->lock);
		return (DDS_RETCODE_PRECONDITION_NOT_MET);
	}

	/* Last local reference.  Disable event propagation to topic. */
	if (!dds_purge_notifications ((Entity_t *) tp, DDS_ALL_STATUS, 1)) {
		lock_release (tp->lock);
		lock_release (dp->lock);
		return (DDS_RETCODE_PRECONDITION_NOT_MET);
	}
	tp->entity.flags &= ~EF_ENABLED;

	/* Delete topic notification data. */
	memset (&tp->listener, 0, sizeof (tp->listener));
	tp->mask = 0;

	/* Delete StatusCondition if it exists. */
	if (tp->condition) {
		cp = (Condition_t *) tp->condition;
		if (cp->deferred)
			dds_defer_waitset_undo (tp, tp->condition);
		dcps_delete_status_condition (tp->condition);
		tp->condition = NULL;
	}

	/* Delete topic data. */
	topic_delete (&dp->participant, tp, NULL, NULL);

	lock_release (dp->lock);
	prof_stop (dcps_delete_topic_p, 1);
	return (DDS_RETCODE_OK);
}

DDS_ContentFilteredTopic DDS_DomainParticipant_create_contentfilteredtopic (
					DDS_DomainParticipant dp,
					const char            *topic_name,
					DDS_Topic             related_topic,
					const char            *filter_expr,
					DDS_StringSeq         *expr_pars)
{
	Topic_t			*tp;
	BCProgram		bc_program;
	FilteredTopic_t		*ftp;
	DDS_ReturnCode_t	ret;
	int			error;
	static const char	ddssql [] = "DDSSQL";

	ctrc_begind (DCPS_ID, DCPS_DP_C_FTOP, &dp, sizeof (dp));
	ctrc_contd (topic_name, strlen (topic_name) + 1);
	ctrc_contd (related_topic, sizeof (DDS_Topic));
	ctrc_contd (filter_expr, strlen (filter_expr) + 1);
	ctrc_contd (&expr_pars, sizeof (expr_pars));
	ctrc_endd ();

	prof_start (dcps_create_ftopic);

	/* Check some required parameters. */
	if (!filter_expr)
		return (NULL);

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, NULL)) {
		log_printf (DCPS_ID, 0, "create_topic(%s): domain doesn't exist!\r\n", topic_name);
		return (NULL);
	}

	/* Get Original Topic descriptor. */
	tp = topic_ptr (related_topic, 1, &ret);
	if (!tp || tp->domain != dp) {
		lock_release (dp->lock);
		return (NULL);
	}

#ifdef DDS_SECURITY

	/* Check if security policy allows this topic. */
	if (check_create_topic (dp->participant.p_permissions, topic_name, NULL)) {
		log_printf (DCPS_ID, 0, "create_topic(%s): topic create not allowed!\r\n", topic_name);
		lock_release (dp->lock);
		return (NULL);
	}
#endif

	/* Check filter validity. */
	ret = sql_parse_filter (tp->type->type_support, filter_expr, &bc_program);
	if (ret)
		goto free_locks;

	/* Check required # of parameters. */
	if (expr_pars && DDS_SEQ_LENGTH (*expr_pars) < bc_program.npars)
		goto free_program;

	/* Create the content-filtered topic. */
	ftp = filtered_topic_create (dp, tp, topic_name);
	if (!ftp)
		goto free_program;

	/* Setup filtered topic data. */
	ftp->data.filter.expression = str_new_cstr (filter_expr);
	if (!ftp->data.filter.expression)
		goto free_filtered_topic;

	if (expr_pars) {
		ftp->data.filter.expression_pars = dcps_new_str_pars (expr_pars, &error);
		if (!ftp->data.filter.expression_pars && error)
			goto pars_failed;
	}
	else
		ftp->data.filter.expression_pars = NULL;

	ftp->data.filter.name = str_ref (ftp->topic.name);
	ftp->data.filter.related_name = str_ref (ftp->related->name);
	ftp->data.filter.class_name = str_new_cstr (ddssql);
	if (!ftp->data.filter.name || !ftp->data.filter.related_name || !ftp->data.filter.class_name)
		goto pars_failed;

	ftp->data.program = bc_program;

	bc_cache_init (&ftp->data.cache);

	lock_release (tp->lock);
	lock_release (dp->lock);

	prof_stop (dcps_create_ftopic, 1);
	return (ftp);

    pars_failed:
	lock_release (tp->lock);
	lock_release (dp->lock);
	DDS_DomainParticipant_delete_contentfilteredtopic (dp, ftp);
	return (NULL);

    free_filtered_topic:
    	filtered_topic_delete (ftp);

    free_program:
	xfree (bc_program.buffer);

    free_locks:
	lock_release (tp->lock);
	lock_release (dp->lock);
	return (NULL);
}

DDS_ReturnCode_t DDS_DomainParticipant_delete_contentfilteredtopic (
				DDS_DomainParticipant    dp,
				DDS_ContentFilteredTopic ftp)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_D_FTOP, &dp, sizeof (dp));
	ctrc_contd (&ftp, sizeof (ftp));
	ctrc_endd ();

	prof_start (dcps_delete_ftopic);

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, &ret)) {
		log_printf (DCPS_ID, 0, "delete_filtered_topic(): domain participant not found!\r\n");
		return (ret);
	}

	/* Get Topic descriptor. */
	if (!topic_ptr (ftp, 0, &ret)) {
		lock_release (dp->lock);
		return (ret);
	}
	if ((ftp->topic.entity.flags & EF_FILTERED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_PRECONDITION_NOT_MET);
	}
	if (ftp->topic.domain != dp || !ftp->topic.nlrefs) {
		log_printf (DCPS_ID, 0, "delete_filtered_topic(): invalid topic!\r\n");
		lock_release (dp->lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	lock_take (ftp->topic.lock);

	/* Remove all references, i.e. readers/writers, of this topic. */
	if (local_endpoints (ftp->topic.readers) || local_endpoints (ftp->topic.writers)) {
		log_printf (DCPS_ID, 0, "delete_filtered_topic(%s): still endpoints using topic!\r\n", str_ptr (ftp->topic.name));
		lock_release (ftp->topic.lock);
		lock_release (dp->lock);
		return (DDS_RETCODE_PRECONDITION_NOT_MET);
	}
	if (ftp->topic.nlrefs > 1) {
		ftp->topic.nlrefs--;
		lock_release (ftp->topic.lock);
		lock_release (dp->lock);
		return (DDS_RETCODE_OK);
	}

	/* Delete topic notification data. */
	memset (&ftp->topic.listener, 0, sizeof (ftp->topic.listener));
	ftp->topic.mask = 0;

	/* Delete content filter info. */
	filter_data_cleanup (&ftp->data);

	/* Delete topic data. */
	filtered_topic_delete (ftp);

	lock_release (dp->lock);
	prof_stop (dcps_delete_ftopic, 1);
	return (DDS_RETCODE_OK);
}


DDS_MultiTopic DDS_DomainParticipant_create_multitopic (
				DDS_DomainParticipant dp,
				const char *name,
				const char *type_name,
				const char *subs_expr,
				DDS_StringSeq *expr_pars)
{
	ctrc_begind (DCPS_ID, DCPS_DP_C_MTOP, &dp, sizeof (dp));
	ctrc_contd (name, strlen (name) + 1);
	ctrc_contd (type_name, strlen (type_name) + 1);
	ctrc_contd (subs_expr, strlen (subs_expr) + 1);
	ctrc_contd (&expr_pars, sizeof (expr_pars));
	ctrc_endd ();

	ARG_NOT_USED (expr_pars)

	/* Check some required parameters. */
	if (!domain_ptr (dp, 0, NULL))
		return (NULL);

	if (!subs_expr || !name || !type_name)
		return (NULL);

	return (NULL);
}

DDS_ReturnCode_t DDS_DomainParticipant_delete_multitopic (
				DDS_DomainParticipant dp,
				DDS_MultiTopic t)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DP_D_MTOP, &dp, sizeof (dp));
	ctrc_contd (&t, sizeof (t));
	ctrc_endd ();

	ARG_NOT_USED (t)

	/* Check some required parameters. */
	if (!domain_ptr (dp, 0, &ret))
		return (ret);

	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_Topic DDS_DomainParticipant_find_topic (DDS_DomainParticipant dp,
					    const char            *topic_name,
					    DDS_Duration_t        *timeout)
{
	Topic_t			*tp;
#ifdef THREADS_USED
	int			ret;
	struct timespec		ts;
	TopicWait_t		*wp, *xp, *prev_wp;
#else
	Ticks_t			d, now, end_time;	/* *10ms */
#endif

	ctrc_begind (DCPS_ID, DCPS_DP_F_TOP, &dp, sizeof (dp));
	ctrc_contd (topic_name, strlen (topic_name) + 1);
	ctrc_contd (timeout, sizeof (DDS_Duration_t));
	ctrc_endd ();

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, NULL)) {
		log_printf (DCPS_ID, 0, "find_topic(): domain participant not found!\r\n");
		return (NULL);
	}
	tp = topic_lookup (&dp->participant, topic_name);
	if (tp) {
		if (!lock_take (tp->lock)) {
			tp->entity.flags |= EF_LOCAL;
			tp->nlrefs++;
			lock_release (tp->lock);
		}
		lock_release (dp->lock);
		return ((DDS_Topic) tp);
	}

#ifdef THREADS_USED
	for (wp = dp->topic_wait; wp; wp = wp->next)
		if (!strcmp (topic_name, wp->name))
			break;

	if (wp)
		wp->nthreads++;
	else {
		wp = mds_pool_alloc (&dcps_mem_blocks [MB_TOPIC_WAIT]);
		if (!wp) {
			lock_release (dp->lock);
			return (NULL);
		}
		wp->next = dp->topic_wait;
		cond_init (wp->condition);
		wp->name = topic_name;
		wp->topic = NULL;
		wp->nthreads = 1;
		dp->topic_wait = wp;
	}
	duration2timespec (timeout, &ts);
	do {
		if (ts.tv_sec || ts.tv_nsec)
			ret = cond_wait_to (wp->condition, dp->lock, ts);
		else
			ret = cond_wait (wp->condition, dp->lock);
	}
	while (!wp->topic && !ret);
	tp = wp->topic;
	if (!--wp->nthreads) {
		for (xp = dp->topic_wait, prev_wp = NULL;
		     xp != NULL && xp != wp;
		     prev_wp = xp, xp = xp->next)
			;
		if (prev_wp)
			prev_wp->next = wp->next;
		else
			dp->topic_wait = wp->next;
		cond_destroy (wp->condition);
		mds_pool_free (&dcps_mem_blocks [MB_TOPIC_WAIT], wp);
	}
	lock_release (dp->lock);
#else
	if (dds_listener_state) {
		lock_release (dp->lock);
		return (NULL);
	}

	/* Wait until timeout elapsed for discovery to add the topic. */
	now = sys_getticks ();
	if (timeout->sec == DDS_DURATION_INFINITE_SEC ||
	    timeout->nanosec == DDS_DURATION_INFINITE_NSEC)
		end_time = now + 0x7ffffffe;
	else
		end_time = now + duration2ticks ((Duration_t *) timeout);
	for (;;) {
		d = end_time - now;
		if (d >= 0x7fffffffUL)
			break;

		DDS_schedule (d * TMR_UNIT_MS);
		tp = topic_lookup (&dp->participant, topic_name);
		if (tp) {
			tp->entity.flags |= EF_LOCAL;
			tp->nlrefs++;
			break;
		}
		now = sys_getticks ();
	}
#endif
	return (tp);
}

DDS_TopicDescription DDS_DomainParticipant_lookup_topicdescription (DDS_DomainParticipant dp,
								    const char *topic_name)
{
	Topic_t			*tp;
	DDS_TopicDescription	td;

	ctrc_begind (DCPS_ID, DCPS_DP_L_TD, &dp, sizeof (dp));
	ctrc_contd (topic_name, strlen (topic_name));
	ctrc_endd ();

	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, NULL)) {
		log_printf (DCPS_ID, 0, "find_topic(): domain participant not found!\r\n");
		return (NULL);
	}
	if (!topic_name) {
		log_printf (DCPS_ID, 0, "find_topic(): topic_name is NULL!\r\n");
		return (NULL);
	}
	tp = topic_lookup (&dp->participant, topic_name);
	td = (tp != NULL) && (tp->entity.flags & EF_LOCAL) != 0 ? (DDS_TopicDescription) tp : NULL;
	lock_release (dp->lock);
	return (td);
}

DDS_ReturnCode_t DDS_DomainParticipant_ignore_topic (DDS_DomainParticipant dp,
						     DDS_InstanceHandle_t handle)
{
	Entity_t		*ep;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DP_IGN_TOP, &dp, sizeof (dp));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	if (!domain_ptr (dp, 1, &ret))
		return (ret);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	ep = entity_ptr (handle);
	if (!ep ||
	    ep->type != ET_TOPIC ||
	    !entity_discovered (ep->flags)) {
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto done;
	}
	ret = disc_ignore_topic ((Topic_t *) ep);

    done:
	lock_release (dp->lock);
	return (ret);
}


