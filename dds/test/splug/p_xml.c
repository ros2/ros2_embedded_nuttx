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

#include "msecplug/msecplug.h"
#include "error.h"
#include "security.h"
#include "list.h"
#include "log.h"
#include "libx.h"
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

static MSAccess_t parse_access (xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar	*s;
	MSAccess_t acc;

	s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
	if (!xmlStrcmp (s, (const xmlChar *) "unclassified"))
		acc = DS_UNCLASSIFIED;
	else if (!xmlStrcmp (s, (const xmlChar *) "confidential"))
		acc = DS_CONFIDENTIAL;
	else if (!xmlStrcmp (s, (const xmlChar *) "secret"))
		acc = DS_SECRET;
	else if (!xmlStrcmp (s, (const xmlChar *) "top secret"))
		acc = DS_TOP_SECRET;
	else
		fatal_printf ("security.xml: access type expected!");

	xmlFree (s);
	return (acc);
}

static int parse_membership (xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar	*s;
	int exclusive;

	s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
	if (!xmlStrcmp (s, (const xmlChar *) "open"))
		exclusive = 0;
	else if (!xmlStrcmp (s, (const xmlChar *) "exclusive"))
		exclusive = 1;
	else
		fatal_printf ("security.xml: access type expected!");

	xmlFree (s);
	return (exclusive);
}

static uint32_t parse_transport (xmlDocPtr doc, xmlNodePtr cur, xmlChar *protocol)
{
	uint32_t	mode;
	xmlChar		*type;
	unsigned	s, v;

	mode = 0;
	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "local"))
			s = 16;
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "remote"))
			s = 0;
		else { /* Ignore other modes. */
			cur = cur->next;
			continue;
		}
		type = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
		if (!xmlStrcmp (type, (const xmlChar *) "open"))
			v = SECC_NONE;
		else if (!xmlStrcmp (type, (const xmlChar *) "DTLS")) {
			if (protocol && astrcmp ((char *) protocol, "UDP"))
				warn_printf ("security.xml: invalid secure transport protocol (%s:DTLS)!", (char *) protocol);
			else
				v = SECC_DTLS_UDP;
		}
		else if (!xmlStrcmp (type, (const xmlChar *) "TLS")) {
			if (protocol && astrcmp ((char *) protocol, "TCP"))
				warn_printf ("security.xml: invalid secure transport protocol (%s:TLS)!", (char *) protocol);
			else
		 		v = SECC_TLS_TCP;
		}
		else if (!xmlStrcmp (type, (const xmlChar *) "DDSSEC"))
			v = SECC_DDS_SEC;
		else {
			warn_printf ("security.xml: unknown transport '%s'!", (const char *) type);
			cur = cur->next;
			xmlFree (type);
			continue;
		}
		xmlFree (type);
		mode |= (v << s);
		cur = cur->next;
	}
	return (mode);
}

static int parse_blacklist (xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar		*s;
	int             blacklist;
	
	s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
	if (!xmlStrcmp (s, (const xmlChar *) "false"))
		blacklist = 0;
	else
		blacklist = 1;

	xmlFree (s);
	return (blacklist);\
}

static void parse_domain_topic (MSDomain_t *p, xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar	*s;
	MSTopic_t *tp;
	int have_name, have_mode;
	
	if (p->ntopics >= MAX_DOMAIN_TOPICS)
		fatal_printf ("security.xml: too many domain topic specifications!");

	tp = calloc (1, sizeof (MSTopic_t));

	have_name = 0;
	have_mode = 0;
	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "name")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "*"))
				tp->name = NULL;
			else {
				tp->name = malloc (strlen ((char *) s) + 1);
				strcpy (tp->name, (char *) s);
			}
			xmlFree (s);
			have_name = 1;
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "mode")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "open"))
				tp->mode = TA_ALL;
			else if (!xmlStrcmp (s, (const xmlChar *) "restrict"))
				tp->mode = TA_NONE;
			else {
				warn_printf ("security.xml: unknown domain topic mode '%s'!", (const char *) s);
				cur = cur->next;
				continue;
			}	
			xmlFree (s);
			have_mode = 1;
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "blacklist")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "false"))
				tp->blacklist = 0;
			else
				tp->blacklist = 1;
		}
		cur = cur->next;
	}

	LIST_ADD_TAIL (p->topics, *tp);

	if (have_name && have_mode)
		p->ntopics++;
}

static void parse_domain_partition (MSDomain_t *p, xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar	*s;
	MSPartition_t *pp;
	int have_name, have_mode;
	
	if (p->npartitions >= MAX_DOMAIN_PARTITIONS)
		fatal_printf ("security.xml: too many domain partition specifications!");

	pp = calloc (1, sizeof (MSPartition_t));

	have_name = 0;
	have_mode = 0;
	pp->blacklist = 0;
	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "name")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "*"))
				pp->name = NULL;
			else {
				pp->name = malloc (strlen ((char *) s) + 1);
				strcpy (pp->name, (char *) s);
			}
			xmlFree (s);
			have_name = 1;
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "mode")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "open"))
				pp->mode = TA_ALL;
			else if (!xmlStrcmp (s, (const xmlChar *) "restrict"))
				pp->mode = TA_NONE;
			else if (!xmlStrcmp (s, (const xmlChar *) "read"))
				pp->mode = TA_READ;
			else if (!xmlStrcmp (s, (const xmlChar *) "write"))
				pp->mode = TA_WRITE;
			else {
				warn_printf ("security.xml: unknown domain partition mode '%s'!", (const char *) s);
				cur = cur->next;
				continue;
			}
			xmlFree (s);
			have_mode = 1;
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "blacklist")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "false"))
				pp->blacklist = 0;
			else
				pp->blacklist = 1;
			xmlFree (s);
		}
		cur = cur->next;
	}

	LIST_ADD_TAIL (p->partitions, *pp);

 	if (have_name && have_mode)
		p->npartitions++;
}

static void parse_domain (xmlDocPtr doc, xmlNodePtr cur, xmlChar *id)
{
	MSDomain_t	*p;
	MSTopic_t	*tp;
	MSPartition_t	*pp;
	xmlChar		*protocol;
	unsigned	domain_id, i;
	int		domain_handle, other_handle;

	if (id [0] == '*' && id [1] == '\0')
		domain_id = ~0;
	else {
		domain_id = atoi ((char *) id);
		if (domain_id > 255)
			fatal_printf ("security.xml: domain id too large!");
	}
	/*if (lookup_domain (domain_id, 0))
		fatal_printf ("security.xml: duplicate domain!");
	*/
	p = malloc (sizeof (MSDomain_t));
	if (!p)
		fatal_printf ("security.xml: out-of-memory for domain!");

	memset (p, 0, sizeof (MSDomain_t));
	p->domain_id = domain_id;
	LIST_INIT (p->topics);
	LIST_INIT (p->partitions);
	p->transport = 0;

	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "access"))
			p->access = parse_access (doc, cur);
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "membership"))
			p->exclusive = parse_membership (doc, cur);
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "transport")) {
			protocol = xmlGetProp (cur, (const xmlChar *) "protocol");
			p->transport |= parse_transport (doc, cur->xmlChildrenNode, protocol);
			xmlFree (protocol);
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "topic"))
			parse_domain_topic (p, doc, cur->xmlChildrenNode);
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "partition"))
			parse_domain_partition (p, doc, cur->xmlChildrenNode);
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "blacklist"))
			p->blacklist = parse_blacklist (doc, cur->xmlChildrenNode);

		cur = cur->next;
	}
	if ((domain_handle = DDS_SP_get_domain_handle (domain_id)) == -1)
		domain_handle = DDS_SP_add_domain ();
	DDS_SP_set_domain_access (domain_handle, domain_id, p->access, p->exclusive, p->transport, p->blacklist);

	LIST_FOREACH (p->topics, tp) {
		if (( other_handle = DDS_SP_get_topic_handle (0, domain_handle, tp->name, tp->mode)) == -1)
			other_handle = DDS_SP_add_topic (0, domain_handle);
		DDS_SP_set_topic_access (0, 
					 domain_handle, 
					 other_handle, 
					 tp->name, 
					 tp->mode, 
					 0,
					 tp->blacklist);
	}

	for (i = 0; i < p->ntopics; i++) {
		tp = LIST_HEAD (p->topics);
		free (tp->name);
		LIST_REMOVE (p->topics, *tp);
		free (tp);
	}

	LIST_FOREACH (p->partitions, pp) {
		if ((other_handle = DDS_SP_get_partition_handle (0, domain_handle, pp->name, pp->mode)) == -1)
			other_handle = DDS_SP_add_partition (0, domain_handle);
		DDS_SP_set_partition_access (0, 
					      domain_handle, 
					      other_handle, 
					      pp->name, 
					      pp->mode, 
					      0,
					      pp->blacklist);
	}

	for (i = 0; i < p->npartitions; i++) {
		pp = LIST_HEAD (p->partitions);
		free (pp->name);
		LIST_REMOVE (p->partitions, *pp);
		free (pp);
	}
	free (p);
}

static void parse_participant_topic (MSParticipant_t *p,
				     DDS_DomainId_t  id,
				     xmlDocPtr       doc,
				     xmlNodePtr      cur)
{
	xmlChar		*s;
	uint32_t	acc;
	int		have_name, have_create, have_read, have_blacklist;
	MSUTopic_t	*tp;

	if (p->ntopics >= MAX_USER_TOPICS)
		fatal_printf ("security.xml: too many user topic specifications!");

	have_name = 0;
	have_create = 0;
	have_read = 0;
	have_blacklist = 0;
	acc = 0;
	tp = calloc (1, sizeof (MSUTopic_t));
	tp->id = id;
	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "name")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "*"))
				tp->topic.name = NULL;
			else {
				tp->topic.name = malloc (strlen ((char *) s) + 1);
				strcpy (tp->topic.name, (char *) s);
			}
			xmlFree (s);
			have_name = 1;
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "create")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "no"))
				acc &= ~(TA_CREATE | TA_DELETE);
			else if (!xmlStrcmp (s, (const xmlChar *) "yes"))
				acc |= TA_CREATE | TA_DELETE;
			else
				warn_printf ("security.xml: unknown participant topic create '%s'!", (const char *) s);
			xmlFree (s);
			have_create = 1;
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "read")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "no"))
				acc &= ~(TA_READ | TA_WRITE);
			else if (!xmlStrcmp (s, (const xmlChar *) "read_only")) {
				acc |= TA_READ;
				acc &= ~TA_WRITE;
			}
			else if (!xmlStrcmp (s, (const xmlChar *) "yes") ||
			         !xmlStrcmp (s, (const xmlChar *) "read_write"))
				acc |= TA_READ | TA_WRITE;
			else
				warn_printf ("security.xml: unknown participant topic read '%s'!", (const char *) s);
			xmlFree (s);
			have_read = 1;
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "blacklist")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "false"))
				tp->topic.blacklist = 0;
			else
				tp->topic.blacklist = 1;
			xmlFree (s);
			have_blacklist = 1;
		}
		cur = cur->next;
	}

	LIST_ADD_TAIL (p->topics, *tp);

	if (have_name && (have_create || have_read || have_blacklist)) {
		tp->topic.mode = acc;
		p->ntopics++;
	}
}

static void parse_participant_partition (MSParticipant_t *p,
					 DDS_DomainId_t  id,
					 xmlDocPtr       doc,
					 xmlNodePtr      cur)
{
	xmlChar		*s;
	uint32_t	acc;
	int		have_name, have_create, have_read;
	MSUPartition_t	*pp;

	if (p->npartitions >= MAX_USER_PARTITIONS)
		fatal_printf ("security.xml: too many user partition specifications!");

	have_name = 0;
	have_create = 0;
	have_read = 0;
	acc = 0;
	pp = calloc (1, sizeof (MSUPartition_t));
	pp->id = id;
	pp->partition.blacklist = 0;
	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "name")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "*"))
				pp->partition.name = NULL;
			else {
				pp->partition.name = malloc (strlen ((char *) s) + 1);
				strcpy (pp->partition.name, (char *) s);
			}
			xmlFree (s);
			have_name = 1;
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "create")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "no"))
				acc &= ~(TA_CREATE | TA_DELETE);
			else if (!xmlStrcmp (s, (const xmlChar *) "yes"))
				acc |= TA_CREATE | TA_DELETE;
			else
				warn_printf ("security.xml: unknown participant partition create '%s'!", (const char *) s);
			xmlFree (s);
			have_create = 1;
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "read")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "no"))
				acc &= ~(TA_READ | TA_WRITE);
			else if (!xmlStrcmp (s, (const xmlChar *) "read_only")) {
				acc |= TA_READ;
				acc &= ~TA_WRITE;
			}
			else if (!xmlStrcmp (s, (const xmlChar *) "yes") ||
			         !xmlStrcmp (s, (const xmlChar *) "read_write"))
				acc |= TA_READ | TA_WRITE;
			else
				warn_printf ("security.xml: unknown participant partition read '%s'!", (const char *) s);
			xmlFree (s);
			have_read = 1;
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "blacklist")) {
			s = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcmp (s, (const xmlChar *) "false"))
				pp->partition.blacklist = 0;
			else
				pp->partition.blacklist = 1;
			xmlFree (s);
		}
		cur = cur->next;
	}

	LIST_ADD_TAIL (p->partitions, *pp);

	if (have_name && have_create && have_read) {
		pp->partition.mode = acc;
		p->npartitions++;
	}
}

static void parse_participant_domain (MSParticipant_t *p,
				      xmlDocPtr       doc,
				      xmlNodePtr      cur,
				      xmlChar         *id)
{
	DDS_DomainId_t	domain_id;

	if (id [0] == '*' && id [1] == '\0')
		domain_id = ~0;
	else {
		domain_id = atoi ((char *) id);
		if (domain_id > 255)
			fatal_printf ("security.xml: domain id too large!");
	}
	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "topic"))
			parse_participant_topic (p, domain_id, doc, cur->xmlChildrenNode);
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "partition"))
			parse_participant_partition (p, domain_id, doc, cur->xmlChildrenNode);
		cur = cur->next;
	}
}

static void parse_participant (xmlDocPtr doc, xmlNodePtr cur, const char *name)
{
	MSParticipant_t	*p;
	MSUTopic_t	*tp;
	MSUPartition_t	*pp;
	xmlChar		*id;
	unsigned	i;
	int		participant_handle, other_handle;

	/*	if (lookup_participant (name))
		fatal_printf ("security.xml: duplicate user ('%s')!", name);
	*/
	p = malloc (sizeof (MSParticipant_t));
	memset (p, 0, sizeof (MSParticipant_t));
	memcpy (p->name, name, strlen (name));

	LIST_INIT (p->topics);
	LIST_INIT (p->partitions);
	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "access"))
			p->access = parse_access (doc, cur);
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "domain")) {
			id = xmlGetProp (cur, (const xmlChar *) "id");
			parse_participant_domain (p, doc, cur->xmlChildrenNode, id);
			xmlFree (id);
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "topic"))
			parse_participant_topic (p, ~0, doc, cur->xmlChildrenNode);
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "partition"))
			parse_participant_partition (p, ~0, doc, cur->xmlChildrenNode);
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "blacklist"))
			p->blacklist = parse_blacklist (doc, cur->xmlChildrenNode);

		cur = cur->next;
	}

	if ((participant_handle = DDS_SP_get_participant_handle (p->name)) == -1)
		participant_handle = DDS_SP_add_participant ();
	DDS_SP_set_participant_access (participant_handle, p->name, p->access, p->blacklist);

	LIST_FOREACH (p->topics, tp) {
		if ((other_handle = DDS_SP_get_topic_handle (participant_handle, 0, tp->topic.name, tp->topic.mode)) == -1)
			other_handle = DDS_SP_add_topic (participant_handle, 0);
		DDS_SP_set_topic_access (participant_handle, 
					  0,
					  other_handle,
					  tp->topic.name,
					  tp->topic.mode,
					  tp->id,
					  tp->topic.blacklist);
	}
	for (i = 0; i < p->ntopics; i++) {
		tp = LIST_HEAD (p->topics);
		free (tp->topic.name);
		LIST_REMOVE (p->topics, *tp);
		free (tp);
	}

	LIST_FOREACH (p->partitions, pp) {
		if ((other_handle = DDS_SP_get_partition_handle (participant_handle, 0, pp->partition.name, pp->partition.mode)) == -1)
			other_handle = DDS_SP_add_partition (participant_handle, 0);
		DDS_SP_set_partition_access (participant_handle, 
					      0, 
					      other_handle,
					      pp->partition.name,
					      pp->partition.mode,
					      pp->id,
					      pp->partition.blacklist);
	}

	for (i = 0; i < p->npartitions; i++) {
		pp = LIST_HEAD (p->partitions);
		free (pp->partition.name);
		LIST_REMOVE (p->topics, *pp);
		free (pp);
	}
	free (p);
}

static void parse_security_rules (xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar	*id, *name;

	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "domain")) {
			id = xmlGetProp (cur, (const xmlChar *) "id");
			parse_domain (doc, cur->xmlChildrenNode, id);
			xmlFree (id);
		}
		else if (!xmlStrcmp (cur->name, (const xmlChar *) "participant")) {
			name = xmlGetProp (cur, (const xmlChar *) "name");
			parse_participant (doc, cur->xmlChildrenNode, (char *) name);
			xmlFree (name);
		}
		cur = cur->next;
	}
}

int DDS_SP_parse_xml (const char *fn)
{
	xmlDocPtr	doc;
	xmlNodePtr	cur;
	int		got_rules;

	log_printf (SEC_ID, 0, "SP: parsing security rules from '%s'.\r\n", fn);
	doc = xmlParseFile (fn);
	if (!doc)
		fatal_printf ("SP: error parsing '%s'!", fn);

	cur = xmlDocGetRootElement (doc);
	if (!cur)
		fatal_printf ("SP: empty file '%s'!", fn);

	if (xmlStrcmp (cur->name, (const xmlChar *) "dds"))
		fatal_printf ("SP: incorrect file '%s'!", fn);

	got_rules = 0;
	cur = cur->xmlChildrenNode;
	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "security")) {
			parse_security_rules (doc, cur->xmlChildrenNode);
			got_rules = 1;
		}
		cur = cur->next;
	}
	xmlFreeDoc (doc);

	return (got_rules);
}

