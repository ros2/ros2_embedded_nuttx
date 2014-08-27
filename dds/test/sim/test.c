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
#include "error.h"
#include "disc.h"
#include "sim.h"

int matched;
GuidPrefix_t prefix = {{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }};
Locator_t def_ucast_locs = { LOCATOR_KIND_UDPv4, 7411,
			     { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  10, 0, 0, 29 }};
Locator_t def_mcast_locs = { LOCATOR_KIND_UDPv4, 7401,
			     { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 239,255,0,  1 }};
Locator_t meta_ucast_locs = { LOCATOR_KIND_UDPv4, 7410,
			     { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  10, 0, 0, 29 }};
Locator_t meta_mcast_locs = { LOCATOR_KIND_UDPv4, 7400,
			     { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 239,255,0,  1 }};
EntityId_t r_eid = {{ 0, 0, 12, ENTITY_KIND_READER_KEY }}; 
EntityId_t w_eid = {{ 0, 0, 13, ENTITY_KIND_WRITER_KEY }}; 

extern int aborting;

/* sim_new_participant -- Simulate that a new Participant is discovered. */

Participant_t *sim_new_participant (void)
{
	Participant_t			*pp;
	SPDPdiscoveredParticipantData	p_data;

	p_data.proxy.guid_prefix = prefix;
	version_init (p_data.proxy.proto_version);
	vendor_id_init (p_data.proxy.vendor_id);
	p_data.proxy.expects_il_qos = 0;
	p_data.proxy.builtins = 0;
	p_data.proxy.def_ucast = locator_list_create (1, &def_ucast_locs);
	p_data.proxy.def_mcast = locator_list_create (1, &def_mcast_locs);
	p_data.proxy.meta_ucast = locator_list_create (1, &meta_ucast_locs);
	p_data.proxy.meta_mcast = locator_list_create (1, &meta_mcast_locs);
	p_data.user_data = NULL;
	p_data.lease_duration.secs = 30;
	p_data.lease_duration.nanos = 0;
	pp = disc_remote_participant_add (domain_lookup (0), &p_data);
	if (!pp)
		fatal_printf ("Remote participant create failed!");

	return (pp);
}

/* sim_new_reader -- Simulate that a new Reader is discovered. */

DiscoveredReader_t *sim_new_reader (Participant_t *pp)
{
	DiscoveredReader_t	*rp;
	DiscoveredReaderData	r_data;

	r_data.proxy.guid.prefix = prefix;
	r_data.proxy.guid.entity_id = r_eid;
	r_data.proxy.expects_inline_qos = 0;
	r_data.proxy.ucast = NULL;
	r_data.proxy.mcast = NULL;
	r_data.topic_name = str_new_cstr ("HelloWorld");
	r_data.type_name = str_new_cstr ("HelloWorldData");
	r_data.qos = qos_def_disc_reader_qos;
	r_data.qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	r_data.qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	r_data.filter = NULL;
	rp = disc_remote_reader_add (pp, &r_data);
	if (!rp)
		fatal_printf ("Remote reader create failed!");

	return (rp);
}

/* simulate_reader -- Called regularly in writer mode to perform test 
		      sequences. */

void simulate_reader (unsigned n, unsigned sleep_time)
{
	static Participant_t		*pp;
	static DiscoveredReader_t	*rp;
	SequenceNumber_t		base;
	uint32_t			bits [8];

	if (n < 34) {
		DDS_wait (sleep_time);
		return;
	}
	if (!matched) {
		printf ("Sim: 34 samples reached - setup remote reader!\r\n");

		pp = sim_new_participant ();
		printf ("Sim: Remote participant registered!\r\n");

		rp = sim_new_reader (pp);
		printf ("SimR: Remote reader registered!\r\n");

		matched = 1;
	}
	if (n == 34) {

		/* 1. Wait for WALIVE HEARTBEAT message. */
		if (!sim_wait_heartbeat (NULL, NULL, NULL))
			fatal_printf ("Heartbeat expected(1)!");

		printf ("SimR: ALIVE HEARTBEAT successfully received!\r\n");

		/* 2. Reply with an ACKNACK requesting everything. */
		sim_create_bitmap (14, 30, "111111111111111111111111111111",
				   &base, bits);
		sim_new_msg (pp, &rp->dr_ep, rp->dr_topic->writers, 0);
		sim_add_acknack (0, &base, 30, bits, 20);
		sim_rx_msg ();

		/* 3. All data should have been sent. Wait for HEARTBEAT. */
		if (!sim_wait_heartbeat (NULL, NULL, NULL))
			fatal_printf ("Heartbeat expected(2)!");

		printf ("SimR: DATA + GAP + HEARTBEAT successfully received!\r\n");

		/* 4. Reply with ACKNACK requesting the gaps. */
		sim_create_bitmap (15, 29, "10101011111111111110101010",
				   &base, bits);

		sim_new_msg (pp, &rp->dr_ep, rp->dr_topic->writers, 0);
		sim_add_acknack (0, &base, 29, bits, 21);
		sim_rx_msg ();

		/* 5. Should resend the GAP now. Wait for HEARTBEAT. */
		if (!sim_wait_heartbeat (NULL, NULL, NULL))
			fatal_printf ("Heartbeat expected(3)!");

		printf ("SimR: test successful!\r\n");
		aborting = 1;
	}
	DDS_wait (sleep_time);
}


/* sim_new_reader -- Simulate that a new Reader is discovered. */

DiscoveredWriter_t *sim_new_writer (Participant_t *pp)
{
	DiscoveredWriter_t	*wp;
	DiscoveredWriterData	w_data;

	w_data.proxy.guid.prefix = prefix;
	w_data.proxy.guid.entity_id = w_eid;
	w_data.proxy.ucast = NULL;
	w_data.proxy.mcast = NULL;
	w_data.topic_name = str_new_cstr ("Square");
	w_data.type_name = str_new_cstr ("ShapeType");
	w_data.qos = qos_def_disc_writer_qos;
	w_data.qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	w_data.qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	wp = disc_remote_writer_add (pp, &w_data);
	if (!wp)
		fatal_printf ("Remote writer create failed!");

	return (wp);
}

/* simulate_writer -- Called regularly in reader mode to perform test
		      sequences. */

void simulate_writer (unsigned n, unsigned sleep_time)
{
	static Participant_t   		*pp;
	static DiscoveredWriter_t	*wp;
	SequenceNumber_t		snr;
	Time_t				time;
	static KeyHash_t		hash = {
	    {
		0xca, 0xc2, 0x17, 0xc3,
		0x18, 0x36, 0x3f, 0x8e,
		0xf1, 0x16, 0x0e, 0xee,
		0xde, 0xf9, 0xe8, 0x86
	    }
	};
	static unsigned char		data [] = {
		0, 1, 0, 0,	/* CDR-LE - no options. */
		5, 0, 0, 0,	/* String: length = 5. */
			'B', 'L', 'U', 'E', 0, 0, 0, 0,
		0x6c, 0, 0, 0,
		0x14, 0, 0, 0,
		0x1e, 0, 0, 0
	};

	ARG_NOT_USED (n)

	DDS_wait (sleep_time);

	pp = sim_new_participant ();
	printf ("Sim: Remote participant registered!\r\n");

	wp = sim_new_writer (pp);
	printf ("Sim: Remote writer registered!\r\n");

	snr.low = 0;
	snr.high = 0;
	sim_wait_acknack (&snr, 0, NULL);

	printf ("Sim: ACKNACK(0,0) - received!\r\n");

	sim_new_msg (pp, &wp->dw_ep, wp->dw_topic->readers, 0);
	time.seconds = 1302855649;
	time.nanos   = 648992;
	sim_add_info_ts (&time, 0);
	snr.low = 28443;
	snr.high = 0;
	sim_add_data (&snr, &hash, NULL, NULL, data, sizeof (data));
	sim_rx_msg ();

	printf ("Sim: DATA (28443) D-Q sent!\r\n");

	sim_new_msg (pp, &wp->dw_ep, wp->dw_topic->readers, 0);
	snr.low = 28442;
	snr.high = 0;
	sim_add_heartbeat (SMF_FINAL, &snr, &snr, 2345);
	sim_rx_msg ();

	printf ("Sim: HEARTBEAT (28442, 28442) sent!\r\n");

	sim_new_msg (pp, &wp->dw_ep, wp->dw_topic->readers, 0);
	snr.low = 28444;
	snr.high = 0;
	data [16]++;
	data [20]++;
	sim_add_data (&snr, &hash, NULL, NULL, data, sizeof (data));
	sim_rx_msg ();

	printf ("Sim: DATA (28444) D-Q sent!\r\n");

	sim_new_msg (pp, &wp->dw_ep, wp->dw_topic->readers, 0);
	snr.low = 28445;
	snr.high = 0;
	data [16]++;
	data [20]++;
	sim_add_data (&snr, &hash, NULL, NULL, data, sizeof (data));
	sim_rx_msg ();

	printf ("Sim: DATA (28445) D-Q sent!\r\n");

	sim_new_msg (pp, &wp->dw_ep, wp->dw_topic->readers, 0);
	snr.low = 28446;
	snr.high = 0;
	data [16]++;
	data [20]++;
	sim_add_data (&snr, &hash, NULL, NULL, data, sizeof (data));
	sim_rx_msg ();

	printf ("Sim: DATA (28446) D-Q sent!\r\n");

	sim_new_msg (pp, &wp->dw_ep, wp->dw_topic->readers, 0);
	snr.low = 28448;
	snr.high = 0;
	sim_add_heartbeat (SMF_FINAL, &snr, &snr, 2346);
	sim_rx_msg ();

	printf ("Sim: HEARTBEAT (28448, 28448) sent!\r\n");

	sim_wait_acknack (NULL, 0, NULL);

	printf ("Sim: Received ACKNACK () messages - check if corrupted!\r\n");

	aborting = 1;
}



