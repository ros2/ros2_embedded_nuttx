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

#include <stdio.h>
#include <stdlib.h>
#include "sp_access_db.h"

 main (int argc, const char *argv [])
{
	unsigned handle = 0, handle2 = 0;
	char *name;
	MSDomain_t *d;
	MSParticipant_t *p;
	MSTopic_t       *dt;
	MSUTopic_t      *pt;
	MSPartition_t   *dp;
	MSUPartition_t  *pp;

	unsigned pone, ptwo, tone, ttwo, pread [5], pwrite [5], poneperm, ptwoperm, matched, done;

	/***** Test sp_access_db *****/

	/****** Domain test *****/

	sp_access_init ();
	
	sp_access_update_start ();

	if (!(d = sp_access_add_domain (&handle)))
		exit (1);

	d->domain_id = 5;

	if (!sp_access_get_domain (handle))
		exit (1);

	if (sp_access_get_domain_handle (5) != handle)
		exit (1);

	if (sp_access_remove_domain (handle))
		exit (1);
	
	if (sp_access_get_domain (handle))
		exit (1);

	sp_access_update_done ();

	/***** Participant test *****/

	sp_access_init ();
	
	sp_access_update_start ();

	if (!(p = sp_access_add_participant (&handle)))
		exit (1);

	strcpy (p->name, "name");

	if (!sp_access_get_participant (handle))
		exit (1);

	if (sp_access_get_participant_handle ("name") != handle)
		exit (1);

	if (sp_access_remove_participant (handle))
		exit (1);
	
	if (sp_access_get_participant (handle))
		exit (1);

	if (sp_access_add_unchecked_participant ("tmp", 3, &handle2))
		exit (1);

	if (!sp_access_get_participant (handle2))
		exit (1);
	
	sp_access_update_done ();
	
	/***** Topic Test *****/

	sp_access_init ();
	
	sp_access_update_start ();

	if (!(p = sp_access_add_participant (&handle)))
		exit (1);

	strcpy (p->name, "name");

	if (!(pt = sp_access_add_topic (&handle2, handle, LIST_PARTICIPANT)))
		exit (1);
	
	pt->topic.name = malloc (10);
	strcpy (pt->topic.name, "topic");

	if (!sp_access_get_topic (handle2, handle, LIST_PARTICIPANT))
		exit (1);
	
	if (!(sp_access_get_topic_handle ("topic", TA_ALL, handle, LIST_PARTICIPANT)))
		exit (1);

	if (sp_access_remove_topic (handle2, handle, LIST_PARTICIPANT))
		exit (1);

	if (sp_access_get_topic (handle2, handle, LIST_PARTICIPANT))
		exit (1);

	/***** Partition test *****/

	if (!(pp = sp_access_add_partition (&handle2, handle, LIST_PARTICIPANT)))
		exit (1);
	
	pp->partition.name = malloc (10);
	strcpy (pp->partition.name, "partition");

	if (!sp_access_get_partition (handle2, handle, LIST_PARTICIPANT))
		exit (1);
	
	if (!(sp_access_get_partition_handle (name, TA_ALL, handle, LIST_PARTICIPANT)))
		exit (1);

	if (sp_access_remove_partition (handle2, handle, LIST_PARTICIPANT))
		exit (1);

	if (sp_access_get_partition (handle2, handle, LIST_PARTICIPANT))
		exit (1);

	/***** Test sp_access_populate *****/

	DDS_SP_parse_xml ("security.xml");

	DDS_SP_update_done ();

	DDS_SP_access_db_cleanup ();

	DDS_SP_update_start ();

	DDS_SP_parse_xml ("security.xml");

	DDS_SP_update_done ();

	DDS_SP_access_db_dump ();

	DDS_SP_update_start ();

	DDS_SP_parse_xml ("security2.xml");
	
	DDS_SP_update_done ();

	DDS_SP_access_db_dump ();

	DDS_SP_access_db_cleanup ();

	DDS_SP_access_db_dump ();

	/***** Test fine grained access control *****/

	DDS_SP_update_start ();

	/* Add a domain */
	done = DDS_SP_add_domain ();
	DDS_SP_set_domain_access (done, 1, DS_UNCLASSIFIED, 0, 0, 0, 0, 0);

	/* add 2 participants */
	pone = DDS_SP_add_participant ();
	ptwo = DDS_SP_add_participant ();

	/* set participant access rules */
	DDS_SP_set_participant_access (pone, "Participant one", DS_UNCLASSIFIED, 0);
	DDS_SP_set_participant_access (ptwo, "Participant two", DS_UNCLASSIFIED, 0);

	/* add two topics */
	tone = DDS_SP_add_topic (pone, 0);
	ttwo = DDS_SP_add_topic (ptwo, 0);

	/* set topic access */
	DDS_SP_set_topic_access (pone, 0, tone, "tmp", TA_ALL, 0, 0, 0, 0, 1, 0);
	DDS_SP_set_topic_access (ptwo, 0, ttwo, "tmp", TA_ALL, 0, 0, 0, 0, 1, 0);

	/* set fine grained topic access */
	pread [0] = pone;
	pread [1] = ptwo;
	pwrite [0] = pone;
	pwrite [1] = ptwo;
	/* Give them both read and write access to each other */
	DDS_SP_set_fine_grained_topic (pone, 0, tone, pread, 2, pwrite, 2);
	DDS_SP_set_fine_grained_topic (ptwo, 0, ttwo, pread, 2, pwrite, 2);

	/* test if it is allowed */
	poneperm =  (done << 16) | pone;
	ptwoperm = (done << 16) | ptwo;
	if (sp_access_check_local_datawriter_match (poneperm, ptwoperm, "tmp", "tmp", &matched) ||
	    !matched) {
		printf ("Matching failed\r\n");
		exit (1);
	}
	if (sp_access_check_local_datareader_match (poneperm, ptwoperm, "tmp", "tmp", &matched) ||
	    !matched) {
		printf ("Matching failed\r\n");
		exit (1);
	}

	/* participant one cannot read from 2 */
	DDS_SP_set_fine_grained_app_topic (pone, 0, tone, pread, 1, pwrite, 2);

	if (sp_access_check_local_datawriter_match (poneperm, ptwoperm, "tmp", "tmp", &matched) ||
	    !matched) {
		printf ("Matching failed\r\n");
		exit (1);
	}
	if (sp_access_check_local_datareader_match (poneperm, ptwoperm, "tmp", "tmp", &matched) || 
	    matched) {
		printf ("Matching succeeded, but it should not!\r\n");
		exit (1);
	}


	/* participant two cannot write to 1 */
	pwrite [0] = ptwo;
	DDS_SP_set_fine_grained_app_topic (ptwo, 0, ttwo, pread, 2, pwrite, 1);

	if (sp_access_check_local_datareader_match (ptwoperm, poneperm, "tmp", "tmp", &matched) ||
	    !matched) {
		printf ("Matching failed\r\n");
		exit (1);
	}
	if (sp_access_check_local_datawriter_match (ptwoperm, poneperm, "tmp", "tmp", &matched) || 
	    matched) {
		printf ("Matching succeeded, but it should not!\r\n");
		exit (1);
	}

	/* update app permissions again so they can communicate again */
	pwrite [0] = pone;
	pwrite [1] = ptwo;
	DDS_SP_set_fine_grained_app_topic (ptwo, 0, ttwo, pread, 2, pwrite, 2);
	DDS_SP_set_fine_grained_app_topic (pone, 0, tone, pread, 2, pwrite, 2);

	if (sp_access_check_local_datareader_match (ptwoperm, poneperm, "tmp", "tmp", &matched) ||
	    !matched) {
		printf ("Matching failed\r\n");
		exit (1);
	}
	if (sp_access_check_local_datawriter_match (ptwoperm, poneperm, "tmp", "tmp", &matched) || 
	    !matched) {
		printf ("Matching failed\r\n");
		exit (1);
	}

	/* participant two cannot write to 1 */
	pwrite [0] = ptwo;
	DDS_SP_set_fine_grained_app_topic (ptwo, 0, ttwo, pread, 2, pwrite, 1);

	if (sp_access_check_local_datareader_match (ptwoperm, poneperm, "tmp", "tmp", &matched) ||
	    !matched) {
		printf ("Matching failed\r\n");
		exit (1);
	}
	if (sp_access_check_local_datawriter_match (ptwoperm, poneperm, "tmp", "tmp", &matched) || 
	    matched) {
		printf ("Matching succeeded, but it should not!\r\n");
		exit (1);
	}

	DDS_SP_access_db_dump ();

	/* Remove fine grained app topic */

	DDS_SP_remove_fine_grained_app_topic (pone, 0, tone);
	DDS_SP_remove_fine_grained_app_topic (ptwo, 0, ttwo);

	if (sp_access_check_local_datareader_match (ptwoperm, poneperm, "tmp", "tmp", &matched) ||
	    !matched) {
		printf ("Matching failed\r\n");
		exit (1);
	}
	if (sp_access_check_local_datawriter_match (ptwoperm, poneperm, "tmp", "tmp", &matched) || 
	    !matched) {
		printf ("Matching failed\r\n");
		exit (1);
	}

	DDS_SP_access_db_dump ();

	DDS_SP_update_done ();

	printf ("test succeeded\r\n");
	return (0);
}
