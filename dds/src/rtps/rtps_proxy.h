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

typedef struct proxy_st Proxy_t;
struct proxy_st {
	Proxy_t		*next_active;
	Proxy_t		*prev_active;
	union {
	  READER	*reader;
	  WRITER	*writer;
	} u;
	unsigned	cstate:2;
	unsigned	tstate:2;
	unsigned	astate:2;
	unsigned	inline_qos:1;
	unsigned	reliable:1;
	unsigned	active:1;
	unsigned	peer_alive:1;
	unsigned	is_writer:1;
	unsigned	heartbeats:1;
	unsigned	marshall:1;
	unsigned	blocked:1;
	unsigned	msg_time:1;
	unsigned	no_mcast:1;
	unsigned	unacked:16;
	CCLIST		changes;
	Endpoint_t	*endpoint;
	Proxy_t		*next_guid;
	RMBUF		*head;
	RMBUF		*tail;
	Proxy_t		*link;
	Timer_t		*timer;
	LocatorNode_t	*uc_reply;
#ifdef EXTRA_STATS
	unsigned	nmsg;
	unsigned	ndata;
	unsigned	ngap;
	unsigned	nheartbeat;
	unsigned	nacknack;
#ifdef RTPS_FRAGMENTS
	unsigned	ndatafrags;
	unsigned	nheartbeatfrags;
	unsigned	nnackfrags;
#endif
#endif
};


