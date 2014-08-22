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

/* ri_tcp.c -- Implements the RTPS over TCP/IP or over TCP/IPv6 transports. */

#ifdef DDS_TCP

#include <stdio.h>
#include <errno.h>
#ifdef _WIN32
#include "win.h"
#include "Ws2IpDef.h"
#include "Ws2tcpip.h"
#include "Iphlpapi.h"
#define ERRNO	WSAGetLastError()
#else
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#define ERRNO errno
#endif
#include "sys.h"
#include "log.h"
#include "error.h"
#include "debug.h"
#include "pool.h"
#include "db.h"
#include "random.h"
#include "rtps.h"
#include "pid.h"
#include "rtps_mux.h"
#ifdef DDS_FORWARD
#include "rtps_fwd.h"
#endif
#ifdef DDS_DYN_IP
#include "dynip.h"
#endif
#include "ri_data.h"
#include "ri_tcp_sock.h"
#include "ri_tcp.h"
#ifdef DDS_SECURITY
#include "ri_tls.h"
#endif

#ifdef DDS_DEBUG
/*#define TCP_SIMULATE_TLS	** Force usage of direct TCP i.o. TLS/TCP. */
/*#define TCP_TRC_CONTROL	** Trace/interpret control msgs if defined. */
/*#define TCP_TRC_STATE 	** Trace/interpret state changes if defined. */
/*#define TCP_TRC_CX		** Trace connection create/match if defined. */
/*#define TCP_TRC_DATA		** Trace all messages if defined. */
/*#define LOG_TCP_SEND		** Trace rtps_tcp_send() decisions. */
/*#define TCP_VALIDATE_CXS	** Verify connection consistency regularly. */
#endif

#ifdef DDS_ACT_LOG
#define	act_printf(s)	log_printf (RTPS_ID, 0, s)
#else
#define	act_printf(s)
#endif

#ifdef TCP_TRC_DATA
#define	trc_print(s)			log_printf (RTPS_ID, 0, s)
#define	trc_print1(s,a)			log_printf (RTPS_ID, 0, s, a)
#define	trc_print_region(p,l,s,o)	log_print_region (RTPS_ID,0,p,l,s,o)
#define	trc_flush()			log_flush (RTPS_ID, 0)
#else
#define	trc_print(s)
#define	trc_print1(s,a)
#define	trc_print_region(p,l,s,o)
#define	trc_flush()
#endif

#define TCP_SHARE	0	/* Share TCP connections for Tx and Rx if 1. */
#ifdef DDS_FORWARD
#define TCP_FWD_FALLBACK	/* Fallback to relay for GUIDPrefix forwarding
				   if defined (needed when relays are used). */
/*#define TCP_FWD_SPDP		** TCP forwards received SPDP messages to TCP. */
#endif

#define	LOCF_MODE	LOCF_MFLAGS

#define	MAX_IP_Q_DESTS	64	/* Max. # of TCP channel sends per multicast. */


/**********************************************************************/
/*   TCP Control protocol.					      */
/**********************************************************************/

#define	TPS	TICKS_PER_SEC

#define	CCWAIT_TO	5 * TPS	/* Time to wait until client->server cx is up. */
#define	CC_WID_TO	2 * TPS
#define	CC_WID_RETRIES	3
#define	IDBREQ_TO	2 * TPS
#define	IDBREQ_RETRIES	3
#define	CLPREQ_TO	2 * TPS
#define	CLPREQ_RETRIES	2
#define	SLPREQ_TO	3 * TPS
#define	SLPREQ_RETRIES	2
#define	CXBREQ_TO	2 * TPS
#define	CXBREQ_RETRIES	2

/* Control protocol fields. */

/* Message types: */
#define	CMT_ID_BIND_REQ		0x0c01
#define	CMT_ID_BIND_SUCC	0x0d01
#define	CMT_ID_BIND_FAIL	0x0e01

#define CMT_SLPORT_REQ		0x0c02
#define	CMT_SLPORT_SUCC		0x0d02
#define	CMT_SLPORT_FAIL		0x0e02

#define	CMT_CLPORT_REQ		0x0c03
#define	CMT_CLPORT_SUCC		0x0d03
#define	CMT_CLPORT_FAIL		0x0e03

#define	CMT_CX_BIND_REQ		0x0c04
#define	CMT_CX_BIND_SUCC	0x0d04
#define	CMT_CX_BIND_FAIL	0x0e04

#define	CMT_FINALIZE		0x0c0f


#define	MK_VENDOR_SPEC	0x8000

#define	PROTOCOL_RPSC	{ 'R', 'P', 'S', 'C' }
ProtocolId_t			rpsc_protocol_id = PROTOCOL_RPSC;
static uint16_t			rpsc_protocol_version = 0x10;
static uint32_t			rpsc_transaction;	/* Next transaction id*/

/* Following the control header are a number of variable length parameters
   encoded as a Parameter_t type (see pid.h). */

/* Following Parameter Ids are defined: */ 
#define	CAID_ERROR		0x0009	/* Error kind. */
#define	CAID_UNKN_ATTR		0x000a	/* Unknown attributes. */
#define	CAID_LOCATOR		0x3d01	/* 128-bit RTPS locator address. */
#define	CAID_LOC_PORT		0x3d02	/* 32-bit RTPS locator port. */
/*#define CAID_CX_TYPE		0x3d03	** 8-bit flags (0x40=WAN):deprecated.*/
#define	CAID_CX_COOKIE		0x3d04	/* Connection identifier. */
#define	CAID_PORT_OPTIONS	0x3d05	/* 8-bit flags (0x80=share Ctrl/Data)*/
#define	CAID_ALLOW_SHARED	0x3d06	/* TCP connection is shared. */
#define	CAID_FORWARD		0x3d07	/* TCP endpoint can forward. */
#define	CAID_GUID_PREFIX	0x3d08	/* GUID prefix. */

/* Error codes: */
#define	CERR_BAD_REQUEST	400	/* Bad request - was malformed. */
#define	CERR_UNKN_ATTR		405	/* Unknown attribute. */
#define	CERR_ALLOC_MISMATCH	406	/* Allocation mismatch - invalid port.*/
#define	CERR_OO_RESOURCES	407	/* Out of resources. */
#define	CERR_UNSUPP_TRANSPORT	415	/* ID_BIND_REQ for unsupported cx-type*/
#define	CERR_EXISTS		446	/* Connection already exists. */
#define	CERR_SERVER_ERROR	503	/* Server error: try again later. */

#define	IDBREQ_PIDS	((1 << (CAID_LOCATOR & 0xff)) |		\
			 (1 << (CAID_FORWARD & 0xff)) |		\
			 (1 << (CAID_GUID_PREFIX & 0xff)))
#define	IDBSUCC_PIDS	((1 << (CAID_LOCATOR & 0xff)) |		\
			 (1 << (CAID_FORWARD & 0xff)) |		\
			 (1 << (CAID_GUID_PREFIX & 0xff)))
#define	IDBFAIL_PIDS	 (1 << (CAID_ERROR & 0xff))
#define	SLPREQ_PIDS	((1 << (CAID_LOC_PORT & 0xff)) |	\
			 (1 << (CAID_PORT_OPTIONS & 0xff)))
#define	SLPSUCC_PIDS	((1 << (CAID_CX_COOKIE & 0xff)) |	\
			 (1 << (CAID_ALLOW_SHARED & 0xff)) |	\
			 (1 << (CAID_GUID_PREFIX & 0xff)))
#define	SLPFAIL_PIDS	 (1 << (CAID_ERROR & 0xff))
#define	CLPREQ_PIDS	((1 << (CAID_LOC_PORT & 0xff)) |	\
			 (1 << (CAID_CX_COOKIE & 0xff)) |	\
			 (1 << (CAID_PORT_OPTIONS & 0xff)))
#define	CLPSUCC_PIDS	 (1 << (CAID_ALLOW_SHARED & 0xff) |	\
			 (1 << (CAID_GUID_PREFIX & 0xff)))
#define	CLPFAIL_PIDS	 (1 << (CAID_ERROR & 0xff))
#define	CXBREQ_PIDS	 (1 << (CAID_CX_COOKIE & 0xff))
#define	CXBSUCC_PIDS	 0
#define	CXBFAIL_PIDS	 (1 << (CAID_ERROR & 0xff))
#define	FINALIZE_PIDS	 0

/* Port options: */

/* - Type of data. */
#define	PO_DATA		0x01	/* Used for User Data. */
#define	PO_META		0x02	/* Used for Meta Data. */
#define	PO_UCAST	0x04	/* Used for Unicast Data. */
#define	PO_MCAST	0x08	/* Used for Multicast Data. */

#define	PO_MODE		(PO_DATA | PO_META | PO_UCAST | PO_MCAST)

/* - Connection sharing. */
#define	PO_SHARE	0x80	/* Share with reverse cx. */

typedef enum {
	RPSC_OK,
	RPSC_FRAME_INCOMPLETE,	/* Message not completely received, continue later. */
	RPSC_ERR_RTPS,		/* RTPS data message. */
	RPSC_ERR_INV_MSG,	/* Incorrect message type/version. */
	RPSC_ERR_VENDOR_KIND,	/* Vendor-specific message. */
	RPSC_ERR_UNKNOWN_KIND,	/* Message kind unknown. */
	RPSC_ERR_INV_LENGTH,	/* Incorrect length. */
	RPSC_ERR_INV_PAR,	/* Unknown parameter. */
	RPSC_ERR_NOMEM		/* Out of memory. */
} ParseError_t;

#define	MAX_COOKIE	16	/* Max. expected cookie data. */

static long tcp_cookie;

/* Parse result or generation data for known message types: */
typedef struct ctrl_info_st {

	/* Parse results: */
	ParseError_t	 result;
	ControlMsgKind_t type;
	unsigned char	 transaction [12];
	unsigned	 pids;

	/* Parameter values: */
	unsigned	 error_kind;
	unsigned short	 parameter_id;
	unsigned char	 port_options;
	int		 shared;
	unsigned	 forward;
	unsigned	 port;
	unsigned char	 address [16];
	unsigned char	 *cookie;
	unsigned	 cookie_length;
	GuidPrefix_t	 prefix;
} CtrlInfo_t;

#ifdef TCP_TRC_CONTROL

static struct err_kind_st {
	unsigned	error;
	const char	*string;
} ctrl_errs [] = {
	{ CERR_BAD_REQUEST,      "Bad request" },
	{ CERR_UNKN_ATTR,        "Unknown attribute" },
	{ CERR_ALLOC_MISMATCH,   "Allocation mismatch" },
	{ CERR_OO_RESOURCES,     "Out of resources" },
	{ CERR_UNSUPP_TRANSPORT, "Unsupported transport" },
	{ CERR_EXISTS,           "Already exists" },
	{ CERR_SERVER_ERROR,     "Server error" }
};

#define	N_CTRL_ERRS	(sizeof (ctrl_errs) / sizeof (struct err_kind_st))

static void tcp_trace_ctrl (int tx, int fd, unsigned char *buf, unsigned len)
{
	CtrlHeader	*hp;
	Parameter_t	*pp;
	GuidPrefix_t	*prp;
	unsigned char	*dp;
	unsigned	i, j, h, l, c, left, n;
	uint32_t	u;
	char		sbuf [28];
	static const char *cmd_str [] = { "IdBind", "SLPort", "CLPort", "CxBind" },
			  *mode_str [] = { "Request", "Success", "Fail" },
			  *par_str [] = { "Locator", "ReqPort", NULL, 
				  	  "Cookie", "PortOpts", "AllowShare",
					  "Forward", "GUIDPrefix", "Error", "UnknAttr" };

	log_printf (RTPS_ID, 0, "RPSC: %c [%d] - %3u: ", (tx) ? 'T' : 'R', fd, len);
	hp = (CtrlHeader *) buf;
	i = 0;
	if (!ctrl_protocol_valid (hp->protocol) ||
	    hp->version < rpsc_protocol_version) {
		log_printf (RTPS_ID, 0, "???");
		goto dump;
	}
	for (i = 8; i < 20; i++) {
		if (i == 12 || i == 16)
			log_printf (RTPS_ID, 0, ":");
		log_printf (RTPS_ID, 0, "%02x", buf [i]);
	}
	c = hp->msg_kind;
	i += 2;
	if ((c & MK_VENDOR_SPEC) != 0) {
		log_printf (RTPS_ID, 0, " ?(%u)", c);
		goto dump;
	}
	h = hp->msg_kind >> 8;
	l = hp->msg_kind & 0xff;
	log_printf (RTPS_ID, 0, " - ");
	if (h < 12 || h > 14 || l < 1 || (l > 4 && (l != 15 || h != 12))) {
		log_printf (RTPS_ID, 0, "?(%u)", hp->msg_kind);
		goto dump;
	}
	if (l == 15)
		log_printf (RTPS_ID, 0, "Finalize\r\n");
	else
		log_printf (RTPS_ID, 0, "%s%s\r\n", cmd_str [l - 1], mode_str [h - 12]);
	if (!hp->length || (hp->length == 4 && buf [24] == 1 && buf [25] == 0))
		return;

	i += 2;
	left = hp->length;
	dp = (unsigned char *) (hp + 1);
	while (left >= 2) {
		pp = (Parameter_t *) dp;
		if (pp->parameter_id == PID_SENTINEL)
			return;

		n = pp->length + 4U;
		if (n > left ||
		    (pp->length & 0x3) != 0) {
		    	log_printf (RTPS_ID, 0, "\t? ");
			goto dump;
		}
		dp += n;
		left -= n;
		h = pp->parameter_id >> 8;
		if (h >= 0x80) { /* Vendor-specific parameter. */
		    	log_printf (RTPS_ID, 0, "\t? ");
			goto dump;
		}
		l = pp->parameter_id & 0xff;
		if ((!h && !(l == 9 || l == 10)) ||
		    (h == 0x3d && (l < 1 || l > 8)) ||
		    (h != 0 && h != 0x3d)) {
			log_printf (RTPS_ID, 0, "\t? ");
			goto dump;
		}
		i += 4;
		if (par_str [l - 1])
			log_printf (RTPS_ID, 0, "\t%10s: ", par_str [l - 1]);
		else
			log_printf (RTPS_ID, 0, "\t%10d?: ", l);
		if (pp->parameter_id == CAID_ERROR ||
		    pp->parameter_id == CAID_LOC_PORT) {
			memcpy (&u, &buf [i], 4);
			log_printf (RTPS_ID, 0, "%u", u);
			i += pp->length;
			if (pp->parameter_id == CAID_ERROR) {
				for (j = 0; j < N_CTRL_ERRS; j++)
					if (ctrl_errs [j].error == u)
						break;

				if (j < N_CTRL_ERRS)
					log_printf (RTPS_ID, 0, ": %s", ctrl_errs [j].string);
				else
					log_printf (RTPS_ID, 0, ".");
			}
			else
				log_printf (RTPS_ID, 0, ".");
		}
		else if (pp->parameter_id == CAID_PORT_OPTIONS) {
			if ((buf [i] & PO_DATA) != 0)
				log_printf (RTPS_ID, 0, "User ");
			if ((buf [i] & PO_META) != 0)
				log_printf (RTPS_ID, 0, "Meta ");
			if ((buf [i] & PO_UCAST) != 0)
				log_printf (RTPS_ID, 0, "UCast ");
			if ((buf [i] & PO_MCAST) != 0)
				log_printf (RTPS_ID, 0, "MCast ");
			if ((buf [i] & PO_SHARE) != 0)
				log_printf (RTPS_ID, 0, "Share");
			else
				log_printf (RTPS_ID, 0, "NoShare");
			i += pp->length;
		}
		else if (pp->parameter_id == CAID_ALLOW_SHARED) {
			if (buf [i])
				log_printf (RTPS_ID, 0, "Share");
			else
				log_printf (RTPS_ID, 0, "NoShare");
			i += pp->length;
		}
		else if (pp->parameter_id == CAID_FORWARD) {
			log_printf (RTPS_ID, 0, "%u", buf [i]);
			i += pp->length;
		}
		else if (pp->parameter_id == CAID_GUID_PREFIX) {
			prp = (GuidPrefix_t *) &buf [i];
			i += 12;
			log_printf (RTPS_ID, 0, "%s", guid_prefix_str (prp, sbuf));
		}
		else {
			for (j = 0; j < pp->length; j++)
				log_printf (RTPS_ID, 0, "%02x", buf [i++]);
		}
		log_printf (RTPS_ID, 0, "\r\n");
	}

    dump:
	for (; i < len; i++) {
		log_printf (RTPS_ID, 0, " %02x", buf [i]);
	}
	log_printf (RTPS_ID, 0, "\r\n");
}

#endif /* TCP_TRC_CONTROL */

#ifdef TCP_TRC_STATE

#define	TCP_NCX_STATE(t,cxp,s)	tcp_trace_cx_state(t, cxp, s)
#define	TCP_NP_STATE(t,cxp,s)	tcp_trace_p_state(t, cxp, s)

static void tcp_trace_cx_state (const char *type, IP_CX *cxp, IP_CX_STATE ns)
{
	static const char *cx_state_str [] = {
		"CLOSED", "LISTEN", "CAUTH", "CONREQ", "CONNECT", "WRETRY", "SAUTH", "OPEN"
	};

	if (ns != cxp->cx_state) {
		log_printf (RTPS_ID, 0, "TCP(%s", type);
		if (cxp->handle)
			log_printf (RTPS_ID, 0, ":%u", cxp->handle);
		log_printf (RTPS_ID, 0, ") CX: %s -> %s\r\n",
				cx_state_str [cxp->cx_state], 
				cx_state_str [ns]);
		cxp->cx_state = ns;
	}
}

static void tcp_trace_p_state (const char *type, IP_CX *cxp, int ns)
{
	static const char *c_state_str [] = {
		"IDLE", "WCXOK", "WIBINDOK", "CONTROL"
	};
	static const char *d_state_str [] = {
		"IDLE", "WCONTROL", "WPORTOK", "WCXOK", "WCBINDOK", "DATA"
	};

	if (ns != cxp->p_state) {
		log_printf (RTPS_ID, 0, "TCP(%s", type);
		if (cxp->handle)
			log_printf (RTPS_ID, 0, ":%u", cxp->handle);
		log_printf (RTPS_ID, 0, ") ");
		if (cxp->cx_mode == ICM_CONTROL)
			log_printf (RTPS_ID, 0, "C: %s -> %s\r\n",
				c_state_str [cxp->p_state], 
				c_state_str [ns]);
		else
			log_printf (RTPS_ID, 0, "D: %s -> %s\r\n",
				d_state_str [cxp->p_state], 
				d_state_str [ns]);
		cxp->p_state = ns;
	}
}


#else /* !TCP_TRC_STATE */

#define	TCP_NCX_STATE(t,cxp,s)	cxp->cx_state = s
#define	TCP_NP_STATE(t,cxp,s)	cxp->p_state = s

#endif /* !TCP_TRC_STATE */

static void info_cleanup (CtrlInfo_t *info)
{
	if (info->cookie) {
		xfree (info->cookie);
		info->cookie = NULL;
	}
}

static void rpsc_parse_pids (CtrlHeader *hp, unsigned pid_set, CtrlInfo_t *info)
{
	Parameter_t	*pp;
	unsigned char	*dp, *p;
	unsigned	left, id, n;
	uint32_t	u;
	uint16_t	s;

	left = hp->length;
	dp = (unsigned char *) (hp + 1);
	info->result = RPSC_OK;
	while (left >= 2) {
		pp = (Parameter_t *) dp;
		if (pp->parameter_id == PID_SENTINEL)
			break;

		n = pp->length + 4U;
		if (n > left ||
		    (pp->length & 0x3) != 0) {
			info->result = RPSC_ERR_INV_LENGTH;
			return;
		}
		dp += n;
		left -= n;
		if (pp->parameter_id >= 0x8000)
			continue;	/* Ignore vendor-specific parameters. */

		id = pp->parameter_id & 0xff;
		if (id > 31 || ((1 << id) & pid_set) == 0) {
			info->result = RPSC_ERR_INV_PAR;
			break;
		}
		if (((1 << id) & info->pids) != 0)
			continue;	/* Ignore if multiple occurence. */

		info->pids |= (1 << id);
		switch (id) {
			case CAID_ERROR:
				if (pp->length != 4) {
					info->result = RPSC_ERR_INV_LENGTH;
					break;
				}
				memcpy (&u, pp->value, sizeof (uint32_t));
				info->error_kind = u;
				break;
			case CAID_UNKN_ATTR:
				if (pp->length != 4) {
					info->result = RPSC_ERR_INV_LENGTH;
					break;
				}
				memcpy (&s, pp->value, sizeof (uint16_t));
				info->parameter_id = s;
				break;
			case CAID_LOCATOR & 0xff:
				if (pp->length != 16) {
					info->result = RPSC_ERR_INV_LENGTH;
					break;
				}
				memcpy (info->address, pp->value, 16);
				break;
			case CAID_LOC_PORT & 0xff:
				if (pp->length != 4) {
					info->result = RPSC_ERR_INV_LENGTH;
					break;
				}
				memcpy (&u, pp->value, sizeof (uint32_t));
				info->port = u;
				break;
			case CAID_CX_COOKIE & 0xff:
				if (info->cookie)
					p = xrealloc (info->cookie, pp->length);
				else
					p = xmalloc (pp->length);
				if (!p) {
					info->result = RPSC_ERR_NOMEM;
					break;
				}
				info->cookie = p;
				info->cookie_length = pp->length;
				memcpy (info->cookie, pp->value, pp->length);
				break;
			case CAID_PORT_OPTIONS & 0xff:
				if (pp->length != 4) {
					info->result = RPSC_ERR_INV_LENGTH;
					break;
				}
				info->port_options = pp->value [0];
				break;
			case CAID_ALLOW_SHARED & 0xff:
				if (pp->length != 4) {
					info->result = RPSC_ERR_INV_LENGTH;
					break;
				}
				info->shared = (pp->value [0] != 0);
				break;
			case CAID_FORWARD & 0xff:
				if (pp->length != 4) {
					info->result = RPSC_ERR_INV_LENGTH;
					break;
				}
				info->forward = pp->value [0];
				break;
			case CAID_GUID_PREFIX & 0xff:
				if (pp->length != 12) {
					info->result = RPSC_ERR_INV_LENGTH;
					break;
				}
				memcpy (info->prefix.prefix, pp->value, 12);
				break;
		}
		if (info->result)
			break;
	}
	if (info->result)
		info_cleanup (info);
}

static void rpsc_parse (unsigned char *rx, size_t length, CtrlInfo_t *info)
{
	unsigned		h, l;
	CtrlHeader		*chp;

	ARG_NOT_USED (length)

	memset (info, 0, sizeof (CtrlInfo_t));
	chp = (CtrlHeader *) rx;

	/* Validate protocol type and version. */
	if (!ctrl_protocol_valid (chp->protocol) ||
	    chp->version < rpsc_protocol_version) {
		info->result = RPSC_ERR_INV_MSG;
		return;
	}
	if ((chp->msg_kind & MK_VENDOR_SPEC) != 0) {
		info->result = RPSC_ERR_VENDOR_KIND;
		return;
	}
	info->type = chp->msg_kind;
	memcpy (info->transaction, chp->transaction, 12);
	h = chp->msg_kind >> 8;
	l = chp->msg_kind & 0xff;
	if (h < 12 || h > 14) {
		info->result = RPSC_ERR_UNKNOWN_KIND;
		return;
	}
	if (l < 1 || (l > 4 && l != 15)) {
		info->result = RPSC_ERR_UNKNOWN_KIND;
		return;
	}
	switch (l) {
		case 1:	if (h == 12) 		/* CMT_ID_BIND_REQ */
				rpsc_parse_pids (chp, IDBREQ_PIDS, info);
			else if (h == 13)	/* CMT_ID_BIND_SUCC */
				rpsc_parse_pids (chp, IDBSUCC_PIDS, info);
			else			/* CMT_ID_BIND_FAIL */
				rpsc_parse_pids (chp, IDBFAIL_PIDS, info);
			break;
		case 2:	if (h == 12)		/* CMT_SLPORT_REQ */
				rpsc_parse_pids (chp, SLPREQ_PIDS, info);
			else if (h == 13)	/* CMT_SLPORT_SUCC */
				rpsc_parse_pids (chp, SLPSUCC_PIDS, info);
			else			/* CMT_SLPORT_FAIL */
				rpsc_parse_pids (chp, SLPFAIL_PIDS, info);
			break;
		case 3:	if (h == 12)		/* CMT_CLPORT_REQ */
				rpsc_parse_pids (chp, CLPREQ_PIDS, info);
			else if (h == 13)	/* CMT_CLPORT_SUCC */
				rpsc_parse_pids (chp, CLPSUCC_PIDS, info);
			else			/* CMT_CLPORT_FAIL */
				rpsc_parse_pids (chp, CLPFAIL_PIDS, info);
			break;
		case 4:	if (h == 12)		/* CMT_CX_BIND_REQ */
				rpsc_parse_pids (chp, CXBREQ_PIDS, info);
			else if (h == 13)	/* CMT_CX_BIND_SUCC */
				rpsc_parse_pids (chp, CXBSUCC_PIDS, info);
			else			/* CMT_CX_BIND_FAIL */
				rpsc_parse_pids (chp, CXBFAIL_PIDS, info);
			break;
		case 15:
			if (h == 12)		/* CMT_FINALIZE */
				rpsc_parse_pids (chp, FINALIZE_PIDS, info);
			else {
				info->result = RPSC_ERR_UNKNOWN_KIND;
				return;
			}
			break;
		default:
			info->result = RPSC_ERR_UNKNOWN_KIND;
			return;
	}
}

static size_t rpsc_create (unsigned char *tx, CtrlInfo_t *info, int new_tr)
{
	CtrlHeader		*chp;
	Parameter_t		*pp;
	size_t			length;
	unsigned		pids, i, j, m, n;
	unsigned char		*dp;
	uint32_t		u;
	uint16_t		s;

	if (!tx || !info)
		return (0);

	length = 0;
	chp = (CtrlHeader *) tx;
	memcpy (chp->protocol, rpsc_protocol_id, 4);
	chp->version = 0x10;
	vendor_id_init (chp->vendor_id);
	if (new_tr) {
		memcpy (info->transaction, guid_local (), 8);
		rpsc_transaction++;
		memcpy (info->transaction + 8, &rpsc_transaction, 4);
	}
	memcpy (chp->transaction, info->transaction, 12);
	chp->msg_kind = info->type;
	chp->length = 0;
	length = sizeof (CtrlHeader);
	if (!info->pids)
		return (length);

	dp = (unsigned char *) (chp + 1);
	pids = info->pids;
	for (i = 0, m = 1U; pids; i++, m <<= 1U) {
		if ((pids & m) == 0)
			continue;

		pids &= ~m;
		pp = (Parameter_t *) dp;
		pp->parameter_id = i;
		if (i < 9)
			pp->parameter_id |= 0x3d00;
		switch (i) {
			case CAID_ERROR:
				n = 4;
				u = info->error_kind;
				memcpy (pp->value, &u, sizeof (uint32_t));
				break;
			case CAID_UNKN_ATTR:
				n = 4;
				s = info->parameter_id;
				memcpy (pp->value, &s, sizeof (uint16_t));
				pp->value [2] = pp->value [3] = 0;
				break;
			case CAID_LOCATOR & 0xff:
				n = 16;
				memcpy (pp->value, info->address, 16);
				break;
			case CAID_LOC_PORT & 0xff:
				n = 4;
				u = info->port;
				memcpy (pp->value, &u, sizeof (uint32_t));
				break;
/*			case CAID_CX_TYPE & 0xff: deprecated!
				n = 4;
				pp->value [0] = info->cx_type;
				pp->value [1] = pp->value [2] = pp->value [3] = 0;
				break;*/
			case CAID_CX_COOKIE & 0xff:
				n = info->cookie_length;
				n = (n + 3) & ~3;
				memcpy (pp->value, info->cookie, info->cookie_length);
				for (j = info->cookie_length; j < n; j++)
					pp->value [j] = 0;
				break;
			case CAID_PORT_OPTIONS & 0xff:
				n = 4;
				pp->value [0] = info->port_options;
				pp->value [1] = pp->value [2] = pp->value [3] = 0;
				break;
			case CAID_ALLOW_SHARED & 0xff:
				n = 4;
				pp->value [0] = info->shared;
				pp->value [1] = pp->value [2] = pp->value [3] = 0;
				break;
			case CAID_FORWARD & 0xff:
				n = 4;
				pp->value [0] = info->forward;
				pp->value [1] = pp->value [2] = pp->value [3] = 0;
				break;
			case CAID_GUID_PREFIX & 0xff:
				n = 12;
				memcpy (pp->value, info->prefix.prefix, 12);
				break;
			default:
				n = 0;
				break;
		}
		pp->length = n;
		n += 4;
		chp->length += n;
		length += n;
		dp += n;
	}
	pp = (Parameter_t *) dp;
	pp->parameter_id = PID_SENTINEL;
	pp->length = 0;
	chp->length += 4;
	return (length + 4);
}

static void tcp_close_fd (IP_CX *cxp);

static WR_RC tcp_send_ctrl (IP_CX *cxp, CtrlInfo_t *info, int new_tr)
{
	size_t		n;
	WR_RC		rc;
	unsigned char	*txp;

	n = rpsc_create (rtps_tx_buf, info, new_tr);
	if (!n) {
		warn_printf ("TCP: can't create control message!");
		cxp->stats.nomem++;
		return (WRITE_ERROR);
	}

	/* Set txp either to a newly allocated buffer (TLS expects to reuse the
	   same buffer data in some situations) or to rtps_tx_buf (for UDP).
	   If set to a new buffer, TLS is responsible to xfree() it when it
	   doesn't need it anymore. */
#ifdef DDS_SECURITY
	if (cxp->stream_fcts == &tls_functions) {
		txp = xmalloc (n);
		if (!txp) {
			warn_printf ("tcp_send_ctrl: out-of-memory for send buffer!");
			cxp->stats.nomem++;
			return (WRITE_ERROR);
		}
		memcpy (txp, rtps_tx_buf, n);
	}
	else
#endif
		txp = rtps_tx_buf;

	trc_print1 ("C:T[%d:", cxp->fd);
	rc = cxp->stream_fcts->write_msg (cxp, txp, n);
	if (rc == WRITE_FATAL) {
		log_printf (RTPS_ID, 0, "tcp_send_ctrl: fatal error sending control message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));
		return (rc);
	}
	else if (rc == WRITE_ERROR) {
		log_printf (RTPS_ID, 0, "tcp_send_ctrl: error sending control message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));
		cxp->stats.write_err++;
		return (rc);
	}
	else if (rc == WRITE_BUSY) {
		log_printf (RTPS_ID, 0, "tcp_send_ctrl: Write still busy from previous control message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));
		if (info->type != CMT_CX_BIND_SUCC ||	/* Retry after time-out! */
		    cxp->cxbs_queued) 			/* Already pending! */
			return (rc);

		txp = xmalloc (n);
		if (!txp) {
			warn_printf ("tcp_send_ctrl: out-of-memory for retry buffer(2)!");
			cxp->stats.nomem++;
			return (WRITE_ERROR);
		}
		memcpy (txp, rtps_tx_buf, n);
		cxp->data = txp;
		cxp->data_length = n;
		cxp->cxbs_queued = 1;
		return (rc);
	}
	else if (rc == WRITE_PENDING) {
		log_printf (RTPS_ID, 0, "tcp_send_ctrl: Write pending of following control message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));
	}
	trc_print_region (rtps_tx_buf, n, 0, 0);
	trc_print ("]\r\n");
	ADD_ULLONG (cxp->stats.octets_sent, n);
	cxp->stats.packets_sent++;
#ifdef TCP_TRC_CONTROL
	tcp_trace_ctrl (1, cxp->fd, rtps_tx_buf, n);
#endif
	return (rc);
}

static DDS_ReturnCode_t tcp_send_request (IP_CX      *ccxp,
					  IP_CX      *dcxp,
					  CtrlInfo_t *info,
					  TMR_FCT    fct,
					  Ticks_t    t)
{
	int	err;

	err = tcp_send_ctrl (ccxp, info, 1);
	if (err == WRITE_FATAL)
		return (DDS_RETCODE_ALREADY_DELETED);

	if (!dcxp->timer) {
		dcxp->timer = tmr_alloc ();
		if (!dcxp->timer)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		tmr_init (dcxp->timer, "ReqTimer");
	}
	log_printf (RTPS_ID, 0, "TCP(%u): timer started (%u)!\r\n", dcxp->handle, dcxp->dst_port);
	tmr_start (dcxp->timer, t, (uintptr_t) dcxp, fct);
	return ((err == WRITE_OK || err == WRITE_PENDING) ? 
				DDS_RETCODE_OK : DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

static void cc_restart (unsigned id);

static DDS_ReturnCode_t tcp_send_id_bind_request (IP_CX *cxp, TMR_FCT fct, Ticks_t t)
{
	CtrlInfo_t	 info;
	unsigned	 id;
	DDS_ReturnCode_t r;

	/*log_printf (RTPS_ID, 0, "TCP(CC): Sending IdentityBindRequest\r\n");*/
	memset (&info, 0, sizeof (info));
	info.type = CMT_ID_BIND_REQ;
	info.pids = IDBREQ_PIDS;
	if ((cxp->locator->locator.kind & LOCATOR_KINDS_IPv4) != 0)
		memcpy (info.address + 12, ipv4_proto.own, 4);
#ifdef DDS_IPV6
	else if ((cxp->locator->locator.kind & LOCATOR_KINDS_IPv6) != 0)
		memcpy (info.address, ipv6_proto.own, 16);
#endif
	else
		warn_printf ("TCP: can't set IdentityBindRequest address!");

#ifdef DDS_FORWARD
	info.forward = rtps_forward;
#endif
	memcpy (info.prefix.prefix, guid_local (), 12);
	guid_normalise (&info.prefix);
	id = cxp->id;
	r = tcp_send_request (cxp, cxp, &info, fct, t);
	if (r == DDS_RETCODE_ALREADY_DELETED) {
		cc_restart (id);
		return (r);
	}
	memcpy (&cxp->transaction_id, info.transaction + 8, 4);
	return (r);
}

static DDS_ReturnCode_t tcp_send_id_bind_success (IP_CX *cxp, CtrlInfo_t *c_info)
{
	CtrlInfo_t	s_info;

	/*log_printf (RTPS_ID, 0, "TCP(SC): Sending IdentityBindSuccess\r\n");*/
	memset (&s_info, 0, sizeof (s_info));
	s_info.type = CMT_ID_BIND_SUCC;
	s_info.pids = IDBSUCC_PIDS;
	memcpy (&s_info.transaction, &c_info->transaction, sizeof (TransactionId_t));
	if ((cxp->locator->locator.kind & LOCATOR_KINDS_IPv4) != 0)
		memcpy (s_info.address + 12, ipv4_proto.own, 4);
#ifdef DDS_IPV6
	else if ((cxp->locator->locator.kind & LOCATOR_KINDS_IPv6) != 0)
		memcpy (s_info.address, ipv6_proto.own, 16);
#endif
	else
		warn_printf ("TCP: can't set IdentityBindRequest address!");

#ifdef DDS_FORWARD
	s_info.forward = rtps_forward;
#endif
	memcpy (s_info.prefix.prefix, guid_local (), 12);
	if (tcp_send_ctrl (cxp, &s_info, 0) == WRITE_FATAL) {
		tcp_close_fd (cxp);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	else
		return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t tcp_send_id_bind_fail (IP_CX *cxp, CtrlInfo_t *c_info, unsigned error)
{
	CtrlInfo_t	s_info;

	/*log_printf (RTPS_ID, 0, "TCP(SC): Sending IdentityBindFail\r\n");*/
	memset (&s_info, 0, sizeof (s_info));
	s_info.type = CMT_ID_BIND_FAIL;
	s_info.pids = IDBFAIL_PIDS;
	memcpy (&s_info.transaction, &c_info->transaction, sizeof (TransactionId_t));
	s_info.error_kind = error;
	if (tcp_send_ctrl (cxp, &s_info, 0) == WRITE_FATAL) {
		tcp_close_fd (cxp);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	else
		return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t tcp_send_slport_request (IP_CX   *cxp,
						 TMR_FCT fct,
						 Ticks_t t,
						 int     share)
{
	CtrlInfo_t	 info;
	unsigned	 id;
	DDS_ReturnCode_t ret;

	/*log_printf (RTPS_ID, 0, "TCP(CC): Sending ServerLogicalPortRequest\r\n");*/
	memset (&info, 0, sizeof (info));
	info.type = CMT_SLPORT_REQ;
	info.pids = SLPREQ_PIDS;
	info.port = cxp->dst_port;
	info.port_options = (cxp->locator->locator.flags & LOCF_MODE);
	if (share)
		info.port_options |= PO_SHARE;
	id = cxp->id;
	ret = tcp_send_request (cxp->parent, cxp, &info, fct, t);
	if (ret == DDS_RETCODE_ALREADY_DELETED) {
		cc_restart (id);
		return (ret);
	}
	memcpy (&cxp->transaction_id, info.transaction + 8, 4);
	return (ret);
}

static DDS_ReturnCode_t tcp_send_slport_success (IP_CX         *cxp,
						 CtrlInfo_t    *c_info,
						 unsigned char *cookie,
						 size_t        length,
						 GuidPrefix_t  *prefix)
{
	CtrlInfo_t	s_info;

	/*log_printf (RTPS_ID, 0, "TCP(SC): Sending ServerLogicalPortSuccess\r\n");*/
	memset (&s_info, 0, sizeof (s_info));
	s_info.type = CMT_SLPORT_SUCC;
	s_info.pids = SLPSUCC_PIDS;
	s_info.cookie = cookie;
	s_info.cookie_length = length;
	if (prefix)
		s_info.prefix = *prefix;
	else
		s_info.pids &= ~(1 << (CAID_GUID_PREFIX & 0xff));
	memcpy (&s_info.transaction, &c_info->transaction, sizeof (TransactionId_t));
	memcpy (s_info.address, c_info->address, 16);
	s_info.shared = c_info->shared;
	if (tcp_send_ctrl (cxp, &s_info, 0) == WRITE_FATAL) {
		tcp_close_fd (cxp);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	else
		return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t tcp_send_slport_fail (IP_CX      *cxp,
					      CtrlInfo_t *c_info,
					      unsigned   error)
{
	CtrlInfo_t	s_info;

	/*log_printf (RTPS_ID, 0, "TCP(SC): Sending ServerLogicalPortFail\r\n");*/
	memset (&s_info, 0, sizeof (s_info));
	s_info.type = CMT_SLPORT_FAIL;
	s_info.pids = SLPFAIL_PIDS;
	memcpy (&s_info.transaction, &c_info->transaction, sizeof (TransactionId_t));
	s_info.error_kind = error;
	if (tcp_send_ctrl (cxp, &s_info, 0) == WRITE_FATAL) {
		tcp_close_fd (cxp);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	else
		return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t tcp_send_clport_request (IP_CX    *cxp,
						 TMR_FCT  fct,
						 Ticks_t  t,
						 void     *cookie,
						 size_t   length,
						 int      share)
{
	CtrlInfo_t	 info;
	DDS_ReturnCode_t ret;

	/*log_printf (RTPS_ID, 0, "TCP(CC): Sending ClientLogicalPortRequest\r\n");*/
	memset (&info, 0, sizeof (info));
	info.type = CMT_CLPORT_REQ;
	info.pids = CLPREQ_PIDS;
	info.port = cxp->dst_port;
	info.port_options = (cxp->locator->locator.flags & LOCF_MODE);
	if (share)
		info.port_options |= PO_SHARE;
	if (cookie) {
		info.cookie = cookie;
		info.cookie_length = length;
	}
	else
		info.pids &= ~(1 << (CAID_CX_COOKIE & 0xff));
	ret = tcp_send_request (cxp->parent, cxp, &info, fct, t);
	if (ret == DDS_RETCODE_ALREADY_DELETED) {
		tcp_close_fd (cxp);
		return (ret);
	}
	memcpy (&cxp->transaction_id, info.transaction + 8, 4);
	return (ret);
}

static DDS_ReturnCode_t tcp_send_clport_success (IP_CX        *cxp,
						 CtrlInfo_t   *c_info,
						 int          share,
						 GuidPrefix_t *prefix)
{
	CtrlInfo_t	s_info;
	unsigned	id;

	/*log_printf (RTPS_ID, 0, "TCP(CC): Sending ClientLogicalPortSuccess\r\n");*/
	memset (&s_info, 0, sizeof (s_info));
	s_info.type = CMT_CLPORT_SUCC;
	s_info.pids = CLPSUCC_PIDS;
	memcpy (&s_info.transaction, &c_info->transaction, sizeof (TransactionId_t));
	memcpy (s_info.address, c_info->address, 16);
	s_info.shared = share;
	if (prefix)
		s_info.prefix = *prefix;
	else
		s_info.pids &= ~(1 << (CAID_GUID_PREFIX & 0xff));
	id = cxp->id;
	if (tcp_send_ctrl (cxp, &s_info, 0) == WRITE_FATAL) {
		cc_restart (id);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	else
		return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t tcp_send_clport_fail (IP_CX      *cxp,
					      CtrlInfo_t *c_info,
					      unsigned   error)
{
	CtrlInfo_t	s_info;
	unsigned	id;

	/*log_printf (RTPS_ID, 0, "TCP(SC): Sending ClientLogicalPortFail\r\n");*/
	memset (&s_info, 0, sizeof (s_info));
	s_info.type = CMT_CLPORT_FAIL;
	s_info.pids = CLPFAIL_PIDS;
	memcpy (&s_info.transaction, &c_info->transaction, sizeof (TransactionId_t));
	s_info.error_kind = error;
	id = cxp->id;
	if (tcp_send_ctrl (cxp, &s_info, 0) == WRITE_FATAL) {
		cc_restart (id);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	else
		return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t tcp_send_cx_bind_request (IP_CX *cxp, TMR_FCT fct, Ticks_t t)
{
	CtrlInfo_t	 info;
	DDS_ReturnCode_t ret;

	/*log_printf (RTPS_ID, 0, "TCP(CDx): Sending ConnectionBindRequest\r\n");*/
	memset (&info, 0, sizeof (info));
	info.type = CMT_CX_BIND_REQ;
	info.pids = CXBREQ_PIDS;
	info.cookie = cxp->data;
	info.cookie_length = cxp->data_length;
	ret = tcp_send_request (cxp, cxp, &info, fct, t);
	if (ret == DDS_RETCODE_ALREADY_DELETED) {
		tcp_close_fd (cxp);
		return (ret);
	}
	memcpy (&cxp->transaction_id, info.transaction + 8, 4);
	return (ret);
}

static DDS_ReturnCode_t tcp_send_cx_bind_success (IP_CX *cxp, CtrlInfo_t *c_info)
{
	CtrlInfo_t	s_info;

	/*log_printf (RTPS_ID, 0, "TCP(SDx): Sending ConnectionBindSuccess\r\n");*/
	memset (&s_info, 0, sizeof (s_info));
	s_info.type = CMT_CX_BIND_SUCC;
	s_info.pids = CXBSUCC_PIDS;
	memcpy (&s_info.transaction, &c_info->transaction, sizeof (TransactionId_t));
	memcpy (s_info.address, c_info->address, 16);
	if (tcp_send_ctrl (cxp, &s_info, 0) == WRITE_FATAL) {
		tcp_close_fd (cxp);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	else
		return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t tcp_send_finalize (IP_CX *cxp)
{
	CtrlInfo_t	s_info;

	/*log_printf (RTPS_ID, 0, "TCP(xx): Sending Finalize\r\n");*/
	memset (&s_info, 0, sizeof (s_info));
	s_info.type = CMT_FINALIZE;
	s_info.pids = FINALIZE_PIDS;
	if (tcp_send_ctrl (cxp, &s_info, 1) == WRITE_FATAL) {
		tcp_close_fd (cxp);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	else
		return (DDS_RETCODE_OK);
}

static int tcp_rx_parse_msg (int           fd,
                             CtrlInfo_t    *info,
			     unsigned char *rx_buf,
			     size_t        size)
{
	int		rpsc_msg;
	CtrlHeader	*hp;

	memset (info, 0, sizeof (CtrlInfo_t));
	hp = (CtrlHeader *) rx_buf;
	if (ctrl_protocol_valid (hp))
		rpsc_msg = 1;
	else if (protocol_valid (hp))
		rpsc_msg = 0;
	else
		return (-1);

	if (rpsc_msg) {
#ifdef TCP_TRC_CONTROL
		tcp_trace_ctrl (0, fd, rx_buf, size);
#else
		ARG_NOT_USED (fd);
#endif
		rpsc_parse (rx_buf, size, info);
	}
	else {
		info->result = RPSC_ERR_RTPS;
		trc_print1 ("RTPS:%lu bytes]\r\n", (unsigned long) size);
	}
	return (0);
}


/**********************************************************************/
/*   TCP common functions.					      */
/**********************************************************************/

RTPS_TCP_PARS		tcp_v4_pars;
#ifdef DDS_IPV6
RTPS_TCP_PARS		tcp_v6_pars;
#endif
IP_CX			*tcpv4_server;
#ifdef DDS_IPV6
IP_CX			*tcpv6_server;
#endif
IP_CX			*tcp_client [TCP_MAX_CLIENTS];
static char		*server_args;
static char		server_buf [64];
static char		*public_args;
static char		public_buf [64];
static unsigned char	tcp_mcast_ip [] = { 239, 255, 0, 1 };

int			tcp_available = 1;

#ifdef TCP_VALIDATE_CXS

static void tcp_cxs_validate (void)
{
	IP_CX		*ccxp, *cxp;
	unsigned	i;

	if (tcpv4_server)
		for (ccxp = tcpv4_server->clients; ccxp; ccxp = ccxp->next) {
			if (ccxp == ccxp->next || ccxp->parent != tcpv4_server)
				abort ();
			if (!cxp->fd)
				abort;

			for (cxp = ccxp->clients; cxp; cxp = cxp->next) {
				if (cxp == cxp->next || cxp->parent != ccxp)
					abort ();
				if (!cxp->fd)
					abort ();
			}
		}
#ifdef DDS_IPV6
	if (tcpv6_server)
		for (ccxp = tcpv6_server->clients; ccxp; ccxp = ccxp->next) {
			if (ccxp == ccxp->next || ccxp->parent != tcpv6_server)
				abort ();
			if (!cxp->fd)
				abort ();

			for (cxp = ccxp->clients; cxp; cxp = cxp->next) {
				if (cxp == cxp->next || cxp->parent != ccxp)
					abort ();
				if (!cxp->fd)
					abort ();
			}
		}
#endif
	for (i = 0; i < TCP_MAX_CLIENTS; i++) {
		ccxp = tcp_client [i];
		if (!ccxp)
			continue;

		for (cxp = ccxp->clients; cxp; cxp = cxp->next) {
			if (cxp == cxp->next || cxp->parent != ccxp)
				abort ();
			if (!cxp->fd)
				abort ();
		}
	}
}

#define	VALIDATE_CXS()	tcp_cxs_validate()
#else
#define	VALIDATE_CXS()
#endif

static void tcp_cx_closed (IP_CX *cxp)
{
	if (cxp->fd_owner)
		log_printf (RTPS_ID, 0, "TCP(*:%u): [%d] connection closed - %p.\r\n", cxp->handle, cxp->fd, (void *) cxp);
	if ((cxp->fd_owner || cxp->cx_state == CXS_CONREQ) && 
	    cxp->stream_fcts &&
	    cxp->stream_fcts->disconnect)
		cxp->stream_fcts->disconnect (cxp);

	TCP_NCX_STATE ("*", cxp, CXS_CLOSED);
	TCP_NP_STATE ("*", cxp, TCS_IDLE);
}

static void tcp_free_data (IP_CX *cxp)
{
	if (cxp->data && cxp->data_length) {
		xfree (cxp->data);
		cxp->data = NULL;
		cxp->data_length = 0;
	}
}

static void tcp_close_data (IP_CX *cxp)
{
	IP_CX	*p, *prev, *paired;

	VALIDATE_CXS ();

	/*log_printf (RTPS_ID, 0, "tcp_close_data(%p:%d);\r\n", (void *) cxp, cxp->handle);*/
	/* Unpair connections if paired. */
	if ((paired = cxp->paired) != NULL) {
		paired->paired = cxp->paired = NULL;
		if (cxp->fd > 0 && cxp->fd == paired->fd) {
			if (cxp->sproto && !paired->sproto) {
				/*log_printf (RTPS_ID, 0, "tcp_close_data(): move sproto of [%d] from %p(%d) to %p(%d).\r\n", cxp->fd, (void *) cxp, cxp->handle, (void *) paired, paired->handle);*/
				paired->sproto = cxp->sproto;
				cxp->sproto = NULL;
			}
			if (cxp->fd_owner) {
				/*log_printf (RTPS_ID, 0, "tcp_close_data(): move owner of [%d] from %p(%d) to %p(%d).\r\n", cxp->fd, (void *) cxp, cxp->handle, (void *) paired, paired->handle);*/
				paired->fd_owner = 1;
				sock_fd_udata (cxp->fd, paired);
				cxp->fd_owner = 0;
			}
			/*if (cxp->stream_cb && !paired->stream_cb) {
				log_printf (RTPS_ID, 0, "tcp_close_data(): move stream_cb of [%d] from %p(%d) to %p(%d).\r\n", cxp->fd, (void *) cxp, cxp->handle, (void *) paired, paired->handle);
				paired->stream_cb = cxp->stream_cb;
				cxp->stream_cb = NULL;
			}
			if (cxp->stream_fcts && !paired->stream_fcts) {
				log_printf (RTPS_ID, 0, "tcp_close_data(): move stream_fcts of [%d] from %p(%d) to %p(%d).\r\n", cxp->fd, (void *) cxp, cxp->handle, (void *) paired, paired->handle);
				paired->stream_fcts = cxp->stream_fcts;
				cxp->stream_fcts = NULL;
				}*/			
		}
	}

	/* Close the file descriptor and adjust the state. */
	tcp_cx_closed (cxp);

	VALIDATE_CXS ();

	/* Remove timer if still running. */
	if (cxp->timer) {
		log_printf (RTPS_ID, 0, "TCP(%u): Stop timer (%u).\r\n", cxp->handle, cxp->dst_port);
		tmr_stop (cxp->timer);
		tmr_free (cxp->timer);
		cxp->timer = NULL;
	}

	/* Free all queued data. */
	if (cxp->head) {
		rtps_unref_messages (cxp->head);
		cxp->head = NULL;
	}

	/* Free cookie if still present. */
	tcp_free_data (cxp);

	/* Cleanup all child contexts. */
	while (cxp->clients)
		tcp_close_data (cxp->clients);

	VALIDATE_CXS ();

	/* Remove context from parent list. */
	if (cxp->parent)
		for (prev = NULL, p = cxp->parent->clients; 
		     p;
		     prev = p, p = p->next)
			if (p == cxp) {
				if (prev)
					prev->next = p->next;
				else
					cxp->parent->clients = p->next;
				break;
			}

	VALIDATE_CXS ();

	/* Remove context handle if it was set. */
	if (cxp->handle) {
		rtps_ip_free_handle (cxp->handle);
		if (cxp->locator)
			cxp->locator->locator.handle = 0;

		/* Don't use the handle anymore. */
		cxp->handle = 0;
	}

	VALIDATE_CXS ();

	/* Free the locator. */
	if (cxp->locator) {
		if (!cxp->locator->users)
			xfree (cxp->locator);
		else
			locator_unref (cxp->locator);
		cxp->locator = NULL;
	}

	/* Free the context. */
	rtps_ip_free (cxp);

	VALIDATE_CXS ();
}

static void tcp_close_fd (IP_CX *cxp)
{
	IP_CX	*cxp_fd, *cxp_nfd;

	if (cxp->paired && cxp->paired->fd == cxp->fd) {
		if (cxp->fd_owner) {
			cxp_fd = cxp;
			cxp_nfd = cxp->paired;
		}
		else {
			cxp_fd = cxp->paired;
			cxp_nfd = cxp;
		}
		tcp_close_data (cxp_nfd);
		tcp_close_data (cxp_fd);
	}
	else
		tcp_close_data (cxp);
}

static WR_RC tcp_send_msg (IP_CX *cxp)
{
	RMBUF		*mp;
	RME		*mep;
	RMREF		*next_mrp;
	unsigned char	*txp;
	size_t		max_size;
	unsigned	ofs;
	unsigned	n;
	WR_RC		rc;

	/* Get the first message from the queue. */
	mp = cxp->head->message;
	next_mrp = cxp->head->next;

	/* Set txp either to a newly allocated buffer for TLS, since it expects
	   to reuse the buffer data in some situations.  TLS will xfree() the
	   buffer itself when done. */
#ifdef DDS_SECURITY
	if (cxp->stream_fcts == &tls_functions) {

		/* Find out length of transmit message. */
		n = sizeof (mp->header) + sizeof (uint32_t);
		for (mep = mp->first; mep; mep = mep->next) {
			if ((mep->flags & RME_HEADER) != 0)
				n += sizeof (mep->header);
			n += mep->length;
		}

		/* Allocate a large enough buffer. */
		txp = xmalloc (n);
		if (!txp) {
			warn_printf ("tcp_send_ctrl: out-of-memory for send buffer!");
			rc = WRITE_ERROR;
			cxp->stats.nomem++;
			goto done;
		}
		max_size = n;
	}
	else {
#endif
		txp = rtps_tx_buf;
		max_size = MAX_TX_SIZE;
#ifdef DDS_SECURITY
	}
#endif

	/* Copy message to the actual transmit buffer, chunk by chunk. */
	memcpy (txp, &mp->header, sizeof (mp->header));
	ofs = sizeof (mp->header) + sizeof (uint32_t);
	for (mep = mp->first; mep; mep = mep->next) {
		if ((mep->flags & RME_HEADER) != 0) {
			n = sizeof (mep->header);
			if (ofs + n > max_size)
				break;

			memcpy (txp + ofs, &mep->header, n);
			ofs += n;
		}
		if ((n = mep->length) != 0) {
			if (ofs + n > max_size)
				break;

			if (mep->data != mep->d && mep->db)
				db_get_data (txp + ofs, mep->db, mep->data, 0, n - mep->pad);
			else
				memcpy (txp + ofs, mep->data, n - mep->pad);
			ofs += n;
		}
	}
	if (mep) {
		log_printf (RTPS_ID, 0, "tcp_send_msg: packet too long (> %lu)\r\n", (unsigned long) max_size);
		cxp->stats.nomem++;
		rc = WRITE_ERROR;
		goto done;
	}
	n = ofs - sizeof (mp->header) - sizeof (uint32_t);
	*((uint32_t *) (txp + sizeof (mp->header))) = n;

	trc_print1 ("D:T[%d:", cxp->fd);
	rc = cxp->stream_fcts->write_msg (cxp, txp, ofs);
	if (rc == WRITE_FATAL) {
		trc_print ("] WRITE_FATAL\r\n");
		log_printf (RTPS_ID, 0, "tcp_send_msg: fatal error sending RTPS message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));
		return (rc);
	}
	else if (rc == WRITE_ERROR) {
		trc_print ("] WRITE_ERROR\r\n");
		log_printf (RTPS_ID, 0, "tcp_send_msg: error sending RTPS message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));
		return (rc);
	}
	else if (rc == WRITE_BUSY) {
		trc_print ("] WRITE_BUSY\r\n");
		log_printf (RTPS_ID, 0, "tcp_send_msg: Write still busy from previous message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));
		return (rc);
	}
	else if (rc == WRITE_PENDING)
		log_printf (RTPS_ID, 0, "tcp_send_msg: Write pending of RTPS message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));

	trc_print1 ("RTPS:%d bytes]\r\n", ofs);
	ADD_ULLONG (cxp->stats.octets_sent, ofs);
	cxp->stats.packets_sent++;

    done:
	rtps_unref_message (cxp->head);
	cxp->head = next_mrp;
	if (next_mrp == NULL) /* Update tail if list becomes empty. */
		cxp->tail = NULL;
	cxp->stats.nqueued--;
	return (rc);
}

static void tcp_send_queued (IP_CX *cxp)
{
	WR_RC	rc;

	while (cxp->head) {
		rc = tcp_send_msg (cxp);
		if (rc != WRITE_OK)
			break;
	}
}

#ifdef TCP_FWD_SPDP

static void tcp_enqueue_msgs (IP_CX *cxp, RMBUF *msgs);

static void tcp_send_meta (unsigned id, IP_CX *ccxp, int orig_fd, RMBUF *msg)
{
	IP_CX	*cxp;

	ARG_NOT_USED (msg);

	/* Find a data connection suitable for meta communication. */
	for (cxp = ccxp->clients; cxp; cxp = cxp->next)
		if (cxp->id == id &&
		    (cxp->locator->locator.flags & (LOCF_META | LOCF_MCAST)) ==
						   (LOCF_META | LOCF_MCAST) &&
		    cxp->tx_data)
			break;

	if (cxp && cxp->fd != orig_fd)
		tcp_enqueue_msgs (cxp, msg);
}

static void tcp_forward_meta_mcast (IP_CX *cxp, RMBUF *msg)
{
	int		orig_fd = cxp->fd;
	unsigned	i;
	IP_CX		*ccxp;

	for (i = 0; i < TCP_MAX_CLIENTS; i++) {
		ccxp = tcp_client [i];
		if (!ccxp)
			break;

		if (ccxp->locator->locator.kind != cxp->locator->locator.kind)
			break;

		tcp_send_meta (cxp->id, ccxp, orig_fd, msg);
	}
	if (cxp->locator->locator.kind == LOCATOR_KIND_TCPv4 && tcpv4_server) {
		for (ccxp = tcpv4_server->clients; ccxp; ccxp = ccxp->next)
			tcp_send_meta (cxp->id, ccxp, orig_fd, msg);
	}
#ifdef DDS_IPV6
	if (cxp->locator->locator.kind == LOCATOR_KIND_TCPv6 && tcpv6_server) {
		for (ccxp = tcpv6_server->clients; ccxp; ccxp = ccxp->next)
			tcp_send_meta (cxp->id, ccxp, orig_fd, msg);
	}
#endif
}

#endif

/* tcp_rx_buffer -- An RTPS message was received over a TCP connection. */

static void tcp_rx_buffer (IP_CX *cxp,
			   IP_CX *paired_cxp,
			   unsigned char *buffer,
			   size_t length)
{
	IP_CX	*tx_cxp;
	RMBUF	*mp;

	if (paired_cxp && paired_cxp->tx_data)
		tx_cxp = paired_cxp;
	else if (!paired_cxp && cxp->parent) {
		for (tx_cxp = cxp->parent->clients; tx_cxp; tx_cxp = tx_cxp->next)
			if (tx_cxp->id == cxp->id &&
			    tx_cxp->tx_data &&
			    tx_cxp != cxp  &&
			    (tx_cxp->locator->locator.flags == cxp->locator->locator.flags)) {
				tx_cxp->paired = cxp;
				cxp->paired = tx_cxp;
				break;
			}
	}
	else
		tx_cxp = NULL;

	if (tx_cxp) {
		mp = rtps_parse_buffer (tx_cxp, buffer, length);
		if (mp) {
#ifdef TCP_FWD_SPDP
			if (rtps_forward &&
			    (cxp->locator->locator.flags & (LOCF_META | LOCF_MCAST)) ==
							   (LOCF_META | LOCF_MCAST))
				tcp_forward_meta_mcast (tx_cxp, mp);
#endif
			rtps_rx_msg (tx_cxp, mp, tx_cxp->dst_addr, tx_cxp->dst_port);
		}
	}
	else {
		/*log_printf (RTPS_ID, 0, "(TCP-RTPS-NoCx:%u)", cxp->locator->locator.port);*/
		rtps_rx_buffer (cxp, buffer, length, NULL, 0);
	}
}


/**********************************************************************/
/*   Client TxData FSM logic.					      */
/**********************************************************************/

static void cdr_cxbind_succ (IP_CX *cxp);
static void cdr_cxbind_fail (IP_CX *cxp);
static void cdr_finalize (IP_CX *cxp);

typedef enum {
	CDTE_TDATA,
	CDTE_CCREADY,
	CDTE_CCFAIL,
	CDTE_TO,
	CDTE_SLPSUCC,
	CDTE_SLPFAIL,
	CDTE_CXOK,
	CDTE_CXERR,
	CDTE_CXBSUCC,
	CDTE_CXBFAIL,
	CDTE_FINALIZE
} CDT_EVENT;

#define	CDT_NSTATES	((unsigned) TDS_DATA + 1)
#define	CDT_NEVENTS	((unsigned) CDTE_FINALIZE + 1)

typedef void (*CDTFCT) (IP_CX *cxp);

static void cdt_timeout (uintptr_t user);

static void cdt_i_d (IP_CX *cxp)
{
	if (cxp->parent && cxp->parent->p_state == TCS_CONTROL) {
		TCP_NP_STATE ("CDT", cxp, TDS_WPORTOK);
		cxp->retries = SLPREQ_RETRIES;
		tcp_send_slport_request (cxp, cdt_timeout, SLPREQ_TO, 1);
	}
	else
		TCP_NP_STATE ("CDT", cxp, TDS_WCONTROL);
}

#define cdt_d_data tcp_send_queued

static void cdt_ready (IP_CX *cxp)
{
	TCP_NP_STATE ("CDT", cxp, TDS_WPORTOK);
	cxp->retries = SLPREQ_RETRIES;
	tcp_send_slport_request (cxp, cdt_timeout, SLPREQ_TO, 1);
}

#define	cdt_wco_ctrl	tcp_close_fd
#define cdt_wco_to	tcp_close_fd
#define cdt_wp_ctrl	tcp_close_fd

static void cdt_wp_to (IP_CX *cxp)
{
	if (cxp->retries > 0) {
		cxp->retries--;
		tcp_send_slport_request (cxp, cdt_timeout, SLPREQ_TO, 1);
	}
	else
		tcp_close_data (cxp);
}

static void cdt_connect (IP_CX *cxp);

#if 0

static void cdt_retry (IP_CX *cxp, int delayed)
{
	if (!cxp->timer) {
		cxp->timer = tmr_alloc ("TCP-CDataTx");
		if (!cxp->timer) {
			warn_printf ("TCP: not enough memory for TCP TxData timer!");
			return;
		}
		if (delayed)
			tmr_start (cxp->timer, TICKS_PER_SEC * 2, 
					(uintptr_t) cxp, cdt_timeout);
	}
	if (delayed) {
		TCP_NCX_STATE ("CDT", cxp, CXS_WRETRY);
		TCP_NP_STATE ("CDT", cxp, TDS_WCXOK);
	}
	else
		cdt_connect (cxp);
}

#endif

static void cdt_connected (IP_CX *cxp)
{
	DDS_ReturnCode_t	r;

	log_printf (RTPS_ID, 0, "TCP(CDT:%u): data connection (%u) established on [%d].\r\n", cxp->handle, cxp->dst_port, cxp->fd);
	TCP_NCX_STATE ("CDT", cxp, CXS_OPEN);
	r = tcp_send_cx_bind_request (cxp, cdt_timeout, CXBREQ_TO);
	if (r == DDS_RETCODE_ALREADY_DELETED)
		return;

	cxp->retries = CXBREQ_RETRIES;
	TCP_NP_STATE ("CDT", cxp, TDS_WCBINDOK);
}

static int cdt_control (IP_CX *cxp, unsigned char *msg, size_t size);

static void continue_write (IP_CX *cxp)
{
	WR_RC		rc;

	/* If an enqueued CxBindSuccess control message is pending, send it
	   first, before any of the RTPS messages.  This happens very frequently
	   in SDR contexts and often leads to very long (5s) delays if not
	   handled properly, i.e. right away, like this. */
	if (cxp->cxbs_queued) {
		trc_print1 ("C:T[%d:", cxp->fd);
		rc = cxp->stream_fcts->write_msg (cxp, cxp->data, cxp->data_length);
		cxp->cxbs_queued = 0;
#ifdef DDS_SECURITY
		if (cxp->stream_fcts != &tls_functions)
#endif
			if (cxp->data)
				xfree (cxp->data);
		cxp->data = NULL;
		if (rc == WRITE_FATAL) {
			trc_print ("] WRITE_FATAL\r\n");
			log_printf (RTPS_ID, 0, "tcp_send_ctrl2: fatal error sending control message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));
			tcp_close_fd (cxp);
			return;
		}
		else if (rc == WRITE_ERROR) {
			trc_print ("] WRITE_ERROR\r\n");
			log_printf (RTPS_ID, 0, "tcp_send_ctrl2: error sending control message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));
			cxp->stats.write_err++;
			return;
		}
		else if (rc == WRITE_BUSY) {
			trc_print ("] WRITE_BUSY\r\n");
			log_printf (RTPS_ID, 0, "tcp_send_ctrl2: Write still busy from previous  message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));
			cxp->data_length = 0;
			return;
		}
		else if (rc == WRITE_PENDING)
			log_printf (RTPS_ID, 0, "tcp_send_ctrl2: Write pending of control message [%d] {rc=%d} (%s).\r\n", cxp->fd, rc, strerror (ERRNO));

		ADD_ULLONG (cxp->stats.octets_sent, cxp->data_length);
		cxp->data_length = 0;
		cxp->stats.packets_sent++;
		if (rc == WRITE_PENDING)
			return;
		/* else
			fall through to send enqueued data messages! */
	}

	/* We can now send enqueued RTPS messages while TCP accepts them. */
	if (cxp->head)
		tcp_send_queued (cxp);
	else
		cxp->tail = NULL;
}

static void cdt_connect (IP_CX *cxp)
{
	int	r;

	TCP_NCX_STATE ("CDT", cxp, CXS_CONNECT);
	TCP_NP_STATE ("CDT", cxp, TDS_WCXOK);
	r = cxp->stream_fcts->connect (cxp, cxp->parent->dst_port);
	if (r < 0) {
		tmr_start (cxp->timer, TICKS_PER_SEC * 2, (uintptr_t) cxp, cdt_timeout);
		if (r == -1) {
			log_printf (RTPS_ID, 0, "TCP(CDT:%u): connecting to server ... \r\n", cxp->handle);
			return;
		}
		log_printf (RTPS_ID, 0, "TCP(CDT:%u): connect() failed!\r\n", cxp->handle);
		TCP_NCX_STATE ("CDT", cxp, CXS_WRETRY);
		return;
	}
	cdt_connected (cxp);
}

static void cdt_closed (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "TCP(CDT:%u): connection shutdown!\r\n", cxp->handle);
	tcp_close_fd (cxp);
}

static IP_CX *reverse_cx (IP_CX *cxp)
{
	IP_CX	*rxp = NULL;
#if TCP_SHARE
	IP_CX	*xp;

	if (cxp->paired)
		return (cxp->paired);

	for (xp = cxp->parent->clients; xp; xp = xp->next) {
		if (xp != cxp &&
		    xp->share &&
		    (xp->locator->locator.flags & LOCF_MODE) == (cxp->locator->locator.flags & LOCF_MODE) &&
		    xp->id == cxp->id) {
			if (!xp->paired &&
			    xp->cx_state > CXS_CONNECT &&
			    !!xp->tx_data == !cxp->tx_data &&
			    xp->p_state > TDS_WPORTOK) {
				if (rxp) {
#ifdef TCP_TRC_CX
					rtps_ip_dump_cx (rxp, 1);
					rtps_ip_dump_cx (xp, 1);
#endif
					fatal_printf ("Multiple reverse possible: found: %d and %d", rxp->handle, xp->handle);
				}
				else
					rxp = xp;
			}
		}
		VALIDATE_CXS ();
	}
#else
	ARG_NOT_USED (cxp)
#endif
	return (rxp);
}

static void cdt_wp_succ (IP_CX *cxp)
{
	CtrlInfo_t	 *info = (CtrlInfo_t *) cxp->data;
	IP_CX		 *rcxp;
	/*unsigned	 port, handle;*/
	DDS_ReturnCode_t r;
	
	static STREAM_CB	cdt_control_cb = {
		NULL,			/* on_new_connection */
		cdt_connected,		/* on_connected */
		continue_write,		/* on_write_completed */
		cdt_control,		/* on_new_message */
		tcp_close_fd		/* on_close */
	};

	cxp->stream_cb = &cdt_control_cb;

	cxp->data = info->cookie;
	cxp->data_length = info->cookie_length;
	info->cookie = NULL;
	rcxp = (cxp->share) ? reverse_cx (cxp) : NULL;
	if (!rcxp)
		cdt_connect (cxp);
	else {
		log_printf (RTPS_ID, 0, "TCP(CDT:%u): data connection (%u) shared on %p(%u) [%u]\r\n", cxp->handle, cxp->dst_port, (void *) rcxp, rcxp->handle, rcxp->fd);
		cxp->paired = rcxp;
		rcxp->paired = cxp;
		cxp->fd = rcxp->fd;
		TCP_NCX_STATE ("CDT", cxp, CXS_OPEN);
		/*handle = cxp->handle;
		port = cxp->dst_port;*/
		r = tcp_send_cx_bind_request (cxp, cdt_timeout, CXBREQ_TO);
		if (r == DDS_RETCODE_ALREADY_DELETED)
			return;

		cxp->retries = CXBREQ_RETRIES;
		TCP_NP_STATE ("CDT", cxp, TDS_WCBINDOK);
	}
	if ((info->pids & (1 << (CAID_GUID_PREFIX & 0xff))) != 0) {
		cxp->dst_prefix = info->prefix;
		cxp->has_prefix = 1;
	}
}

#define cdt_wp_fail	tcp_close_data

#define cdt_wc_ctrl	tcp_close_fd
#define cdt_wc_to	tcp_close_fd
#define cdt_wc_ok	cdt_connected
#define cdt_wc_cerr	tcp_close_fd

#define cdt_wb_ctrl	tcp_close_fd

static void cdt_wb_to (IP_CX *cxp)
{
	DDS_ReturnCode_t	r;

	if (cxp->retries > 0) {
		cxp->retries--;
		r = tcp_send_cx_bind_request (cxp, cdt_timeout, CXBREQ_TO);
		if (r == DDS_RETCODE_ALREADY_DELETED)
			return;
	}
	else {
		r = tcp_send_finalize (cxp);
		if (r != DDS_RETCODE_ALREADY_DELETED)
			cdt_wc_to (cxp);
	}
}

#define cdt_wb_cerr	tcp_close_fd
#define cdt_wb_fail	tcp_close_data
#define cdt_wb_fin	tcp_close_data

static void cdt_wb_succ (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "TCP(CDT:%u): data connection (%u) ready.\r\n", cxp->handle, cxp->dst_port);
	TCP_NP_STATE ("CDT", cxp, TDS_DATA);
	log_printf (RTPS_ID, 0, "TCP(%u): Stop timer (%u).\r\n", cxp->handle, cxp->dst_port);
	tmr_stop (cxp->timer);
	tmr_free (cxp->timer);
	tcp_free_data (cxp);
	cxp->timer = NULL;
	if (cxp->head)
		tcp_send_queued (cxp);
}

#define cdt_d_ctrl	tcp_close_fd
#define cdt_d_cerr	tcp_close_fd
#define	cdt_d_fin	tcp_close_data

static CDTFCT cdt_fsm [CDT_NEVENTS][CDT_NSTATES] = {
	      /*IDLE     WCONTROL     WPORTOK     WCXOK       WCBINDOK    DATA*/
/*TDATA   */  {	cdt_i_d, NULL,        NULL,       NULL,       NULL,       cdt_d_data },
/*CCREADY */  {	NULL,	 cdt_ready,   NULL,       NULL,       NULL,       NULL       },
/*CCFAIL  */  {	NULL,	 cdt_wco_ctrl,cdt_wp_ctrl,cdt_wc_ctrl,cdt_wb_ctrl,cdt_d_ctrl },
/*TO      */  {	NULL,    cdt_wco_to,  cdt_wp_to,  cdt_wc_to,  cdt_wb_to,  NULL       },
/*SLPSUCC */  {	NULL,    NULL,        cdt_wp_succ,NULL,       NULL,       NULL       },
/*SLPFAIL */  {	NULL,    NULL,        cdt_wp_fail,NULL,       NULL,       NULL       },
/*CXOK    */  {	NULL,    NULL,        NULL,       cdt_wc_ok,  NULL,       NULL       },
/*CXERR   */  {	NULL,    NULL,        NULL,       cdt_wc_cerr,cdt_wb_cerr,cdt_d_cerr },
/*CXBSUCC */  {	NULL,    NULL,        NULL,       NULL,       cdt_wb_succ,NULL       },
/*CXBFAIL */  {	NULL,    NULL,        NULL,       NULL,       cdt_wb_fail,NULL       },
/*FINALIZE*/  {	NULL,    NULL,        NULL,       NULL,       cdt_wb_fin, cdt_d_fin  }
};

static void cdt_control_ready (IP_CX *cxp)
{
	CDTFCT		fct;

	fct = cdt_fsm [CDTE_CCREADY][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void cdt_data_queued (IP_CX *cxp)
{
	CDTFCT		fct;

	fct = cdt_fsm [CDTE_TDATA][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void cdt_spsucc (IP_CX *cxp)
{
	CDTFCT		fct;

	fct = cdt_fsm [CDTE_SLPSUCC][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void cdt_spfail (IP_CX *cxp)
{
	CDTFCT		fct;

	fct = cdt_fsm [CDTE_SLPFAIL][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void cdt_timeout (uintptr_t user)
{
	IP_CX		*cxp = (IP_CX *) user;
	CDTFCT		fct;

	log_printf (RTPS_ID, 0, "TCP(CDT:%u): timeout (%u)!\r\n", cxp->handle, cxp->dst_port);
	fct = cdt_fsm [CDTE_TO][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void cdt_cxbind_succ (IP_CX *cxp)
{
	CDTFCT		fct;

	fct = cdt_fsm [CDTE_CXBSUCC][cxp->p_state];
	if (fct)
		(*fct) (cxp);
	else
		log_printf (RTPS_ID, 0, "TCP(CDT:%u): unexpected CxBindSuccess on (%u)!\r\n", cxp->handle, cxp->dst_port);
}

static void cdt_cxbind_fail (IP_CX *cxp)
{
	CDTFCT		fct;

	fct = cdt_fsm [CDTE_CXBFAIL][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void cdt_finalize (IP_CX *cxp)
{
	CDTFCT		fct;

	fct = cdt_fsm [CDTE_FINALIZE][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static int cdt_control (IP_CX *cxp, unsigned char *msg, size_t size)
{
	uint32_t	*wp;
	CtrlInfo_t	info;
	int		n;
	unsigned	i;
	uint32_t	tid;

	n = tcp_rx_parse_msg (cxp->fd, &info, msg, size);
	if (n < 0 || info.result > RPSC_ERR_RTPS) {
		log_printf (RTPS_ID, 0, "TCP(CDT:%u): message error (%u)!\r\n", cxp->handle, info.result);
		info_cleanup (&info);
		cdt_closed (cxp);
		return (0);
	}

	/* Check message type. */
	if (info.result == RPSC_ERR_RTPS && cxp->paired) {
		if (cxp->paired->p_state < TDS_DATA)
			return (1);	/* RTPS messages only allowed after successful bind! */

		ADD_ULLONG (cxp->paired->stats.octets_rcvd, size);
		cxp->paired->stats.packets_rcvd++;

		for (i = 0, wp = (uint32_t *) (msg + 20); i < 5; i++, wp--)
			*wp = *(wp - 1);
		tcp_rx_buffer (cxp->paired, cxp, msg + 4, size - 4);
		return (1);
	}
	ADD_ULLONG (cxp->stats.octets_rcvd, size);
	cxp->stats.packets_rcvd++;

	memcpy (&tid, info.transaction + 8, 4);
	cxp->data = &info;
	cxp->data_length = 0;
	switch (info.type) {
		case CMT_CX_BIND_SUCC:
			/*log_printf (RTPS_ID, 0, "TCP(CDT): ConnectionBindSuccess received.\r\n");*/
			if (tid == cxp->transaction_id)
				cdt_cxbind_succ (cxp);
			else if (cxp->paired && tid == cxp->paired->transaction_id)
				cdr_cxbind_succ (cxp->paired);
			else
				log_printf (RTPS_ID, 0, "TCP(CDT:%u): unexpected CxBindSuccess on (%u)?\r\n", cxp->handle, cxp->dst_port);
			break;
		case CMT_CX_BIND_FAIL:
			/*log_printf (RTPS_ID, 0, "TCP(CDT): ConnectionBindFail received.\r\n");*/
			if (tid == cxp->transaction_id)
				cdt_cxbind_fail (cxp);
			else if (cxp->paired && tid == cxp->paired->transaction_id)
				cdr_cxbind_fail (cxp->paired);
			break;
		case CMT_FINALIZE:
			/*log_printf (RTPS_ID, 0, "TCP(CDT): Finalize received.\r\n");*/
			if (cxp->paired)
				cdr_finalize (cxp->paired);
			else
				cdt_finalize (cxp);
			info_cleanup (&info);
                        cxp->data = NULL;
                        cxp->data_length = 0;
			return (0);

		default:
			log_printf (RTPS_ID, 0, "TCP(CDT:%u): Unexpected message (%u) received!\r\n", cxp->handle, info.type);
	  		break;
	}
	info_cleanup (&info);
        cxp->data = NULL;
        cxp->data_length = 0;
	return (1);
}


/**********************************************************************/
/*   Client RxData FSM logic.					      */
/**********************************************************************/

typedef enum {
	CDRE_CCFAIL,
	CDRE_TO,
	CDRE_CXOK,
	CDRE_CXERR,
	CDRE_CXBSUCC,
	CDRE_CXBFAIL,
	CDRE_FINALIZE
} CDR_EVENT;

#define	CDR_NSTATES	((unsigned) TDS_DATA + 1)
#define	CDR_NEVENTS	((unsigned) CDRE_FINALIZE + 1)

typedef void (*CDRFCT) (IP_CX *cxp);

static void cdr_timeout (uintptr_t user);

#define cdr_x_ctrl	tcp_close_fd
#define cdr_cx_to	tcp_close_fd

static void cdr_cb_to (IP_CX *cxp)
{
	DDS_ReturnCode_t	r;

	if (cxp->retries > 0) {
		cxp->retries--;
		r = tcp_send_cx_bind_request (cxp, cdr_timeout, CXBREQ_TO);
	}
	else {
		r = tcp_send_finalize (cxp);
		if (r != DDS_RETCODE_ALREADY_DELETED) {
			tcp_free_data (cxp);
			cdr_cx_to (cxp);
		}
	}
}

static void cdr_connected (IP_CX *cxp)
{
	DDS_ReturnCode_t	r;

	log_printf (RTPS_ID, 0, "TCP(CDR:%u): data connection (%u) established on [%d].\r\n", cxp->handle, cxp->group->locator->locator.port, cxp->fd);
	TCP_NCX_STATE ("CDR", cxp, CXS_OPEN);
	r = tcp_send_cx_bind_request (cxp, cdr_timeout, CXBREQ_TO);
	if (r == DDS_RETCODE_ALREADY_DELETED)
		return;

	cxp->retries = CXBREQ_RETRIES;
	TCP_NP_STATE ("CDR", cxp, TDS_WCBINDOK);
	if (cxp->paired)
		cxp->paired->fd = cxp->fd;
}

static void cdr_closed (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "TCP(CDR:%u): connection shutdown!\r\n", cxp->handle);
	tcp_close_fd (cxp);
}

#define cdr_cx_cxok	cdr_connected
#define cdr_x_cerr	tcp_close_fd

static void cdr_cb_succ (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "TCP(CDR:%u): data connection (%u) ready.\r\n", cxp->handle, cxp->group->locator->locator.port);
	TCP_NP_STATE ("CDR", cxp, TDS_DATA);
	log_printf (RTPS_ID, 0, "TCP(%u): Stop timer (%u).\r\n", cxp->handle, cxp->dst_port);
	tmr_stop (cxp->timer);
	tmr_free (cxp->timer);
	cxp->timer = NULL;
	tcp_free_data (cxp);
	cxp->rx_data = 1;
}

#define cdr_cb_fail	tcp_close_data
#define cdr_cb_fin	tcp_close_data
#define cdr_d_fin	tcp_close_data

static CDTFCT cdr_fsm [CDR_NEVENTS][CDR_NSTATES] = {
	      /*IDLE    WCONTROL WPORTOK  WCXOK        WCBINDOK     DATA*/
/*CCFAIL  */  {	NULL,   NULL,    NULL,    cdr_x_ctrl,  cdr_x_ctrl,  cdr_x_ctrl },
/*TO      */  {	NULL,   NULL,    NULL,    cdr_cx_to,   cdr_cb_to,   NULL       },
/*CXOK    */  {	NULL,   NULL,    NULL,    cdr_cx_cxok, NULL,        NULL       },
/*CXERR   */  {	NULL,   NULL,    NULL,    cdr_x_cerr,  cdr_x_cerr,  cdr_x_cerr },
/*CXBSUCC */  { NULL,   NULL,    NULL,    NULL,        cdr_cb_succ, NULL       },
/*CXBFAIL */  { NULL,   NULL,    NULL,    NULL,        cdr_cb_fail, NULL       },
/*FINALIZE*/  {	NULL,   NULL,    NULL,    NULL,        cdr_cb_fin,  cdr_d_fin  }
};

static int cdr_control (IP_CX *cxp, unsigned char *msg, size_t size);

static void cdr_connect (IP_CX *cxp)
{
	static STREAM_CB	cdr_control_cb = {
		NULL,			/* on_new_connection */
		cdr_connected,		/* on_connected */
		NULL,			/* on_write_completed */
		cdr_control,		/* on_new_message */
		tcp_close_fd		/* on_close */
	};
	int	r;

	TCP_NCX_STATE ("CDR", cxp, CXS_CONNECT);
	TCP_NP_STATE ("CDR", cxp, TDS_WCXOK);
	cxp->stream_cb = &cdr_control_cb;
	r = cxp->stream_fcts->connect (cxp, cxp->parent->dst_port);
	if (r < 0) {
		tmr_start (cxp->timer, TICKS_PER_SEC * 2, (uintptr_t) cxp, cdr_timeout);
		if (r == -1) {
			log_printf (RTPS_ID, 0, "TCP(CDR:%u): connecting to server ... \r\n", cxp->handle);
			return;
		}
		log_printf (RTPS_ID, 0, "TCP(CDR:%u): connect() failed!\r\n", cxp->handle);
		TCP_NCX_STATE ("CDR", cxp, CXS_WRETRY);
		return;
	}
	cdr_connected (cxp);
}

static void cdr_port_req (IP_CX *ccxp, CtrlInfo_t *info)
{
	IP_CX			*cxp, *gcxp, *rcxp;
	GuidPrefix_t		*sprefix;
	DDS_ReturnCode_t	ret;

	/* Try to find existing context with the requested port. */
	for (cxp = ccxp->clients; cxp; cxp = cxp->next)
		if (cxp->locator->locator.port == info->port && cxp->group) {
			tcp_send_clport_fail (ccxp, info, CERR_ALLOC_MISMATCH);
			return;
		}

	/* Not found: create a new context. */
	gcxp = rtps_ip_lookup_port (~0, ccxp->locator->locator.kind, info->port);
	if (!gcxp) {
		tcp_send_clport_fail (ccxp, info, CERR_ALLOC_MISMATCH);
		return;
	}
	cxp = rtps_ip_alloc ();
	if (!cxp) {
		warn_printf ("TCP(CDR): not enough memory for TCP connection!");
		goto port_rej;
	}
	cxp->locator = xmalloc (sizeof (LocatorNode_t));
	if (!cxp->locator) {
		warn_printf ("TCP(CDR): not enough memory for TCP locator!");
		goto free_cx;
	}
	cxp->locator->users = 0;
	cxp->locator->locator = ccxp->locator->locator;
	cxp->locator->locator.port = info->port;
	cxp->locator->locator.flags = gcxp->locator->locator.flags | ccxp->locator->locator.flags;
	cxp->stream_fcts = ccxp->stream_fcts; /* Inherit transport */
	cxp->cx_type = ccxp->cx_type;
	cxp->cx_side = ICS_CLIENT;
	cxp->cx_mode = ICM_DATA;
	cxp->cx_state = CXS_CLOSED;
	cxp->p_state = TDS_IDLE;
	cxp->share = TCP_SHARE;
	memcpy (cxp->dst_addr, ccxp->dst_addr, 16);
	cxp->dst_port = ccxp->dst_port;
	cxp->has_dst_addr = 1;
	cxp->associated = 1;
	cxp->data = info->cookie;
	cxp->data_length = info->cookie_length;
	info->cookie = NULL;
	cxp->parent = ccxp;
	cxp->next = ccxp->clients;
	ccxp->clients = cxp;
	cxp->group = gcxp;
	cxp->id = gcxp->id;
	rtps_ip_new_handle (cxp);
	cxp->locator->locator.handle = cxp->handle;

	rcxp = ((info->port_options & PO_SHARE) != 0) ? reverse_cx (cxp) : NULL;
	if (rcxp) {
		cxp->paired = rcxp;
		rcxp->paired = cxp;
	}
	sprefix = gcxp->has_prefix ? &gcxp->dst_prefix : NULL;
	if (rcxp && rcxp->fd) {
		log_printf (RTPS_ID, 0, "TCP(CDR:%u): data connection (%u) shared on %p(%u) [%u]\r\n", cxp->handle, cxp->group->locator->locator.port, (void *) rcxp, rcxp->handle, rcxp->fd);

		TCP_NCX_STATE ("CDR", cxp, CXS_OPEN);
		cxp->fd = rcxp->fd;
		ret = tcp_send_clport_success (ccxp, info, 1, sprefix);
		if (ret != DDS_RETCODE_ALREADY_DELETED) {
			ret = tcp_send_cx_bind_request (cxp, cdr_timeout, CXBREQ_TO);
			if (ret == DDS_RETCODE_ALREADY_DELETED)
				return;

			cxp->retries = CXBREQ_RETRIES;
			TCP_NP_STATE ("CDR", cxp, TDS_WCBINDOK);
		}
	}
	else {
		TCP_NCX_STATE ("CDR", cxp, CXS_CLOSED);

		cxp->timer = tmr_alloc ();
		if (!cxp->timer) {
			warn_printf ("TCP: not enough memory for TCP RxData timer!");
			ccxp->clients = cxp->next;
			goto free_loc;
		}
		tmr_init (cxp->timer, "TCP-RxData");
		ret = tcp_send_clport_success (ccxp, info, 1, sprefix);
		if (ret != DDS_RETCODE_ALREADY_DELETED)
			cdr_connect (cxp);
	}
	return;

    free_loc:
	xfree (cxp->locator);
    free_cx:
	rtps_ip_free (cxp);
    port_rej:
	tcp_send_clport_fail (ccxp, info, CERR_OO_RESOURCES);
}

static void cdr_timeout (uintptr_t user)
{
	IP_CX		*cxp = (IP_CX *) user;
	CDRFCT		fct;

	log_printf (RTPS_ID, 0, "TCP(CDR:%u): timeout on (%u)!\r\n", cxp->handle, cxp->group->locator->locator.port);
	fct = cdr_fsm [CDRE_TO][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void cdr_cxbind_succ (IP_CX *cxp)
{
	CDTFCT		fct;

	fct = cdr_fsm [CDRE_CXBSUCC][cxp->p_state];
	if (fct)
		(*fct) (cxp);
	/*else
		dbg_printf ("TCP(CDR): unexpected CxBindSuccess on (%u)!\r\n", cxp->group->locator->locator.port);*/
}

static void cdr_cxbind_fail (IP_CX *cxp)
{
	CDTFCT		fct;

	fct = cdr_fsm [CDRE_CXBFAIL][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void cdr_finalize (IP_CX *cxp)
{
	CDTFCT		fct;

	fct = cdr_fsm [CDRE_FINALIZE][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static int cdr_control (IP_CX *cxp, unsigned char *msg, size_t size)
{
	uint32_t	*wp;
	CtrlInfo_t	info;
	int		n;
	unsigned	i;
	uint32_t	tid;

	n = tcp_rx_parse_msg (cxp->fd, &info, msg, size);
	if (n < 0 || info.result > RPSC_ERR_RTPS) {
		log_printf (RTPS_ID, 0, "TCP(CDR:%u): message error(%u)!\r\n", cxp->handle, info.result);
		info_cleanup (&info);
		cdr_closed (cxp);
		return (0);
	}

	/* Check message type. */
	ADD_ULLONG (cxp->stats.octets_rcvd, size);
	cxp->stats.packets_rcvd++;
	if (info.result == RPSC_ERR_RTPS) {
		if (cxp->p_state < TDS_DATA)
			return (1); /* RTPS messages only allowed after successful bind! */

		for (i = 0, wp = (uint32_t *) (msg + 20); i < 5; i++, wp--)
			*wp = *(wp - 1);
		tcp_rx_buffer (cxp, cxp->paired, msg + 4, size - 4);
		ADD_ULLONG (cxp->group->stats.octets_rcvd, (unsigned) (size - 4));
		cxp->group->stats.packets_rcvd++;
		return (1);
	}
	memcpy (&tid, info.transaction + 8, 4);
	cxp->data = &info;
	cxp->data_length = 0;
	switch (info.type) {
		case CMT_CX_BIND_SUCC:
			/*log_printf (RTPS_ID, 0, "TCP(CDR): ConnectionBindSuccess received.\r\n");*/
			if (tid == cxp->transaction_id)
				cdr_cxbind_succ (cxp);
			else if (cxp->paired && tid == cxp->paired->transaction_id)
				cdt_cxbind_succ (cxp->paired);
			else
				log_printf (RTPS_ID, 0, "TCP(CDR:%u): unexpected CxBindSuccess on (%u)?\r\n", cxp->handle, cxp->group->locator->locator.port);
			break;

		case CMT_CX_BIND_FAIL:
			/*log_printf (RTPS_ID, 0, "TCP(CDR): ConnectionBindFail received.\r\n");*/
			if (tid == cxp->transaction_id)
				cdr_cxbind_fail (cxp);
			else if (cxp->paired && tid == cxp->paired->transaction_id)
				cdt_cxbind_fail (cxp->paired);
			info_cleanup (&info);
                        cxp->data = NULL;
                        cxp->data_length = 0;
			return (0);

		case CMT_FINALIZE:
			/*log_printf (RTPS_ID, 0, "TCP(CDR): Finalize received.\r\n");*/
			if (cxp->paired)
				cdr_finalize (cxp->paired);
			else
				cdr_finalize (cxp);
			info_cleanup (&info);
                        cxp->data = NULL;
                        cxp->data_length = 0;
			return (0);

		default:
			log_printf (RTPS_ID, 0, "TCP(CDR:%u): Unexpected message (%u) received!\r\n", cxp->handle, info.type);
	  		break;
	}
	info_cleanup (&info);
        cxp->data = NULL;
        cxp->data_length = 0;
	return (1);
}


/**********************************************************************/
/*   Client Control FSM logic.					      */
/**********************************************************************/

static void cc_closed (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "TCP(CC:%u): connection closed!\r\n", cxp->handle);
	tcp_cx_closed (cxp);
}

static void cc_req_port_all (IP_CX *cxp)
{
	IP_CX		*ccxp;
	
	for (ccxp = cxp->clients; ccxp; ccxp = ccxp->next)
		if (ccxp->p_state == TDS_WCONTROL)
			cdt_control_ready (ccxp);
}

static void cc_req_purge (IP_CX *cxp)
{
	IP_CX	*ccxp, *prev_ccxp, *next_cxp;

	for (ccxp = cxp->parent->clients, prev_ccxp = NULL;
	     ccxp;
	     ccxp = next_cxp) {
		next_cxp = ccxp->next;
		if (ccxp == cxp) {
			if (prev_ccxp)
				prev_ccxp->next = ccxp->next;
			else
				cxp->parent->clients = ccxp->next;
			tcp_close_data (ccxp);
		}
		else
			prev_ccxp = ccxp;
	}
}

static void cc_req_purge_all (IP_CX *cxp)
{
	while (cxp->clients)
		cc_req_purge (cxp->clients);
}

static void cc_connect (IP_CX *cxp);

static void cc_timeout (uintptr_t user);

static void cc_retry (IP_CX *cxp, int delayed)
{
	if (!cxp->timer) {
		cxp->timer = tmr_alloc ();
		if (!cxp->timer) {
			warn_printf ("TCP: not enough memory for TCP Client timer!");
			return;
		}
		tmr_init (cxp->timer, "TCP-Client");
		if (delayed)
			tmr_start (cxp->timer, TICKS_PER_SEC * 2, 
					(uintptr_t) cxp, cc_timeout);
	}
	if (delayed) {
		TCP_NCX_STATE ("CC", cxp, CXS_WRETRY);
		TCP_NP_STATE ("CC", cxp, TCS_WCXOK);
	}
	else
		cc_connect (cxp);
}

static void cc_connected (IP_CX *cxp)
{
	DDS_ReturnCode_t	ret;
	unsigned		id = cxp->id;

	log_printf (RTPS_ID, 0, "TCP(CC:%u): control connection established.\r\n", cxp->handle);
	TCP_NCX_STATE ("CC", cxp, CXS_OPEN);
	cxp->retries = CC_WID_RETRIES;
	ret = tcp_send_id_bind_request (cxp, cc_timeout, CC_WID_TO);
	if (!ret)
		TCP_NP_STATE ("CC", cxp, TCS_WIBINDOK);
	else if (ret == DDS_RETCODE_ALREADY_DELETED)
		cc_restart (id);
}

static void cc_connect (IP_CX *cxp)
{
	int	r;

	TCP_NCX_STATE ("CC", cxp, CXS_CONNECT);
	TCP_NP_STATE ("CC", cxp, TCS_WCXOK);
	r = cxp->stream_fcts->connect (cxp, cxp->dst_port);
	if (r < 0) {
		tmr_start (cxp->timer, TICKS_PER_SEC * 2, (uintptr_t) cxp, cc_timeout);
		if (r == -1) {
			log_printf (RTPS_ID, 0, "TCP(CC:%u): connecting to server ... \r\n", cxp->handle);
			return;
		}
		log_printf (RTPS_ID, 0, "TCP(CC:%u): connect() failed!\r\n", cxp->handle);
		TCP_NCX_STATE ("CC", cxp, CXS_WRETRY);
		return;
	}
	cc_connected (cxp);
}

static void cc_reconnect (IP_CX *cxp, int delay)
{
	cc_closed (cxp);
	cc_retry (cxp, delay);
}

typedef enum {
	CCE_START,
	CCE_STOP,
	CCE_CXERR,
	CCE_TO,
	CCE_FINALIZE,
	CCE_IDBFAIL,
	CCE_IDBSUCC,
	CCE_CLPREQ,
	CCE_SLPSUCC,
	CCE_SLPFAIL
} CC_EVENT;

#define	CC_NSTATES	((unsigned) TCS_CONTROL + 1)
#define	CC_NEVENTS	((unsigned) CCE_SLPFAIL + 1)

typedef void (*CCFCT) (IP_CX *cxp);

static void cc_i_start (IP_CX *cxp)
{
	cc_connect (cxp);
}

static void cc_x_stop (IP_CX *cxp)
{
	tcp_close_fd (cxp);
}

static void cc_wc_stop (IP_CX *cxp)
{
	tmr_stop (cxp->timer);
	cc_x_stop (cxp);
}

static void cc_x_cxerr (IP_CX *cxp)
{
	int	delay;

	delay = cxp->p_state != TCS_CONTROL;
	cc_reconnect (cxp, delay);
}

#define cc_wc_cxerr cc_x_cxerr

static void cc_wc_to (IP_CX *cxp)
{
	if (cxp->cx_state == CXS_WRETRY)
		cc_connect (cxp);
	else
		cc_reconnect (cxp, 0);
}

#define cc_wi_stop	cc_x_stop
#define cc_wi_cxerr	cc_x_cxerr

static void cc_wi_to (IP_CX *cxp)
{
	if (cxp->retries > 0) {
		cxp->retries--;
		tcp_send_id_bind_request (cxp, cc_timeout, IDBREQ_TO);
	}
	else {
		tcp_send_finalize (cxp);
		TCP_NP_STATE ("CC", cxp, TCS_WCXOK);
		cc_wc_to (cxp);
	}
}

static void cc_x_final (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "TCP(CC): control connection ready!\r\n");
	log_printf (RTPS_ID, 0, "TCP(CC): Stop timer.\r\n");
	tmr_stop (cxp->timer);
	cc_req_purge_all (cxp);
	cc_reconnect (cxp, 1);
}

#define cc_wi_final cc_x_final

static void cc_wi_ifail (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "TCP(CC): Identity rejected by server.\r\n");
	cc_reconnect (cxp, 1);
}

static void cc_wi_isucc (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "TCP(CC): control connection ready!\r\n");
	log_printf (RTPS_ID, 0, "TCP(CC): Stop timer.\r\n");
	tmr_stop (cxp->timer);
	tmr_free (cxp->timer);
	cxp->timer = NULL;
	TCP_NP_STATE ("CC", cxp, TCS_CONTROL);
	cc_req_port_all (cxp);
}

#define cc_a_stop	cc_x_stop
#define cc_a_cxerr	cc_x_cxerr

static void cc_a_final (IP_CX *cxp)
{
	if (tcp_send_finalize (cxp) != DDS_RETCODE_ALREADY_DELETED)
		cc_x_final (cxp);
}

static void cc_a_cprq (IP_CX *cxp)
{
	CtrlInfo_t	*info = cxp->data;

	cdr_port_req (cxp, info);
}

static void cc_a_spsucc (IP_CX *cxp)
{
	IP_CX		*ccxp;
	CtrlInfo_t	*info = cxp->data;
	uint32_t	tid;

	log_printf (RTPS_ID, 0, "TCP(CC): data connection allowed!\r\n");
	memcpy (&tid, info->transaction + 8, 4);
	for (ccxp = cxp->clients; ccxp; ccxp = ccxp->next)
		if (tid == ccxp->transaction_id) {
			ccxp->data = info;
			ccxp->data_length = 0;
			cdt_spsucc (ccxp);
			break;
		}
}

static void cc_a_spfail (IP_CX *cxp)
{
	IP_CX		*ccxp, *next = NULL;
	CtrlInfo_t	*info = cxp->data;
	uint32_t	tid;

	log_printf (RTPS_ID, 0, "TCP(CC): data connection not allowed!\r\n");
	memcpy (&tid, info->transaction + 8, 4);
	for (ccxp = cxp->clients; ccxp; ccxp = next) {
		next = ccxp->next;
		if (tid == ccxp->transaction_id)
			cdt_spfail (ccxp);
	}
}

static CCFCT cc_fsm [CC_NEVENTS][CC_NSTATES] = {
		/*IDLE      WCXOK        WIBINDOK     CONTROL*/
/*START   */  {	cc_i_start, NULL,        NULL,        NULL        },
/*STOP    */  {	NULL,	    cc_wc_stop,  cc_wi_stop,  cc_a_stop   },
/*CXERR   */  {	NULL,	    cc_wc_cxerr, cc_wi_cxerr, cc_a_cxerr  },
/*TO      */  {	NULL,	    cc_wc_to,    cc_wi_to,    NULL        },
/*FINALIZE*/  {	NULL,	    NULL,        cc_wi_final, cc_a_final  },
/*IDBFAIL */  {	NULL,	    NULL,        cc_wi_ifail, NULL        },
/*IDBSUCC */  {	NULL,	    NULL,        cc_wi_isucc, NULL        },
/*CLPREQ  */  {	NULL,	    NULL,        NULL,        cc_a_cprq   },
/*SLPSUCC */  {	NULL,	    NULL,        NULL,        cc_a_spsucc },
/*SLPFAIL */  {	NULL,	    NULL,        NULL,        cc_a_spfail }
};

static void cc_timeout (uintptr_t user)
{
	IP_CX		*cxp = (IP_CX *) user;
	CCFCT           fct;

	log_printf (RTPS_ID, 0, "TCP(CC): timeout!\r\n");
	fct = cc_fsm [CCE_TO][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void cc_on_close (IP_CX *cxp)
{
	CCFCT fct = cc_fsm [CCE_CXERR][cxp->p_state];

	if (fct)
		fct (cxp);

	VALIDATE_CXS ();
}

static void cc_start (IP_CX *cxp)
{
	cc_fsm [CCE_START][TCS_IDLE] (cxp);
}

static int cc_control (IP_CX *cxp, unsigned char *msg, size_t size)
{
	uint32_t	*wp;
	CtrlInfo_t	info;
	int		n;
	unsigned	i;
	CC_EVENT	e;
	CCFCT		fct;
	uint32_t	tid;

	n = tcp_rx_parse_msg (cxp->fd, &info, msg, size);
	if (n < 0 || info.result > RPSC_ERR_RTPS) {
		log_printf (RTPS_ID, 0, "TCP(CC): message error (%u)!\r\n", info.result);
		info_cleanup (&info);
		cc_reconnect (cxp, 1);
		return (0);
	}

	/* Check message type. */
	ADD_ULLONG (cxp->stats.octets_rcvd, size);
	cxp->stats.packets_rcvd++;
	if (info.result == RPSC_ERR_RTPS) { /* Should not happen! */
		if (cxp->p_state < TCS_CONTROL)
			return (1);	/* RTPS messages only allowed after successful bind! */

		for (i = 0, wp = (uint32_t *) (msg + 20); i < 5; i++, wp--)
			*wp = *(wp - 1);
		tcp_rx_buffer (cxp, NULL, msg + 4, size - 4);
		return (1);
	}
	memcpy (&tid, info.transaction + 8, 4);
	cxp->data = &info;
	cxp->data_length = 0;
	switch (info.type) {
		case CMT_ID_BIND_SUCC:
			/*log_printf (RTPS_ID, 0, "TCP(CC): IdentityBindSuccess received.\r\n");*/
			if (tid == cxp->transaction_id) {
				cxp->dst_forward = info.forward;
				cxp->dst_prefix = info.prefix;
				cxp->has_prefix = 1;
				fct = cc_fsm [CCE_IDBSUCC][cxp->p_state];
				if (fct)
					(*fct) (cxp);
			}
			else
				log_printf (RTPS_ID, 0, "TCP(CC): ignoring IdentityBindSuccess (wrong id).\r\n");
			break;
		case CMT_ID_BIND_FAIL:
			/*log_printf (RTPS_ID, 0, "TCP(CC): IdentityBindFail received.\r\n");*/
			if (tid == cxp->transaction_id) {
				fct = cc_fsm [CCE_IDBFAIL][cxp->p_state];
				if (fct) {
					(*fct) (cxp);
					info_cleanup (&info);
                                        cxp->data = NULL;
                                        cxp->data_length = 0;
					return (0);
				}
			}
			else
				log_printf (RTPS_ID, 0, "TCP(CC): ignoring IdentityBindFail (wrong id).\r\n");
			break;
		case CMT_SLPORT_SUCC:
		case CMT_SLPORT_FAIL:
			/*if (info.type == CMT_SLPORT_SUCC)
				log_printf (RTPS_ID, 0, "TCP(CC): ServerLogicalPortSuccess received.\r\n");
			else
				log_printf (RTPS_ID, 0, "TCP(CC): ServerLogicalPortFail received.\r\n");*/

			if (cxp->p_state == TCS_CONTROL) {
				cxp->transaction_id = tid;
				if (info.type == CMT_SLPORT_SUCC)
					e = CCE_SLPSUCC;
				else
					e = CCE_SLPFAIL;
				fct = cc_fsm [e][TCS_CONTROL];
				if (fct)
					(*fct) (cxp);
			}
			break;
		case CMT_CLPORT_REQ:
			/*log_printf (RTPS_ID, 0, "TCP(CC): ClientLogicalPortRequest received.\r\n");*/
			fct = cc_fsm [CCE_CLPREQ][cxp->p_state];
			if (fct)
				(*fct) (cxp);
			break;
		case CMT_FINALIZE:
			/*log_printf (RTPS_ID, 0, "TCP(CC): Finalize received.\r\n");*/
			fct = cc_fsm [CCE_FINALIZE][cxp->p_state];
			if (fct)
				(*fct) (cxp);
			info_cleanup (&info);
                        cxp->data = NULL;
                        cxp->data_length = 0;
			return (0);
		default:
			log_printf (RTPS_ID, 0, "TCP(CC): Unexpected message (%u) received!\r\n", info.type);
	  		break;
	}
	info_cleanup (&info);
        cxp->data = NULL;
        cxp->data_length = 0;
	return (1);
}

static void rtps_tcp_client_start (RTPS_TCP_RSERV *sp, int index, unsigned delay)
{
	static STREAM_CB	control_channel_cb = {
		NULL,			/* on_new_connection (NA) */
		cc_connected,		/* on_connected */
		NULL,			/* on_write_completed (NA) */
		cc_control,		/* on_new_message */
		cc_on_close		/* on_close */
	};
	IP_CX			*cxp;
	struct hostent		*he;
	struct in_addr		addr;

	log_printf (RTPS_ID, 0, "TCP: Client %d start.\r\n", index);
	tcp_client [index] = cxp = rtps_ip_alloc ();
	if (!cxp) {
		warn_printf ("TCP: not enough memory for TCP Client!");
		return;
	}
	cxp->id = index;

#ifdef DDS_SECURITY
	if (sp->secure) {
#ifdef TCP_SIMULATE_TLS
		cxp->stream_fcts = &tcp_functions;
#else
		cxp->stream_fcts = &tls_functions;
#endif
		cxp->cx_type = CXT_TCP_TLS;
	}
	else {
#endif
		cxp->stream_fcts = &tcp_functions;
		cxp->cx_type = CXT_TCP;
#ifdef DDS_SECURITY
	}
#endif
	cxp->stream_cb = &control_channel_cb;
	cxp->cx_side = ICS_CLIENT;
	cxp->cx_mode = ICM_CONTROL;
	cxp->cx_state = CXS_CLOSED;
	cxp->locator = xmalloc (sizeof (LocatorNode_t));
	if (!cxp->locator) {
		warn_printf ("TCP: not enough memory for TCP Client locator!");
		goto free_cx;
	}
	cxp->locator->users = 0;
	memset (&cxp->locator->locator, 0, sizeof (Locator_t));
#ifdef DDS_IPV6
	if (sp->ipv6)
		cxp->locator->locator.kind = LOCATOR_KIND_TCPv6;
	else
#endif
		cxp->locator->locator.kind = LOCATOR_KIND_TCPv4;
#ifdef DDS_SECURITY
	if (sp->secure) {
		cxp->locator->locator.flags |= LOCF_SECURE;
		cxp->locator->locator.sproto = SECC_TLS_TCP;
	}
#endif
	cxp->timer = tmr_alloc ();
	if (!cxp->timer) {
		warn_printf ("TCP: not enough memory for TCP Client timer!");
		goto free_loc;
	}
	tmr_init (cxp->timer, "TCP-Client");
	if (sp->name) {
		he = gethostbyname (sp->addr.name);
		if (!he) {
			warn_printf ("TCP: server name could not be resolved!");
			goto free_timer;
		}
		addr = *((struct in_addr *) he->h_addr_list [0]);
		log_printf (RTPS_ID, 0, "TCP: server name resolved to %s\r\n", 
						          inet_ntoa (addr));
	}
	else
		addr.s_addr = htonl (sp->addr.ipa_v4);
	cxp->cx_state = CXS_CLOSED;
	cxp->associated = 1;
#ifdef DDS_IPV6
	if (!sp->ipv6) {
#endif
		cxp->dst_addr [12] = ntohl (addr.s_addr) >> 24;
		cxp->dst_addr [13] = (ntohl (addr.s_addr) >> 16) & 0xff;
		cxp->dst_addr [14] = (ntohl (addr.s_addr) >> 8) & 0xff;
		cxp->dst_addr [15] = ntohl (addr.s_addr) & 0xff;
#ifdef DDS_IPV6
	}
	else
		memcpy (cxp->dst_addr, sp->addr.ipa_v6, 16);
#endif
	cxp->dst_port = sp->port;
	cxp->has_dst_addr = 1;
	if (delay)
		tmr_start (cxp->timer, delay, (uintptr_t) cxp, cc_timeout);
	else
		cc_start (cxp);
	VALIDATE_CXS ();
	return;

    free_timer:
    	tmr_free (cxp->timer);
    free_loc:
	xfree (cxp->locator);
    free_cx:
	rtps_ip_free (cxp);
	tcp_client [index] = NULL;
}

static void rtps_tcp_client_stop (int index)
{
	IP_CX		*cxp;

	log_printf (RTPS_ID, 0, "TCP: Client %d stop.\r\n", index);
	cxp = tcp_client [index];
	if (!cxp)
		return;

	cc_fsm [CCE_STOP][cxp->p_state] (cxp);
	tcp_client [index] = NULL;
	VALIDATE_CXS ();
}

static void cc_restart (unsigned id)
{
	unsigned	delay;

	/* Try to connect again to the remote server with a randomized delay. */
	delay = TICKS_PER_SEC + fastrandn (TICKS_PER_SEC * 4);
	rtps_tcp_client_start (&tcp_v4_pars.rservers [id], id, delay);
}


/**********************************************************************/
/*   TCP Server Receive Data FSM.				      */
/**********************************************************************/

static IP_CX *tcp_server_data_add (IP_CX *ccxp, CtrlInfo_t *info);
static void sdt_finalize (IP_CX *cxp);

typedef enum {
	SDRE_SLPREQ,
	SDRE_SCFAIL,
	SDRE_TO,
	SDRE_CXBIND,
	SDRE_CXERR,
	SDRE_FINALIZE
} SDR_EVENT;

#define	SDR_NSTATES	((unsigned) TDS_DATA + 1)
#define	SDR_NEVENTS	((unsigned) SDRE_FINALIZE + 1)

typedef void (*SDRFCT) (IP_CX *cxp);

static DDS_ReturnCode_t sdr_send_error;

static void sdr_timeout (uintptr_t user);

static void sdr_i_preq (IP_CX *cxp)
{
	CtrlInfo_t	*info;
	GuidPrefix_t	*prefix;

	info = (CtrlInfo_t *) cxp->data;
	if ((info->pids & (1 << (CAID_GUID_PREFIX & 0xff))) != 0)
		prefix = &info->prefix;
	else
		prefix = NULL;
	TCP_NP_STATE ("SDR", cxp, TDS_WCBINDOK);
	cxp->timer = tmr_alloc ();
	if (!cxp->timer) {
		warn_printf ("TCP: not enough memory for TCP Server RxData timer!");
		return;
	}
	tmr_init (cxp->timer, "TCP-SRxData");
	tmr_start (cxp->timer, TICKS_PER_SEC * 2, (uintptr_t) cxp, sdr_timeout);
	sdr_send_error = tcp_send_slport_success (cxp->parent, cxp->data,
				 (unsigned char *) &cxp->label, sizeof (cxp->label),
				 prefix);
}

#define sdr_wb_scerr	tcp_close_fd
#define sdr_wb_to	tcp_close_data

static void sdr_wb_bind (IP_CX *cxp)
{
	if (cxp->timer) {
		log_printf (RTPS_ID, 0, "TCP(SDR:%u): Stop timer.\r\n", cxp->handle);
		tmr_stop (cxp->timer);
		tmr_free (cxp->timer);
		cxp->timer = NULL;
	}
	TCP_NP_STATE ("SDR", cxp, TDS_DATA);
	cxp->rx_data = 1;
	cxp->share = TCP_SHARE;
	log_printf (RTPS_ID, 0, "TCP(SDR:%u): data connection (%u) ready!\r\n", cxp->handle, cxp->group->locator->locator.port);
}

static void sdr_d_scerr (IP_CX *cxp)
{
	sdr_send_error = tcp_send_finalize (cxp);
	if (sdr_send_error != DDS_RETCODE_ALREADY_DELETED)
		tcp_close_fd (cxp);
}

#define sdr_d_cxerr tcp_close_fd

static void sdr_d_final (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "TCP(SDR:%u): Finalize received!\r\n", cxp->handle);
	cxp->p_state = TDS_IDLE;
}

static SDRFCT sdr_fsm [SDR_NEVENTS][SDR_NSTATES] = {
	      /*IDLE        WCONTROL WPORTOK  WCXOK    WCBINDOK      DATA*/
/*SLPREQ  */  {	sdr_i_preq, NULL,    NULL,    NULL,    NULL,         NULL        },
/*SCFAIL  */  { NULL,       NULL,    NULL,    NULL,    sdr_wb_scerr, sdr_d_scerr },
/*TO      */  {	NULL,       NULL,    NULL,    NULL,    sdr_wb_to,    NULL        },
/*CXBIND  */  { NULL,       NULL,    NULL,    NULL,    sdr_wb_bind,  NULL        },
/*CXERR   */  { NULL,       NULL,    NULL,    NULL,    NULL,         sdr_d_cxerr },
/*FINALIZE*/  { NULL,       NULL,    NULL,    NULL,    NULL,         sdr_d_final }
};

static void sdr_finalize (IP_CX *cxp)
{
	SDRFCT	fct;

	fct = sdr_fsm [SDRE_FINALIZE][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static int sdr_control (IP_CX *cxp, unsigned char *msg, size_t size)
{
	uint32_t	*wp;
	CtrlInfo_t	info;
	int		n;
	unsigned	i;
	IP_CX		*dcxp;

	n = tcp_rx_parse_msg (cxp->fd, &info, msg, size);
	if (n < 0 || info.result > RPSC_ERR_RTPS) {
		log_printf (RTPS_ID, 0, "TCP(SDR:%u): message error (%u)!\r\n", cxp->handle, info.result);
		info_cleanup (&info);
		tcp_close_fd (cxp);
		return (0);
	}

	/* Check message type. */
	ADD_ULLONG (cxp->stats.octets_rcvd, (unsigned) size);
	cxp->stats.packets_rcvd++;
	if (info.result == RPSC_ERR_RTPS) {
		if (cxp->p_state < TDS_DATA)
			return (1); /* RTPS messages only allowed after successful bind! */

		for (i = 0, wp = (uint32_t *) (msg + 20); i < 5; i++, wp--)
			*wp = *(wp - 1);
		tcp_rx_buffer (cxp, cxp->paired, msg + 4, size - 4);
		/*ADD_ULLONG (cxp->group->stats.octets_rcvd, (unsigned) (size - 4));
		cxp->group->stats.packets_rcvd++;*/
		return (1);
	}
	cxp->data = &info;
	cxp->data_length = 0;
	if (info.type == CMT_FINALIZE) {
		/*log_printf (RTPS_ID, 0, "TCP(SDR): Finalize received.\r\n");*/
                cxp->data = NULL;
		if (cxp->paired)
			sdt_finalize (cxp);
		else
			sdr_finalize (cxp);
		info_cleanup (&info);
		return (0);
	}
	else if (info.type == CMT_CX_BIND_REQ) {
		/*log_printf (RTPS_ID, 0, "TCP(SDR): ConnectionBindRequest received.\r\n");*/
		dcxp = tcp_server_data_add (cxp, &info);
		if (dcxp) {
			ADD_ULLONG (dcxp->stats.octets_rcvd, (unsigned) size);
			dcxp->stats.packets_rcvd++;
		}
	}
	else
		log_printf (RTPS_ID, 0, "TCP(SDR:%u): Unexpected message (%u) received!\r\n", cxp->handle, info.type);
	info_cleanup (&info);
        cxp->data = NULL;
        cxp->data_length = 0;
	return (1);
}

static void sdr_cbind_ok (IP_CX *cxp)
{
	static STREAM_CB	sdr_control_cb = {
		NULL,			/* on_new_connection */
		NULL,			/* on_connected */
		NULL,			/* on_write_completed */
		sdr_control,		/* on_new_message */
		tcp_close_fd		/* on_close */
	};

	SDRFCT		fct;

	cxp->stream_cb = &sdr_control_cb;

	fct = sdr_fsm [SDRE_CXBIND][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static DDS_ReturnCode_t sdr_start (IP_CX *server_cxp, CtrlInfo_t *info)
{
	IP_CX		 *cxp, *gcxp, *rcxp;
	SDRFCT		 fct;
	GuidPrefix_t	 *sprefix;
	DDS_ReturnCode_t ret;

	/* Check if group port exists. */
	gcxp = rtps_ip_lookup_port (~0, server_cxp->locator->locator.kind, info->port);
	if (!gcxp) {
		ret = tcp_send_slport_fail (server_cxp, info, CERR_ALLOC_MISMATCH);
		return (ret);
	}
	sprefix = gcxp->has_prefix ? &gcxp->dst_prefix : NULL;

#if TCP_SHARE
	/* Try to find existing context with the requested port. */
	for (cxp = server_cxp->clients; cxp; cxp = cxp->next)
		if (cxp->locator->locator.port == info->port) {
			log_printf (RTPS_ID, 0, "TCP(SDR:%u): Existing connection (%u)!\r\n", cxp->handle, info->port);
			if (cxp->paired)
				info->shared = 1;
			ret = tcp_send_slport_success (server_cxp, info,
					(unsigned char *) &cxp, sizeof (cxp),
					sprefix);
			return (ret);
		}
#endif

	log_printf (RTPS_ID, 0, "TCP(SDR): new connection!\r\n");

	/* Exists: create new connection. */
	cxp = rtps_ip_alloc ();
	if (!cxp) {
		warn_printf ("TCP(SDR): not enough memory for context!");
		goto port_rej;
	}
	cxp->locator = xmalloc (sizeof (LocatorNode_t));
	if (!cxp->locator) {
		warn_printf ("TCP(SDR): not enough memory for locator!");
		goto free_cx;
	}
	cxp->locator->users = 0;
	cxp->locator->locator = server_cxp->locator->locator;
	cxp->locator->locator.port = info->port;
	cxp->stream_fcts = server_cxp->stream_fcts; /* Inherit transport */
	cxp->cx_type = server_cxp->cx_type;
	cxp->cx_side = ICS_SERVER;
	cxp->cx_mode = ICM_DATA;
	cxp->cx_state = CXS_CLOSED;
	memcpy (cxp->dst_addr, server_cxp->dst_addr, 16);
	cxp->dst_port = server_cxp->dst_port;
	cxp->has_dst_addr = 1;
	cxp->associated = 1;
	cxp->parent = server_cxp;
	cxp->next = server_cxp->clients;
	cxp->group = gcxp;
	cxp->id = gcxp->id;
	cxp->locator->locator.sproto = gcxp->locator->locator.sproto;
	cxp->locator->locator.flags = gcxp->locator->locator.flags |
				      server_cxp->locator->locator.flags;

	rtps_ip_new_handle (cxp);
	cxp->locator->locator.handle = cxp->handle;

	server_cxp->clients = cxp;
	rcxp = ((info->port_options & PO_SHARE) != 0) ? reverse_cx (cxp) : NULL;
	if (rcxp) {
		cxp->paired = rcxp;
		rcxp->paired = cxp;
	}
	info->shared = TCP_SHARE;
	cxp->label = tcp_cookie++;
	if (rcxp && rcxp->fd) {
		TCP_NCX_STATE ("SDR", cxp, CXS_OPEN);
		TCP_NP_STATE ("SDR", cxp, TDS_WCBINDOK);
		cxp->rx_data = 1;
		cxp->fd = rcxp->fd;
		ret = tcp_send_slport_success (server_cxp, info,
				 (unsigned char *) &cxp->label, sizeof (cxp->label),
				 sprefix);
		if (ret != DDS_RETCODE_ALREADY_DELETED)
			log_printf (RTPS_ID, 0, "TCP(SDR:%u): data connection (%u) shared on [%d]\r\n", cxp->handle, info->port, cxp->fd);
		else
			return (ret);
	}
	else {
		fct = sdr_fsm [SDRE_SLPREQ][TDS_IDLE];
		TCP_NCX_STATE ("SDR", cxp, CXS_CLOSED);
		TCP_NP_STATE ("SDR", cxp, TDS_WCBINDOK);
		cxp->data = info;
		cxp->data_length = 0;
		if (sprefix) {
			info->prefix = *sprefix;
			info->pids = SLPSUCC_PIDS;
		}
		if (fct)
			(*fct) (cxp);
		if (!sdr_send_error)
			log_printf (RTPS_ID, 0, "TCP(SDR:%u): data connection (%u) pending!\r\n", cxp->handle, info->port);
		else
			return (sdr_send_error);
	}
	VALIDATE_CXS ();
	return (DDS_RETCODE_OK);

    free_cx:
	rtps_ip_free (cxp);
    port_rej:
	ret = tcp_send_slport_fail (server_cxp, info, CERR_OO_RESOURCES);
	return (ret);
}

static void sdr_stop (IP_CX *cxp)
{
	SDRFCT		fct;

	fct = sdr_fsm [SDRE_SCFAIL][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void sdr_timeout (uintptr_t user)
{
	IP_CX		*cxp = (IP_CX *) user;
	SDRFCT		fct;

	log_printf (RTPS_ID, 0, "TCP(SDR:%u): timeout!\r\n", cxp->handle);
	fct = sdr_fsm [SDRE_TO][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

/**********************************************************************/
/*   TCP Server Transmit Data FSM.				      */
/**********************************************************************/

typedef enum {
	SDTE_TDATA,
	SDTE_SCFAIL,
	SDTE_TO,
	SDTE_CLPSUCC,
	SDTE_CLPFAIL,
	SDTE_CXBIND,
	SDTE_CXERR,
	SDTE_FINALIZE
} SDT_EVENT;

#define	SDT_NSTATES	((unsigned) TDS_DATA + 1)
#define	SDT_NEVENTS	((unsigned) SDTE_FINALIZE + 1)

typedef void (*SDTFCT) (IP_CX *cxp);

static void sdt_timeout (uintptr_t user);

static DDS_ReturnCode_t sdt_send_error;

static void sdt_i_data (IP_CX *cxp)
{
	IP_CX		*rcxp;

	TCP_NP_STATE ("SDT", cxp, TDS_WPORTOK);
	rcxp = (cxp->share) ? reverse_cx (cxp) : NULL;
	cxp->retries = 2;
	sdt_send_error = tcp_send_clport_request (cxp, sdt_timeout, CLPREQ_TO, 
						  &cxp->label, sizeof (tcp_cookie),
						  (rcxp) ? PO_SHARE : 0);
}

#define sdt_d_data tcp_send_queued

static void sdt_close_fd (IP_CX *cxp)
{
	tcp_close_fd (cxp);
	sdt_send_error = DDS_RETCODE_ALREADY_DELETED;
}

static void sdt_close_data (IP_CX *cxp)
{
	tcp_close_data (cxp);
	sdt_send_error = DDS_RETCODE_ALREADY_DELETED;
}

#define sdt_wp_scerr sdt_close_fd

static void sdt_wp_to (IP_CX *cxp)
{
	IP_CX		*rcxp;

	if (!--cxp->retries)
		sdt_close_data (cxp);
	else {
		log_printf (RTPS_ID, 0, "TCP(SDT:%u): Retry port request (%u)!\r\n", cxp->handle, cxp->dst_port);
		rcxp = (cxp->share) ? reverse_cx (cxp) : NULL;
		sdt_send_error = tcp_send_clport_request (cxp, sdt_timeout, CLPREQ_TO,
							  &cxp->label, sizeof (tcp_cookie),
							  (rcxp) ? PO_SHARE : 0);
	}
}

static void sdt_wp_succ (IP_CX *cxp)
{
	CtrlInfo_t	*info;
	IP_CX		*rcxp = NULL;

	info = (CtrlInfo_t *) cxp->data;
	if (cxp->data && cxp->share) {
		if (info->shared)
			rcxp = reverse_cx (cxp);
	}
	if ((info->pids & (1 << (CAID_GUID_PREFIX & 0xff))) != 0) {
		cxp->dst_prefix = info->prefix;
		cxp->has_prefix = 1;
	}
	if (rcxp) {
		cxp->paired = rcxp;
		rcxp->paired = cxp;
		cxp->fd = rcxp->fd;
		log_printf (RTPS_ID, 0, "TCP(SDT:%u): data connection (%u) shared on [%u]\r\n", cxp->handle, cxp->dst_port, cxp->fd);
	}
	else
		log_printf (RTPS_ID, 0, "TCP(SDT:%u): data connection (%u) pending!\r\n", cxp->handle, cxp->dst_port);

	TCP_NP_STATE ("SDT", cxp, TDS_WCBINDOK);
}

#define sdt_wp_fail	sdt_close_data

#define sdt_wb_scerr	sdt_close_fd
#define	sdt_wb_to	sdt_close_fd

static void sdt_wb_cbind (IP_CX *cxp)
{
	TCP_NP_STATE ("SDT", cxp, TDS_DATA);
	cxp->share = 1;
	log_printf (RTPS_ID, 0, "TCP(SDT:%u): Stop timer.\r\n", cxp->handle);
	tmr_stop (cxp->timer);
	tmr_free (cxp->timer);
	cxp->timer = NULL;
#ifdef TCP_TRC_CX
	log_printf (RTPS_ID, 0, "TCP(SDT:%u): sending queued data.\r\n", cxp->handle);
#endif
	tcp_send_queued (cxp);
}

static void sdt_wp_cbind (IP_CX *cxp)
{
	CtrlInfo_t	*info;
	IP_CX		*rcxp = NULL;

	if (cxp->data && cxp->share) {
		info = (CtrlInfo_t *) cxp->data;
		if (info->shared)
			rcxp = reverse_cx (cxp);
	}
	if (rcxp) {
		cxp->paired = rcxp;
		rcxp->paired = cxp;
		cxp->fd = rcxp->fd;
		log_printf (RTPS_ID, 0, "TCP(SDT:%u): data connection (%u) shared on [%u]\r\n", cxp->handle, cxp->dst_port, cxp->fd);
	}
	else
		log_printf (RTPS_ID, 0, "TCP(SDT:%u): data connection (%u) ready!\r\n", cxp->handle, cxp->dst_port);

	sdt_wb_cbind (cxp);
}

static void sdt_d_succ (IP_CX *cxp)
{
	CtrlInfo_t	*info;

	info = (CtrlInfo_t *) cxp->data;
	if ((info->pids & (1 << (CAID_GUID_PREFIX & 0xff))) != 0) {
		cxp->dst_prefix = info->prefix;
		cxp->has_prefix = 1;
	}
}

#define sdt_d_scerr	tcp_close_fd
#define sdt_d_cxerr	tcp_close_fd

static void sdt_d_final (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "TCP(SDT:%u): Finalize received!\r\n", cxp->handle);
	cxp->p_state = TDS_IDLE;
}

static SDTFCT sdt_fsm [SDT_NEVENTS][SDT_NSTATES] = {
	      /*IDLE        WCONTROL    WPORTOK      WCXOK    WCBINDOK      DATA*/
/*TDATA   */  {	sdt_i_data, NULL,       NULL,        NULL,    NULL,         sdt_d_data  },
/*SCFAIL  */  {	NULL,       NULL,       sdt_wp_scerr,NULL,    sdt_wb_scerr, sdt_d_scerr },
/*TO      */  {	NULL,       NULL,       sdt_wp_to,   NULL,    sdt_wb_to,    NULL        },
/*CLPSUCC */  {	NULL,       NULL,       sdt_wp_succ, NULL,    NULL,         sdt_d_succ  },
/*CLPFAIL */  {	NULL,       NULL,       sdt_wp_fail, NULL,    NULL,         NULL        },
/*CXBIND  */  {	NULL,       NULL,       sdt_wp_cbind,NULL,    sdt_wb_cbind, NULL        },
/*CXERR   */  {	NULL,       NULL,       NULL,        NULL,    NULL,         sdt_d_cxerr },
/*FINAL   */  {	NULL,       NULL,       NULL,        NULL,    NULL,         sdt_d_final }
};

static void sdt_timeout (uintptr_t user)
{
	IP_CX		*cxp = (IP_CX *) user;
	SDTFCT		fct;

	log_printf (RTPS_ID, 0, "TCP(SDT:%u): timeout!\r\n", cxp->handle);
	fct = sdt_fsm [SDTE_TO][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void sdt_data_queued (IP_CX *cxp)
{
	SDTFCT		fct;

	fct = sdt_fsm [SDTE_TDATA][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static IP_CX *lookup_transaction (IP_CX *cxp, CtrlInfo_t *info)
{
	IP_CX		*ccxp;
	uint32_t	tid;

	memcpy (&tid, info->transaction + 8, 4);
	for (ccxp = cxp->clients; ccxp; ccxp = ccxp->next)
		if (ccxp->transaction_id == tid)
			return (ccxp);

	return (NULL);
}

static void sdt_success (IP_CX *cxp, CtrlInfo_t *info)
{
	IP_CX		*ccxp;
	SDTFCT		fct;

	ccxp = lookup_transaction (cxp, info);
	if (!ccxp)
		return;

	fct = sdt_fsm [SDTE_CLPSUCC][ccxp->p_state];
	if (fct) {
		ccxp->data = info;
		ccxp->data_length = 0;
		(*fct) (ccxp);
	}
}

static void sdt_failed (IP_CX *cxp, CtrlInfo_t *info)
{
	IP_CX		*ccxp;
	SDTFCT		fct;

	ccxp = lookup_transaction (cxp, info);
	if (!ccxp)
		return;

	fct = sdt_fsm [SDTE_CLPFAIL][ccxp->p_state];
	if (fct) {
		ccxp->data = info;
		ccxp->data_length = 0;
		(*fct) (ccxp);
	}
}

static void sdt_finalize (IP_CX *cxp)
{
	SDTFCT	fct;

	fct = sdt_fsm [SDTE_FINALIZE][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static void sdt_stop (IP_CX *cxp)
{
	SDRFCT		fct;

	fct = sdt_fsm [SDTE_SCFAIL][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}

static int sdt_control (IP_CX *cxp, unsigned char *msg, size_t size)
{
	uint32_t	*wp;
	CtrlInfo_t	info;
	unsigned	i;
	int		n;
	IP_CX		*dcxp;

	n = tcp_rx_parse_msg (cxp->fd, &info, msg, size);
	if (n < 0 || info.result > RPSC_ERR_RTPS) {
		log_printf (RTPS_ID, 0, "TCP(SDT:%u): message error (%u)!\r\n", cxp->handle, info.result);
		info_cleanup (&info);
		tcp_close_fd (cxp);
		return (0);
	}

	/* Check message type. */
	if (info.result == RPSC_ERR_RTPS && cxp->paired) {
		if (cxp->paired->p_state < TDS_DATA)
			return (1); /* RTPS messages only allowed after successful bind! */

		ADD_ULLONG (cxp->paired->stats.octets_rcvd, (unsigned) size);
		cxp->paired->stats.packets_rcvd++;

		for (i = 0, wp = (uint32_t *) (msg + 20); i < 5; i++, wp--)
			*wp = *(wp - 1);
		tcp_rx_buffer (cxp->paired, cxp, msg + 4, size - 4);
		return (1);
	}
	ADD_ULLONG (cxp->stats.octets_rcvd, (unsigned) size);
	cxp->stats.packets_rcvd++;
	cxp->data = &info;
	cxp->data_length = 0;
	if (info.type == CMT_FINALIZE) {
		/*log_printf (RTPS_ID, 0, "TCP(CDT): Finalize received.\r\n");*/
                cxp->data = NULL;
		if (cxp->paired)
			sdr_finalize (cxp->paired);
		else
			sdt_finalize (cxp);
		info_cleanup (&info);
		return (0);
	}
	else if (info.type == CMT_CX_BIND_REQ) {
		log_printf (RTPS_ID, 0, "TCP(SD:%u): ConnectionBindRequest received.\r\n", cxp->handle);
		dcxp = tcp_server_data_add (cxp, &info);
		if (dcxp) {
			ADD_ULLONG (dcxp->stats.octets_rcvd, (unsigned) size);
			dcxp->stats.packets_rcvd++;
		}
	}
	else
		log_printf (RTPS_ID, 0, "TCP(SDT:%u): Unexpected message (%u) received!\r\n", cxp->handle, info.type);
	VALIDATE_CXS ();
	info_cleanup (&info);
        cxp->data = NULL;
        cxp->data_length = 0;
	return (1);
}

static void sdt_cbind_ok (IP_CX *cxp)
{
	static STREAM_CB	sdt_control_cb = {
		NULL,			/* on_new_connection */
		NULL,			/* on_connected */
		continue_write,		/* on_write_completed */
		sdt_control,		/* on_new_message */
		tcp_close_fd		/* on_close */
	};

	SDRFCT		fct;

	cxp->stream_cb = &sdt_control_cb;
	fct = sdt_fsm [SDTE_CXBIND][cxp->p_state];
	if (fct)
		(*fct) (cxp);
}


/**********************************************************************/
/*   TCP Server Control logic.					      */
/**********************************************************************/

static void tcp_server_ctrl_stop (IP_CX *cxp)
{
	IP_CX	*dcxp;

	while ((dcxp = cxp->clients) != NULL)
		if (dcxp->tx_data)
			sdt_stop (dcxp);
		else
			sdr_stop (dcxp);
	tcp_close_fd (cxp);
	log_printf (RTPS_ID, 0, "TCP(SC): stop!");
}

static int tcp_server_ctrl_in (IP_CX *cxp, unsigned char *msg, size_t size)
{
	CtrlInfo_t	info;
	int		n;

	n = tcp_rx_parse_msg (cxp->fd, &info, msg, size);
	if (n < 0 || info.result >= RPSC_ERR_RTPS) {
		log_printf (RTPS_ID, 0, "TCP(SC): message error!\r\n");
		info_cleanup (&info);
		return (0);
	}

	/* Check message type. */
	ADD_ULLONG (cxp->stats.octets_rcvd, size);
	cxp->stats.packets_rcvd++;
	switch (info.type) {
		case CMT_SLPORT_REQ:
			/*log_printf (RTPS_ID, 0, "TCP(SC): ServerLogicalPortRequest received.\r\n");*/
			if (sdr_start (cxp, &info) == DDS_RETCODE_ALREADY_DELETED)
				return (0);
			break;

		case CMT_CLPORT_SUCC:
			/*log_printf (RTPS_ID, 0, "TCP(SC): ClientLogicalPortSuccess received.\r\n");*/
			sdt_success (cxp, &info);
			break;

		case CMT_CLPORT_FAIL:
			/*log_printf (RTPS_ID, 0, "TCP(SC): ClientLogicalPortFail received.\r\n");*/
			info_cleanup (&info);
			sdt_failed (cxp, &info);
			return (0);

		case CMT_FINALIZE:
			/*log_printf (RTPS_ID, 0, "TCP(SC): Finalize received.\r\n");*/
			tcp_server_ctrl_stop (cxp);
			info_cleanup (&info);
			return (0);

		default:
			log_printf (RTPS_ID, 0, "TCP(SC): Unexpected message (%u) received!\r\n", info.type);
	  		break;
	}
	info_cleanup (&info);
	VALIDATE_CXS ();
	return (1);
}

static IP_CX *tcp_server_ctrl_start (TCP_FD *pp, CtrlInfo_t *info)
{
	IP_CX			*cxp, *ccxp, *next_cxp;
	struct linger		l;
	int			ret;
	static STREAM_CB	server_ctrl_cb = {
		NULL,			/* on_new_connection */
		NULL,			/* on_connected */
		NULL,			/* on_write_completed */
		tcp_server_ctrl_in,	/* on_new_message */
		tcp_close_fd		/* on_close */
	};

	cxp = rtps_ip_alloc ();
	if (!cxp) {
		warn_printf ("TCP(SC): not enough memory for context!");
		goto port_rej;
	}
	cxp->locator = xmalloc (sizeof (LocatorNode_t));
	if (!cxp->locator) {
		warn_printf ("TCP(SC): not enough memory for locator!");
		goto free_cx;
	}
	cxp->locator->users = 0;
	cxp->locator->locator = pp->parent->locator->locator;

	cxp->stream_fcts = pp->parent->stream_fcts; /* Inherit transport */
	cxp->stream_cb = &server_ctrl_cb;

	cxp->cx_type = pp->parent->cx_type;
	cxp->cx_side = ICS_SERVER;
	cxp->cx_mode = ICM_CONTROL;
	TCP_NCX_STATE ("SC", cxp, CXS_OPEN);
	TCP_NP_STATE ("SC", cxp, TCS_CONTROL);
	cxp->fd = pp->fd;
	cxp->fd_owner = 1;

	l.l_onoff = 1;
	l.l_linger = 0;
	ret = setsockopt (cxp->fd, SOL_SOCKET, SO_LINGER, &l, sizeof (l));
	if (ret)
		perror ("tcp_server_ctrl_start(): setsockopt(SO_LINGER)");

	memcpy (cxp->dst_addr, pp->dst_addr, 16);
	cxp->dst_port = pp->dst_port;
	cxp->has_dst_addr = 1;
	cxp->dst_forward = info->forward;
	cxp->dst_prefix = info->prefix;
	cxp->has_prefix = 1;
	cxp->associated = 1;
	cxp->parent = pp->parent;
	cxp->sproto = pp->sproto;

	/* Dispose other, already established control channels of client. */
	for (ccxp = cxp->parent->clients; ccxp; ccxp = next_cxp) {
		next_cxp = ccxp->next;
		if (ccxp->has_prefix && 
		    guid_prefix_eq (ccxp->dst_prefix, info->prefix))
			tcp_close_data (ccxp);
	}

	/* Add to list of TCP clients. */
	cxp->next = cxp->parent->clients;
	cxp->parent->clients = cxp;

	/* Indicate successful binding. */
	if (tcp_send_id_bind_success (cxp, info) == DDS_RETCODE_ALREADY_DELETED)
		return (NULL);

	log_printf (RTPS_ID, 0, "TCP(SC): control connection ready!\r\n");
	VALIDATE_CXS ();
	return (cxp);

    free_cx:
	rtps_ip_free (cxp);
    port_rej:
	tcp_send_id_bind_fail (pp->parent, info, CERR_OO_RESOURCES);
	return (NULL);
}

/**********************************************************************/
/*   TCP Common Server logic.					      */
/**********************************************************************/

static IP_CX *tcp_server_data_add (IP_CX *dcxp, CtrlInfo_t *info)
{
	IP_CX	*xp;
	long	cookie;

	if (info->cookie_length != sizeof (tcp_cookie)) {
		log_printf (RTPS_ID, 0, "TCP(SD): incorrect cookie!\r\n");
		return (NULL);
	}
	memcpy (&cookie, info->cookie, sizeof (cookie));

	/* Check matching connection on control connection. */
	for (xp = dcxp->parent->clients; xp; xp = xp->next)
		if (xp->label == cookie)
			break;

	if (!xp) {
		log_printf (RTPS_ID, 0, "TCP(SD): cookie not found!\r\n");
		return (NULL);
	}
	TCP_NCX_STATE ("SD", xp, CXS_OPEN);
	xp->fd = dcxp->fd;
	xp->paired = dcxp;
	dcxp->paired = xp;
	xp->dst_forward = info->forward;
	xp->associated = 1;
	if (tcp_send_cx_bind_success (xp, info) == DDS_RETCODE_ALREADY_DELETED)
		return (NULL);

	if (xp->group)
		sdr_cbind_ok (xp);
	else
		sdt_cbind_ok (xp);
	VALIDATE_CXS ();
	return (xp);
}

static IP_CX *tcp_server_data_start (TCP_FD *pp, CtrlInfo_t *info)
{
	IP_CX		*scxp, *xp = NULL;
	int		ret;
	long		cookie;
	struct linger	l;

	if (info->cookie_length != sizeof (tcp_cookie)) {
		log_printf (RTPS_ID, 0, "TCP(SD): incorrect cookie!\r\n");
		return (NULL);
	}
	memcpy (&cookie, info->cookie, sizeof (cookie));

	/* Find control connection belonging to pending fd. */
	for (scxp = pp->parent->clients; scxp; scxp = scxp->next) {
		/*if (!memcmp (scxp->dst_addr, pp->dst_addr, 16)) {*/

			/* Check matching connection on control connection. */
			for (xp = scxp->clients; xp; xp = xp->next)
				if (xp->label == cookie)
					break;
		/*}
		else
			xp = NULL;*/

		if (xp && xp->label == cookie)
			break;
	}
	if (!xp) {
		log_printf (RTPS_ID, 0, "TCP(SD): cookie not found!\r\n");
		return (NULL);
	}

	xp->stream_fcts = pp->parent->stream_fcts; /* Inherit transport */

	TCP_NCX_STATE ("SD", xp, CXS_OPEN);
	xp->fd = pp->fd;
	xp->fd_owner = 1;
	
	l.l_onoff = 1;
	l.l_linger = 0;
	ret = setsockopt (xp->fd, SOL_SOCKET, SO_LINGER, &l, sizeof (l));
	if (ret)
		perror ("tcp_server_ctrl_start(): setsockopt(SO_LINGER)");

	/*
	memcpy (xp->dst_addr, pp->dst_addr, 16);
	xp->dst_port = pp->dst_port;
	*/
	xp->dst_forward = info->forward;
	xp->sproto = pp->sproto;
	xp->associated = 1;
	if (tcp_send_cx_bind_success (xp, info) == DDS_RETCODE_ALREADY_DELETED)
		return (NULL);

	if (xp->group) {
		log_printf (RTPS_ID, 0, "TCP(SDR:%u): CxBind successful!\r\n", xp->handle);
		sdr_cbind_ok (xp);
	}
	else {
		log_printf (RTPS_ID, 0, "TCP(SDT:%u): CxBind successful!\r\n", xp->handle);
		sdt_cbind_ok (xp);
	}
	VALIDATE_CXS ();
	return (xp);
}

static IP_CX *tcp_pending_in (TCP_FD *pp, unsigned char *msg, size_t size)
{
	IP_CX		*cxp;
	CtrlInfo_t	info;
	int		n;

	n = tcp_rx_parse_msg (pp->fd, &info, msg, size);
	if (n < 0 || info.result >= RPSC_ERR_RTPS) {
		log_printf (RTPS_ID, 0, "TCP(Sp): message error!\r\n");
		info_cleanup (&info);
		return NULL;
	}

	/* Check message type. */
	switch (info.type) {
		case CMT_ID_BIND_REQ:
			cxp = tcp_server_ctrl_start (pp, &info);
			if (cxp) {
				ADD_ULLONG (cxp->stats.octets_rcvd, (unsigned) n);
				cxp->stats.packets_rcvd++;
			}
			VALIDATE_CXS ();
			break;

		case CMT_CX_BIND_REQ:
			cxp = tcp_server_data_start (pp, &info);
			if (cxp) {
				ADD_ULLONG (cxp->stats.octets_rcvd, (unsigned) n);
				cxp->stats.packets_rcvd++;
			}
			VALIDATE_CXS ();
			break;

		default:
			log_printf (RTPS_ID, 0, "TCP(Sp): Unexpected message (%u) received!\r\n", info.type);
			cxp = NULL;
	  		break;
	}
	info_cleanup (&info);
	return (cxp);
}

static void rtps_tcp_server_timeout (uintptr_t user)
{
	IP_CX	*cxp = (IP_CX *) user;

	if (cxp->stream_fcts && cxp->stream_fcts->start_server (cxp)) {
		tmr_start (cxp->timer, TICKS_PER_SEC * 5, (uintptr_t) cxp, rtps_tcp_server_timeout);
		return;
	}
	if (cxp->timer) {
		tmr_free (cxp->timer);
		cxp->timer = NULL;
	}
}

static int rtps_tcp_server_start (unsigned family)
{
	IP_CX			*cxp;
	static STREAM_CB	server_cb = {
		tcp_pending_in,		/* on_new_connection */
		NULL,			/* on_connected */
		NULL,			/* on_write_completed */
		NULL,			/* on_new_message */
		NULL			/* on_close */
	};

#ifndef DDS_IPV6
	ARG_NOT_USED (family)
#endif
	act_printf ("TCP: start server.\r\n");
	cxp = rtps_ip_alloc ();
	if (!cxp) {
		log_printf (RTPS_ID, 0, "rtps_tcp_server_start: out of contexts!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	cxp->locator = xmalloc (sizeof (LocatorNode_t));
	if (!cxp->locator) {
		warn_printf ("rtps_tcp_server_start: out of memory for locator.");
		rtps_ip_free (cxp);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	memset (cxp->locator, 0, sizeof (LocatorNode_t));
	cxp->locator->locator.flags = LOCF_SERVER;

#ifdef DDS_SECURITY
	if (tcp_v4_pars.sport_s) {
#ifdef TCP_SIMULATE_TLS
		cxp->stream_fcts = &tcp_functions;
#else
		cxp->stream_fcts = &tls_functions;
#endif
		cxp->locator->locator.flags |= LOCF_SECURE;
		cxp->locator->locator.sproto = SECC_TLS_TCP;
		cxp->cx_type = CXT_TCP_TLS;
	}
	else {
#endif
		cxp->stream_fcts = &tcp_functions;
		cxp->cx_type = CXT_TCP;
#ifdef DDS_SECURITY
	}
#endif
	cxp->stream_cb = &server_cb;
	cxp->cx_side = ICS_SERVER;
	cxp->cx_mode = ICM_ROOT;
	cxp->cx_state = CXS_CLOSED;

#ifdef DDS_IPV6
	if (family == AF_INET) {
#endif
		cxp->locator->locator.kind = LOCATOR_KIND_TCPv4;
		cxp->locator->locator.port = (tcp_v4_pars.sport_s) ? 
							tcp_v4_pars.sport_s :
							tcp_v4_pars.sport_ns;
		tcpv4_server = cxp;
#ifdef DDS_IPV6
	}
	else {
		cxp->locator->locator.kind = LOCATOR_KIND_TCPv6;
		cxp->locator->locator.port = (tcp_v6_pars.sport_s) ?
							tcp_v6_pars.sport_s :
							tcp_v6_pars.sport_ns;
		tcpv6_server = cxp;
	}
#endif

#ifdef DDS_IPV6
	if (family == AF_INET)
#endif
		tcpv4_server = cxp;
#ifdef DDS_IPV6
	else
		tcpv6_server = cxp;
#endif
	if (!cxp->stream_fcts->start_server (cxp))
		return (DDS_RETCODE_OK);

	cxp->timer = tmr_alloc ();
	if (!cxp->timer) {
		xfree (cxp->locator);
		rtps_ip_free (cxp);
#ifdef DDS_IPV6
		if (family == AF_INET)
#endif
			tcpv4_server = NULL;
#ifdef DDS_IPV6
		else
			tcpv6_server = NULL;
#endif
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	tmr_init (cxp->timer, "TCP_S_RETRY");
	TCP_NCX_STATE ("S", cxp, CXS_WRETRY);
	tmr_start (cxp->timer, TICKS_PER_SEC * 5, (uintptr_t) cxp, rtps_tcp_server_timeout);
	VALIDATE_CXS ();
	return (DDS_RETCODE_ALREADY_DELETED);
}

static void rtps_tcp_server_stop (unsigned family)
{
	IP_CX	*cxp, *hcxp, *next_hcxp;

	act_printf ("TCP: Stop server.\r\n");
	if (family == AF_INET)
		cxp = tcpv4_server;
#ifdef DDS_IPV6
	else if (family == AF_INET6)
		cxp = tcpv6_server;
#endif
	else
		cxp = NULL;
	if (!cxp)
		return;

	/* Stop listening to new connections */
	if ((cxp->fd_owner || cxp->cx_state == CXS_CONREQ) &&
	    cxp->stream_fcts &&
	    cxp->stream_fcts->disconnect)
		cxp->stream_fcts->disconnect (cxp);

	/* Cleanup all existing connections */
	for (hcxp = cxp->clients; hcxp; hcxp = next_hcxp) {
		next_hcxp = hcxp->next;
		tcp_close_fd (hcxp);
	}
	xfree (cxp->locator);
	rtps_ip_free (cxp);
	if (family == AF_INET)
		tcpv4_server = NULL;
#ifdef DDS_IPV6
	else if (family == AF_INET6)
		tcpv6_server = NULL;
#endif
	VALIDATE_CXS ();
}

static int rtps_tcpv4_enable (void)
{
	RTPS_TCP_RSERV	*rp;
	unsigned	i;

	act_printf ("rtps_tcpv4_enable()\r\n");
	tcp_v4_pars.enabled = 1;
	for (i = 0, rp = tcp_v4_pars.rservers;
	     i < tcp_v4_pars.nrservers && i < RTPS_MAX_TCP_SERVERS;
	     i++, rp++) {

		/* Connect to remote server. */
		rtps_tcp_client_start (rp, i, 0);
		VALIDATE_CXS ();
	}
	if (tcp_v4_pars.sport_s || tcp_v4_pars.sport_ns)
		rtps_tcp_server_start (AF_INET);

	return (DDS_RETCODE_OK);
}

static void rtps_tcpv4_disable (void)
{
	unsigned	i;

	act_printf ("rtps_tcpv4_disable()\r\n");
	if (tcp_v4_pars.sport_s || tcp_v4_pars.sport_ns)
		rtps_tcp_server_stop (AF_INET);

	for (i = 0; i < tcp_v4_pars.nrservers && i < RTPS_MAX_TCP_SERVERS; i++) {

		/* Disconnect from remote server. */
		rtps_tcp_client_stop (i);
		VALIDATE_CXS ();
	}
	tcp_v4_pars.enabled = 0;
}

static void tcp_port_change (Config_t c)
{
	Domain_t	*dp;
	unsigned	i, n, oport;
	int		secure, osecure;

#ifdef DDS_SECURITY
	secure = (c == DC_TCP_SecPort);
#else
	secure = 0;
#endif
	if (tcp_v4_pars.sport_s) {
		osecure = 1;
		oport = tcp_v4_pars.sport_s;
	}
	else if (tcp_v4_pars.sport_ns) {
		osecure = 0;
		oport = tcp_v4_pars.sport_ns;
	}
	else {
		osecure = 0;
		oport = 0;
	}
	if (config_defined (c)) {
		if ((i = config_get_number (c, 0)) >= 1024 && i <= 65530) {
			if (tcp_v4_pars.enabled && oport && (secure || !osecure))
				rtps_tcp_server_stop (AF_INET);

			if (secure)
				tcp_v4_pars.sport_s = i;
			else
				tcp_v4_pars.sport_ns = i;
			log_printf (RTPS_ID, 0, "TCP: local %sTCP server port = %u\r\n", (secure) ? "secure " : "", i);
		}
		else
			log_printf (RTPS_ID, 0, "TCP: invalid local %sTCP server port!\r\n", (secure) ? "secure " : "");
	}
	else {
		if (tcp_v4_pars.enabled && oport && secure == osecure)
			rtps_tcp_server_stop (AF_INET);

		if (secure)
			tcp_v4_pars.sport_s = 0;
		else
			tcp_v4_pars.sport_ns = 0;
		log_printf (RTPS_ID, 0, "TCP: local %sTCP server disabled\r\n", (secure) ? "secure " : "");
	}
	if (!tcp_v4_pars.enabled || (!secure && osecure))
		return;

	if (tcp_v4_pars.sport_s || tcp_v4_pars.sport_ns)
		rtps_tcp_server_start (AF_INET);

	/* Propagate the change to all domains. */
	if (domains_used ()) {
		n = 0;
		while ((dp = domain_next (&n, NULL)) != NULL) {
			rtps_participant_update (dp);
			VALIDATE_CXS ();
		}
	}
}

static int is_dns_name (const char *s)
{
	unsigned	ndots, ncolons;

	ndots = ncolons = 0;
	while (*s) {
		if (((*s < '0' || *s > '9') && 
		     *s != '.' && *s != ':') ||
		    (*s == '.' && ++ndots > 3) ||
		    (*s == ':' && ++ncolons > 1))
			return (1);

		s++;
	}
	return (0);
}

static void tcp_server_change (Config_t c)
{
	const char	*env_str;
	char		*save_ptr = NULL, *s, *arg, *cp;
	Domain_t	*dp;
	RTPS_TCP_RSERV	*rp;
	unsigned	n, i, j;
	int		secure, d [4];

#ifdef DDS_SECURITY
	secure = (c == DC_TCP_SecServer);
#else
	ARG_NOT_USED (c)

	secure = 0;
#endif
	if (tcp_v4_pars.enabled) {

		/* First stop all running clients of the given type. */
		for (i = 0, rp = tcp_v4_pars.rservers;
		     i < tcp_v4_pars.nrservers;
		     i++, rp++)
			if (rp->secure == secure)
				rtps_tcp_client_stop (i);
	}

	/* Remove all clients of the given type from the remote servers list. */
	for (i = 0, rp = tcp_v4_pars.rservers; i < tcp_v4_pars.nrservers; )
		if (rp->secure == secure) {
			tcp_v4_pars.nrservers--;
			for (j = i; j < tcp_v4_pars.nrservers; j++)
				tcp_v4_pars.rservers [j] = tcp_v4_pars.rservers [j + 1];
		}
		else {
			i++;
			rp++;
		}
#ifdef DDS_SECURITY
	env_str = config_get_string ((secure) ? DC_TCP_SecServer : DC_TCP_Server, NULL);
#else
	env_str = config_get_string (DC_TCP_Server, NULL);
#endif
	if (env_str) {
		n = strlen (env_str) + 1;
		if (n > sizeof (server_buf)) {
			server_args = xmalloc (n);
			if (!server_args) {
				warn_printf ("No room to store TCP server parameters!");
				return;
			}
		}
		else
			server_args = server_buf;
		memcpy (server_args, env_str, n);
		s = server_args;
		for (; tcp_v4_pars.nrservers < RTPS_MAX_TCP_SERVERS; ) {
			arg = strtok_r (s, ";", &save_ptr);
			if (!arg)
				break;

			s = NULL;
			rp = &tcp_v4_pars.rservers [tcp_v4_pars.nrservers++];
			rp->secure = secure;
			if ((rp->name = is_dns_name (arg)) != 0) {
				rp->addr.name = arg;
				cp = strrchr (arg, ':');
				if (cp) {
					*cp = '\0';
					rp->port = atoi (++cp);
				}
				else
					rp->port = 7400;
				log_printf (RTPS_ID, 0, "TCP: remote %sTCP server at %s:%u\r\n",
						(secure) ? "secure " : "",
						rp->addr.name, rp->port);
			}
			else if (sscanf (arg, "%d.%d.%d.%d:%u", &d [0], &d [1],
						 &d [2], &d [3], &rp->port) >= 1) {
				rp->addr.ipa_v4 = (d [0] << 24) |
						  (d [1] << 16) |
						  (d [2] << 8) |
						   d [3];
				log_printf (RTPS_ID, 0, "TCP: remote %sTCP server at %u.%u.%u.%u:%u\r\n",
						(secure) ? "secure " : "",
						d [0], d [1], d [2], d [3], rp->port);
			}
			/* else - just ignore it ... */
		}
	}

	VALIDATE_CXS ();

	if (!tcp_v4_pars.enabled)
		return;

	/* Start all clients of the given type. */
	for (i = 0, rp = tcp_v4_pars.rservers;
	     i < tcp_v4_pars.nrservers;
	     i++, rp++)
		if (rp->secure == secure)
			rtps_tcp_client_start (&tcp_v4_pars.rservers [i], i, 0);

	VALIDATE_CXS ();

	/* Propagate the change to all domains. */
	if (domains_used ()) {
		n = 0;
		while ((dp = domain_next (&n, NULL)) != NULL) {
			rtps_participant_update (dp);
			VALIDATE_CXS ();
		}
	}
}

static void tcp_public_change (Config_t c)
{
	const char	*env_str;
	char		*cp;
	unsigned	n;
	int		d [4];
	Domain_t	*dp;

	ARG_NOT_USED (c)

	env_str = config_get_string (DC_TCP_Public, NULL);
	if (env_str) {
		n = strlen (env_str) + 1;
		if (n > sizeof (public_buf)) {
			cp = xmalloc (n);
			if (!cp) {
				warn_printf ("No room to store TCP public address!");
				return;
			}
		}
		else
			cp = public_buf;

		if (public_args && public_args != public_buf)
			xfree (public_args); /* free() if it has been malloc()-ed before. */

		public_args = cp;
		tcp_v4_pars.oaddr.name = public_args;
		memcpy (public_args, env_str, n);

		cp = strrchr (tcp_v4_pars.oaddr.name, ':');
		if (cp) {
			*cp++ = '\0';
			if (*cp == '\0')
				tcp_v4_pars.oport = 7400;
			else
				tcp_v4_pars.oport = atoi (cp);
		}
		else
			tcp_v4_pars.oport = 7400;
		if ((tcp_v4_pars.oname = is_dns_name (public_args)) != 0) {
			log_printf (RTPS_ID, 0, "TCP: announce public %sTCP server at %s:%u\r\n",
					(tcp_v4_pars.sport_s) ? "secure " : "",
					tcp_v4_pars.oaddr.name, tcp_v4_pars.oport);
			tcp_v4_pars.oaddr.name = (const char *) public_args;
		}
		else if (sscanf (public_args, "%d.%d.%d.%d:%u", &d [0], &d [1],
					 &d [2], &d [3], &tcp_v4_pars.oport) >= 1) {
			tcp_v4_pars.oaddr.ipa_v4 = (d [0] << 24) |
					  (d [1] << 16) |
					  (d [2] << 8) |
					   d [3];
			log_printf (RTPS_ID, 0, "TCP: announce public %sTCP server at %u.%u.%u.%u:%u\r\n",
					(tcp_v4_pars.sport_s) ? "secure " : "",
					d [0], d [1], d [2], d [3], tcp_v4_pars.oport);
		}
		else
			tcp_v4_pars.oport = 0;

		if (!config_defined (DC_TCP_Private))
			tcp_v4_pars.private_ok = MODE_DISABLED;
	}
	else {
		if (!config_defined (DC_TCP_Private))
			tcp_v4_pars.private_ok = MODE_ENABLED;
		tcp_v4_pars.oport = 0;
		tcp_v4_pars.oaddr.name = NULL;
		tcp_v4_pars.oaddr.ipa_v4 = 0;
		if (public_args && public_args != public_buf)
			xfree (public_args); /* free() if it has been malloc()-ed before */
	}

	/* propagate the change to all domains, if enabled */
	if (tcp_v4_pars.enabled && domains_used ()) {
		n = 0;
		while ((dp = domain_next (&n, NULL)) != NULL) {
			rtps_participant_update (dp);
			VALIDATE_CXS ();
		}
	}
}

static void tcp_private_change (Config_t c)
{
	Domain_t	*dp;
	unsigned	n;
	int		prev_ok;

	ARG_NOT_USED (c)

	prev_ok = tcp_v4_pars.private_ok;
	tcp_v4_pars.private_ok = config_get_mode (DC_TCP_Private,
						    !config_defined (DC_TCP_Public));
	if (config_defined (DC_TCP_Private))
		log_printf (RTPS_ID, 0, "TCP: private TCP server addresses are%sallowed\r\n",
			(tcp_v4_pars.private_ok == 0) ? " not " : " ");

	/* Propagate the change to all domains, if enabled. */
	if (prev_ok != tcp_v4_pars.private_ok &&
	    tcp_v4_pars.enabled &&
	    domains_used ()) {
		n = 0;
		while ((dp = domain_next (&n, NULL)) != NULL) {
			rtps_participant_update (dp);
			VALIDATE_CXS ();
		}
	}
}

/* rtps_tcpv4_init -- Initialize the TCP/IP RTPS transport. */

static int rtps_tcpv4_init (RMRXF    rxf,
			    MEM_DESC msg_hdr,
			    MEM_DESC msg_elem)
{
	int		error;

	act_printf ("rtps_tcpv4_init()\r\n");
	tcp_cookie = random ();
	if ((ip_attached & LOCATOR_KINDS_IPv4) == 0) {
		error = rtps_ipv4_init (rxf, msg_hdr, msg_elem);
		if (error)
			return (error);
	}
	memset (&tcp_v4_pars, 0, sizeof (tcp_v4_pars));
	tcp_v4_pars.port_pars = rtps_udp_def_pars;

#ifdef DDS_SECURITY
	config_notify (DC_TCP_SecPort, tcp_port_change);
	config_notify (DC_TCP_SecServer, tcp_server_change);
#endif
	config_notify (DC_TCP_Port, tcp_port_change);
	config_notify (DC_TCP_Server, tcp_server_change);
	config_notify (DC_TCP_Public, tcp_public_change);
	config_notify (DC_TCP_Private, tcp_private_change);

# if 0
	tcp_port_change (DC_TCP_Port);
	tcp_server_change (DC_TCP_Server);
#ifdef DDS_SECURITY
	tcp_port_change (DC_TCP_SecPort);
	tcp_server_change (DC_TCP_SecServer);
#endif
	tcp_public_change (DC_TCP_Public);
	tcp_private_change (DC_TCP_Private);
# endif
	VALIDATE_CXS ();

	return (DDS_RETCODE_OK);
}

/* rtps_tcpv4_final -- Finalize the TCP/IP RTPS transport. */

static void rtps_tcpv4_final (void)
{
	act_printf ("rtps_tcpv4_final()\r\n");

	if ((ip_attached & LOCATOR_KINDS_IPv4) == 0)
		rtps_ipv4_final ();
}

static int rtps_tcp_pars_set (LocatorKind_t kind, const void *p)
{
	const RTPS_TCP_PARS *pp = (const RTPS_TCP_PARS *) p;
	RTPS_TCP_PARS	pars;

	if (kind != LOCATOR_KIND_TCPv4 &&
	    kind != LOCATOR_KIND_TCPv6)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!p) {
		pars.port_pars = rtps_udp_def_pars;
		pars.sport_s = 0;
		pars.sport_ns = 0;
		pars.nrservers = 0;
		pars.port_pars.pb = config_get_number (DC_TCP_PB, pars.port_pars.pb);
		pars.port_pars.dg = config_get_number (DC_TCP_DG, pars.port_pars.dg);
		pars.port_pars.pg = config_get_number (DC_TCP_PG, pars.port_pars.pg);
		pars.port_pars.d0 = config_get_number (DC_TCP_D0, pars.port_pars.d0);
		pars.port_pars.d1 = config_get_number (DC_TCP_D1, pars.port_pars.d1);
		pars.port_pars.d2 = config_get_number (DC_TCP_D2, pars.port_pars.d2);
		pars.port_pars.d3 = config_get_number (DC_TCP_D3, pars.port_pars.d3);
		pp = &pars;
	}
	else if (!pp->port_pars.pb || pp->port_pars.pb > 0xff00 ||
		 !pp->port_pars.dg || pp->port_pars.dg > 0x8000 ||
		 !pp->port_pars.pg || pp->port_pars.pg > 0x8000 ||
		 pp->port_pars.d0 > 0x8000 ||
		 pp->port_pars.d1 > 0x8000 ||
		 pp->port_pars.d2 > 0x8000 ||
		 pp->port_pars.d3 > 0x8000 ||
		 pp->nrservers > 3)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (kind == LOCATOR_KIND_TCPv4)
		tcp_v4_pars = *pp;
#ifdef DDS_IPV6
	else if (kind == LOCATOR_KIND_TCPv6)
		tcp_v6_pars = *pp;
#endif
	return (DDS_RETCODE_OK);
}

static int rtps_tcp_pars_get (LocatorKind_t kind, void *pars, size_t msize)
{
	if (kind != LOCATOR_KIND_TCPv4 && kind != LOCATOR_KIND_TCPv6)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!pars || msize < sizeof (RTPS_TCP_PARS))
		return (DDS_RETCODE_OUT_OF_RESOURCES);
#ifdef DDS_IPV6
	if (kind == LOCATOR_KIND_TCPv4)
#endif
		memcpy (pars, &tcp_v4_pars, sizeof (tcp_v4_pars));
#ifdef DDS_IPV6
	else
		memcpy (pars, &tcp_v6_pars, sizeof (tcp_v4_pars));
#endif
	return (DDS_RETCODE_OK);
}

/* tcpv4_add_port -- Add a locator based on the IP address and port number. */

static void tcpv4_add_port (LocatorList_t       *llp,
			    const unsigned char *ip,
			    unsigned            port,
			    Scope_t             scope,
			    unsigned            flags,
			    unsigned            sproto)
{
	LocatorKind_t	kind;
	unsigned char	addr [16];

	if (port >= 0xffffU)
		warn_printf ("TCP: can't create locator due to selected DomainId and ParticipantId parameters!");

	if (llp && port < 0xffff) {
		memset (addr, 0, 12);
		memcpy (addr + 12, ip, 4);
		kind = LOCATOR_KIND_TCPv4;
		locator_list_add (llp, kind, addr, port, 0, scope, flags, sproto);
	}
}

static unsigned char *tcpv4_resolve_public (unsigned char *buf, int log)
{
	struct hostent	*he;
	struct in_addr	addr;

	if (tcp_v4_pars.oname) {
		he = gethostbyname (tcp_v4_pars.oaddr.name);
		if (!he) {
			warn_printf ("TCP: public server name could not be resolved!");
			return (NULL);
		}
		addr = *((struct in_addr *) he->h_addr_list [0]);
		if (log)
			log_printf (RTPS_ID, 0, "TCP: public server name resolved to %s\r\n", 
					          inet_ntoa (addr));
	}
	else
		addr.s_addr = htonl (tcp_v4_pars.oaddr.ipa_v4);
	buf [0] = ntohl (addr.s_addr) >> 24;
	buf [1] = (ntohl (addr.s_addr) >> 16) & 0xff;
	buf [2] = (ntohl (addr.s_addr) >> 8) & 0xff;
	buf [3] = ntohl (addr.s_addr) & 0xff;
	return (buf);
}

static void rtps_tcpv4_locators_get (DomainId_t    domain_id,
				     unsigned      participant_id,
				     RTPS_LOC_TYPE type,
				     LocatorList_t *uc,
				     LocatorList_t *mc,
				     LocatorList_t *dst)
{
	unsigned	i;
	unsigned char	*cp;
	Scope_t		scope;
	unsigned char	addr_buf [4];

	ARG_NOT_USED (mc)
	ARG_NOT_USED (dst)

	if (tcp_v4_pars.sport_s || tcp_v4_pars.sport_ns || tcp_v4_pars.nrservers)
		switch (type) {
			case RTLT_USER:
				for (i = 0, cp = ipv4_proto.own;
				     i < ipv4_proto.num_own;
				     i++, cp += OWN_IPV4_SIZE) {
					if (ipv4_proto.filters &&
					    !ip_match (ipv4_proto.filters,
						       domain_id,
						       cp))
						continue;

					memcpy (&scope, cp + OWN_IPV4_SCOPE_OFS, 4);
					if (scope <= SITE_LOCAL &&
					    !tcp_v4_pars.private_ok)
						continue;

					tcpv4_add_port (uc,
						        cp,
						        tcp_v4_pars.port_pars.pb +
						        tcp_v4_pars.port_pars.dg * domain_id + 
						        tcp_v4_pars.port_pars.pg * participant_id +
						        tcp_v4_pars.port_pars.d3,
							scope,
							LOCF_DATA | LOCF_UCAST,
							0);
				}
				if (tcp_v4_pars.oport && tcpv4_resolve_public (addr_buf, 1))
					tcpv4_add_port (uc,
						        addr_buf,
						        tcp_v4_pars.oport +
						        tcp_v4_pars.port_pars.dg * domain_id + 
						        tcp_v4_pars.port_pars.pg * participant_id +
						        tcp_v4_pars.port_pars.d3,
							GLOBAL_SCOPE,
							LOCF_DATA | LOCF_UCAST,
							0);
				break;
			case RTLT_SPDP_SEDP:
				for (i = 0, cp = ipv4_proto.own;
				     i < ipv4_proto.num_own;
				     i++, cp += OWN_IPV4_SIZE) {
					if (ipv4_proto.filters &&
					    !ip_match (ipv4_proto.filters,
					    	       domain_id,
						       cp))
						continue;

					memcpy (&scope, cp + OWN_IPV4_SCOPE_OFS, 4);
					if (scope <= SITE_LOCAL &&
					    !tcp_v4_pars.private_ok)
						continue;

					tcpv4_add_port (uc,
						        cp,
						        tcp_v4_pars.port_pars.pb +
						        tcp_v4_pars.port_pars.dg * domain_id +
						        tcp_v4_pars.port_pars.pg * participant_id +
						        tcp_v4_pars.port_pars.d1,
							scope,
							LOCF_META | LOCF_UCAST,
							0);
				}
				if (tcp_v4_pars.oport && tcpv4_resolve_public (addr_buf, 0)) {
					tcpv4_add_port (uc,
						        addr_buf,
						        tcp_v4_pars.oport +
						        tcp_v4_pars.port_pars.dg * domain_id + 
						        tcp_v4_pars.port_pars.pg * participant_id +
						        tcp_v4_pars.port_pars.d1,
							GLOBAL_SCOPE,
							LOCF_META | LOCF_UCAST,
							0);
				}
				break;
			default:
				break;
		}

	/* Unconditionally add the TCP destination locator, just in case TCP
	   might be enabled later on. */
	if (type == RTLT_SPDP_SEDP)
		tcpv4_add_port (dst,
			        tcp_mcast_ip,
			        tcp_v4_pars.port_pars.pb +
			        tcp_v4_pars.port_pars.dg * domain_id +
			        tcp_v4_pars.port_pars.d0,
				ORG_LOCAL,
				LOCF_META | LOCF_MCAST,
				0);
}

LocatorList_t rtps_tcp_secure_servers (LocatorList_t uclocs)
{
	LocatorList_t	slist = NULL;
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
	unsigned char	abuf [4];

	if (!tcp_v4_pars.enabled || !tcp_v4_pars.sport_s)
		return (NULL);

	/* Add private server locators. */
	if (tcp_v4_pars.private_ok && tcp_v4_pars.sport_s)
		foreach_locator (uclocs, rp, np) {
			if (np->locator.kind != LOCATOR_KIND_TCPv4 ||
			    (np->locator.scope != SITE_LOCAL &&
			     np->locator.scope != GLOBAL_SCOPE))
				continue;

			tcpv4_add_port (&slist,
					&np->locator.address [12],
					tcp_v4_pars.sport_s,
					np->locator.scope,
					LOCF_META | LOCF_DATA |
					LOCF_UCAST | 
					LOCF_SECURE | LOCF_SERVER,
					SECC_TLS_TCP);
		}

	/* Add public server if defined. */
	if (tcp_v4_pars.oport) {
		if (tcp_v4_pars.oname) {
			if (tcpv4_resolve_public (abuf, 0))
				tcpv4_add_port (&slist,
						abuf,
						tcp_v4_pars.oport,
						sys_ipv4_scope (abuf),
						LOCF_META | LOCF_DATA |
						LOCF_UCAST | 
						LOCF_SECURE | LOCF_SERVER,
						SECC_TLS_TCP);
		}
		else {
			abuf [0] = tcp_v4_pars.oaddr.ipa_v4 >> 24;
			abuf [1] = ((tcp_v4_pars.oaddr.ipa_v4 >> 16) & 0xff);
			abuf [2] = ((tcp_v4_pars.oaddr.ipa_v4 >> 8) & 0xff);
			abuf [3] = (tcp_v4_pars.oaddr.ipa_v4 & 0xff);
			tcpv4_add_port (&slist,
					abuf,
					tcp_v4_pars.oport,
					sys_ipv4_scope (abuf),
					LOCF_META | LOCF_DATA |
					LOCF_UCAST | 
					LOCF_SECURE | LOCF_SERVER,
					SECC_TLS_TCP);
		}
	}
	return (slist);
}

static int ip_socket_match (int fd, Locator_t *lp)
{
	struct sockaddr_in	sa_v4;
#ifdef DDS_IPV6
	struct sockaddr_in6	sa_v6;
#endif
	struct sockaddr		*sa;
	socklen_t		len, nlen;
	unsigned		family;
	int			r, eq;
#ifdef LOG_GSOCKNAME
	char			buf [128];
#endif

	if (fd <= 0)
		return (0);

	if ((lp->kind & LOCATOR_KINDS_IPv4) != 0) {
		memset (&sa_v4, 0, sizeof (sa_v4));
		sa_v4.sin_family = family = AF_INET;
		sa = (struct sockaddr *) &sa_v4;
		nlen = len = sizeof (sa_v4);
	}
#ifdef DDS_IPV6
	else if ((lp->kind & LOCATOR_KINDS_IPv6) != 0) {
		memset (&sa_v6, 0, sizeof (sa_v6));
		sa_v6.sin6_family = family = AF_INET6;
		sa = (struct sockaddr *) &sa_v6;
		nlen = len = sizeof (sa_v6);
	}
#endif
	else
		return (0);

	r = getsockname (fd, sa, &nlen);
	if (r < 0 || nlen > len) {
		if (r < 0)
			perror ("getsockname()");
		return (0);
	}
#ifdef LOG_GSOCKNAME
	if (sa_v4.sin_family == AF_INET)
		inet_ntop (AF_INET, &sa_v4.sin_addr.s_addr, buf, 128);
	else
		inet_ntop (AF_INET6, sa_v6.sin6_addr.s6_addr, buf, 128);
	log_printf (RTPS_ID, 0, "Check %s <=> real socket address: %s", locator_str (lp), buf);
#endif
#ifdef DDS_IPV6
	if (family == AF_INET)
#endif
		eq = (sa_v4.sin_addr.s_addr == htonl ((lp->address [12] << 24) |
						        (lp->address [13] << 16) |
						        (lp->address [14] << 8) |
						         lp->address [15]));
#ifdef DDS_IPV6
	else
		eq = !memcmp (sa_v6.sin6_addr.s6_addr, lp->address, 16);
#endif
#ifdef LOG_GSOCKNAME
	log_printf (RTPS_ID, 0, ": result = %d\r\n", eq);
#endif
	return (eq);
}

static int rtps_tcpv4_add_locator (DomainId_t    domain_id,
				   LocatorNode_t *lnp,
				   unsigned      id,
				   int           serve)
{
	Locator_t	 *lp = &lnp->locator;
	IP_CX		 *cxp, *scxp, *ccxp;
	char		 buf [24];
	unsigned	 i;
#ifdef DDS_NATIVE_SECURITY
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;
#endif

	ARG_NOT_USED (domain_id)
	ARG_NOT_USED (serve)

	snprintf (buf, sizeof (buf), "%u.%u.%u.%u:%u",
				lp->address [12], lp->address [13],
				lp->address [14], lp->address [15],
				lp->port);
	log_printf (RTPS_ID, 0, "TCP: adding %s\r\n", buf);

	cxp = rtps_ip_lookup (id, lp);
	if (cxp) {
		if (cxp->redundant) {
			cxp->redundant = 0;
			/*log_printf (RTPS_ID, 0, "Resetting redundant on %p (%d)\r\n", (void *) cxp, cxp->handle);*/
			if (tcpv4_server)
				for (scxp = tcpv4_server->clients; scxp; scxp = scxp->next) {/* Iterate over the TCP.H contexts */
					for (ccxp = scxp->clients; ccxp; ccxp = ccxp->next) /* Iterate over the corresponding TCP.D contexts */
						if (ccxp->fd_owner && ip_socket_match (ccxp->fd, lp)) {
							ccxp->redundant = 0;
							/*log_printf (RTPS_ID, 0, " Resetting redundant on server TCP data %p (%d)\r\n", (void *) ccxp, ccxp->handle);*/
							if (ccxp->paired) {
								ccxp->paired->redundant = 0;
								/*log_printf (RTPS_ID, 0, " Resetting redundant on paired server TCP data %p (%d)\r\n", (void *) ccxp->paired, ccxp->paired->handle);*/
							}
						}
					if (scxp->fd_owner && ip_socket_match (scxp->fd, lp)) {
						scxp->redundant = 0;
						/*log_printf (RTPS_ID, 0, " Resetting redundant on server TCP control %p (%d)\r\n", (void *) scxp, scxp->handle);*/
					}
				}
#ifdef DDS_IPV6
			if (tcpv6_server) {
				for (scxp = tcpv6_server->clients; scxp; scxp = scxp->next) /* Iterate over the TCP.H contexts */
					for (ccxp = scxp->clients; ccxp; ccxp = ccxp->next) /* Iterate over the corresponding TCP.D contexts */
						if (ccxp->fd_owner && ip_socket_match (ccxp->fd, lp)) {
							ccxp->redundant = 0;
							/*log_printf (RTPS_ID, 0, " Resetting redundant on server TCPv6 data %p (%d)\r\n", (void *) ccxp, ccxp->handle);*/
							if (ccxp->paired) {
								ccxp->paired->redundant = 0;
								/*log_printf (RTPS_ID, 0, " Resetting redundant on paired server TCPv6 data %p (%d)\r\n", (void *) ccxp->paired, ccxp->paired->handle);*/
							}
						}
					if (scxp->fd_owner && ip_socket_match (scxp->fd, lp)) {
						scxp->redundant = 0;
						/*log_printf (RTPS_ID, 0, " Resetting redundant on server TCPv6 control %p (%d)\r\n", (void *) scxp, scxp->handle);*/
					}
			}
#endif
			for (i = 0; i < TCP_MAX_CLIENTS; i++) { /* Iterate over the TCP.C contexts */
				scxp = tcp_client [i];
				if (!scxp)
					continue;

				for (ccxp = scxp->clients; ccxp; ccxp = ccxp->next) /* Iterate over the corresponding TCP.D contexts */
					if (ccxp->fd_owner && ip_socket_match (ccxp->fd, lp)) {
						ccxp->redundant = 0;
						/*log_printf (RTPS_ID, 0, " Resetting redundant on client TCP data %p (%d)\r\n", (void *) ccxp, cxp->handle);*/
						if (ccxp->paired) {
							ccxp->paired->redundant = 0;
							/*log_printf (RTPS_ID, 0, " Resetting redundant on paired client TCP data %p (%d)\r\n", (void *) ccxp->paired, ccxp->paired->handle);*/
						}
					}
				if (ip_socket_match (scxp->fd, lp)) {
					scxp->redundant = 0;
					/*log_printf (RTPS_ID, 0, " Resetting redundant on client TCP control %p (%d)\r\n", (void *) scxp, scxp->handle);*/
				}
			}
			VALIDATE_CXS ();
			return (DDS_RETCODE_OK);
		}
		else {
			log_printf (RTPS_ID, 0, "rtps_tcpv4_add_locator (%s): already exists!\r\n", buf);
			return (DDS_RETCODE_PRECONDITION_NOT_MET);
		}
	}
	cxp = rtps_ip_alloc ();
	if (!cxp) {
		log_printf (RTPS_ID, 0, "tcpv4_add_locator: out of contexts!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	locator_lock (lp);
	cxp->locator = lnp;
	lnp->users++;
	locator_release (lp);
	cxp->id = id;
	cxp->cx_type = CXT_TCP;
	cxp->cx_mode = ICM_DATA;
	cxp->cx_state = CXS_CLOSED;
	cxp->associated = 0;
	cxp->rx_data = 1;
	cxp->tx_data = 0;
#ifdef DDS_NATIVE_SECURITY
	dp = domain_get (id, 0, &ret);
	if (dp && dp->security) {
		cxp->dst_prefix = dp->participant.p_guid_prefix;
		cxp->has_prefix = 1;
	}
#endif
	/* Complete the locator context. */
	rtps_ip_new_handle (cxp);
	cxp->locator->locator.handle = cxp->handle;

	VALIDATE_CXS ();

	return (DDS_RETCODE_OK);
}

void rtps_tcpv4_rem_locator (unsigned id, LocatorNode_t *lnp)
{
	rtps_ip_rem_locator (id, lnp);
}

void rtps_tcp_addr_update_start (unsigned family)
{
	IP_CX		*cxp;
	unsigned	i;

	/*printf ("rtps_tcp_addr_update_start(%u);\r\n", family);*/
	if (family == AF_INET && tcpv4_server)
		for (cxp = tcpv4_server->clients; cxp; cxp = cxp->next)
			cxp->redundant = 1;

#ifdef DDS_IPV6
	if (family == AF_INET6 && tcpv6_server)
		for (cxp = tcpv6_server->clients; cxp; cxp = cxp->next)
			cxp->redundant = 1;
#endif

	for (i = 0; i < TCP_MAX_CLIENTS; i++) { /* Iterate over the TCP.C contexts */
		cxp = tcp_client [i];
		if (!cxp)
			continue;

		if ((family == AF_INET && cxp->locator->locator.kind == LOCATOR_KIND_TCPv4) 
#ifdef DDS_IPV6
		  || (family == AF_INET6 && cxp->locator->locator.kind == LOCATOR_KIND_TCPv6)
#endif
		  									   )
			cxp->redundant = 1;
	}
	VALIDATE_CXS ();
}

void rtps_tcp_addr_update_end (unsigned family)
{
	IP_CX		*cxp, *next_cxp;
	unsigned	i;

	/*printf ("rtps_tcp_addr_update_end(%u);\r\n", family);*/
	if (family == AF_INET && tcpv4_server)
		for (cxp = tcpv4_server->clients; cxp; cxp = next_cxp) {
			next_cxp = cxp->next;
			if (cxp->redundant) {
				log_printf (RTPS_ID, 0, "TCP(SC): close control connection [%d]!\r\n", cxp->fd);
				tcp_close_fd (cxp);
			}
		}
	VALIDATE_CXS ();

#ifdef DDS_IPV6
	if (family == AF_INET6 && tcpv6_server)
		for (cxp = tcpv6_server->clients; cxp; cxp = next_cxp) {
			next_cxp = cxp->next;
			if (cxp->redundant)
				tcp_close_fd (cxp);
		}
#endif
	VALIDATE_CXS ();

	for (i = 0; i < TCP_MAX_CLIENTS; i++) { /* Iterate over the TCP.C contexts */
		cxp = tcp_client [i];
		if (!cxp)
			continue;

		if (cxp->redundant &&
		    ((family == AF_INET && cxp->locator->locator.kind == LOCATOR_KIND_TCPv4) 
#ifdef DDS_IPV6
		  || (family == AF_INET6 && cxp->locator->locator.kind == LOCATOR_KIND_TCPv6)
#endif
		  									   )) {
			log_printf (RTPS_ID, 0, "TCP(CC): close control connection [%d]!\r\n", cxp->fd);
			tcp_cx_closed (cxp);
			VALIDATE_CXS ();

			while (cxp->clients)
				tcp_close_data (cxp->clients);
			cc_retry (cxp, 1);
			VALIDATE_CXS ();
		}
	}
}

static IP_CX *tcp_new_cx (unsigned     id,
			  Locator_t    *lp,
			  IP_CX        *ccxp,
			  GuidPrefix_t *dst_prefix)
{
	IP_CX		 *cxp /*, *xp*/;
	LocatorNode_t	 *xlnp;
	unsigned	 share;
	DDS_ReturnCode_t ret;

	log_printf (RTPS_ID, 0, "TCP(%cDT): create new data context (%u)\r\n", 
					(ccxp->cx_side == ICS_SERVER) ? 'S' : 'C', lp->port);
	cxp = rtps_ip_alloc ();
	if (!cxp) {
		log_printf (RTPS_ID, 0, "tcp_new_cx: out of contexts!\r\n");
		return (NULL);
	}
	xlnp = xmalloc (sizeof (LocatorNode_t));
	if (!xlnp) {
		rtps_ip_free (cxp);
		log_printf (RTPS_ID, 0, "tcp_new_cx: out of memory for locator node!\r\n");
		return (NULL);
	}
	memset (xlnp, 0, sizeof (LocatorNode_t));
	xlnp->locator.kind = lp->kind;
	/*xlnp->locator.port = lp->port;*/
	cxp->locator = xlnp;
	cxp->dst_port = lp->port;
	if (dst_prefix) {
		cxp->dst_prefix = *dst_prefix;
		memcpy (cxp->dst_addr, lp->address, 16);
		cxp->has_dst_addr = 1;
		cxp->has_prefix = 1;
	}
	else
		memset (cxp->dst_addr, 0, sizeof (cxp->dst_addr));
	cxp->id = id;
	cxp->users = 1;
	cxp->cx_type = ccxp->cx_type;
	cxp->cx_mode = ICM_DATA;
	cxp->cx_side = ccxp->cx_side;
	cxp->cx_state = CXS_CLOSED;
	cxp->p_state = TDS_IDLE;
	cxp->associated = 1;
	cxp->rx_data = 0;
	cxp->tx_data = 1;
	cxp->share = 1;
	cxp->label = tcp_cookie++;
	cxp->timer = tmr_alloc ();
	if (!cxp->timer) {
		rtps_ip_free (cxp);
		log_printf (RTPS_ID, 0, "tcp_new_cx: out of memory for timer!\r\n");
		return (NULL);
	}
	tmr_init (cxp->timer, "TCP-Data");
	rtps_ip_new_handle (cxp);
	cxp->locator->locator.handle = cxp->handle;
	cxp->next = ccxp->clients;
	/* cxp->sproto = ccxp->sproto; */
	ccxp->clients = cxp;
	cxp->parent = ccxp;
	cxp->stream_fcts = ccxp->stream_fcts; /* Inherit transport/mode/protocol. */
	cxp->locator->locator.flags = ccxp->locator->locator.flags | lp->flags;
	cxp->locator->locator.sproto = ccxp->locator->locator.sproto;

#ifdef TCP_TRC_CX
	log_printf (RTPS_ID, 0, "tcp_new_cx: new connection!\r\n");
	rtps_ip_dump_cx (cxp, 1);
#endif

	/* If control connection is up, ask peer for logical port.
	   If not up, set the connection to pending. */
	if (ccxp->p_state == TCS_CONTROL) {
		/*dbg_printf ("!tcp_new_cx(%p)!\r\n", cxp);*/
		TCP_NP_STATE ("xDT", cxp, TDS_WPORTOK);
		/*xp = (cxp->share) ? reverse_cx (cxp) : NULL;*/
#if TCP_SHARE
		share = cxp->share /*(xp != NULL) !!! Always allow sharing !!! */;
#else
		share = 0;
#endif
		if (ccxp->cx_side == ICS_SERVER)
			ret = tcp_send_clport_request (cxp, sdt_timeout, CLPREQ_TO,
						       &cxp->label, sizeof (cxp->label), share);
		else
			ret = tcp_send_slport_request (cxp, cdt_timeout, SLPREQ_TO, share);
		if (ret == DDS_RETCODE_ALREADY_DELETED)
			return (NULL);
	}
	else {
		TCP_NP_STATE ("xDT", cxp, TDS_WCONTROL);
		tmr_start (cxp->timer, CCWAIT_TO, (uintptr_t) cxp, cdt_timeout);
	}
	VALIDATE_CXS ();
	return (cxp);
}

static int info_destination_prefix (RMBUF *msgs, GuidPrefix_t *prefix)
{
	RME			*ep;
	InfoDestinationSMsg	*dp;

	for (ep = msgs->first; ep; ep = ep->next)
		if ((ep->flags & RME_HEADER) != 0 &&
		    ep->header.id == ST_INFO_DST) {
			dp = (InfoDestinationSMsg *) ep->d;
			*prefix = dp->guid_prefix;
			return (1);
		}

	return (0);
}

/* tcp_enqueue_msgs -- Enqueue and try to send messages on a data connection. */

static void tcp_enqueue_msgs (IP_CX *cxp, RMBUF *msgs)
{
	RMBUF	*mp;
	RMREF	*rp;
	int	send_queued = 0;

	if (!msgs)
		return;

	for (rp = cxp->head; rp; rp = rp->next)
		if (rp->message == msgs) {
#ifdef TCP_TRC_CX
			log_printf (RTPS_ID, 0, "*  Channel (%p) - ignore: already queued!\r\n", (void *) cxp);
#endif
			return;
		}
			
	/* Enqueue data in TCP TxData context. */
	for (mp = msgs; mp; mp = mp->next) {
		rp = rtps_ref_message (mp);
		if (!rp)
			break;

		if (cxp->head)
			cxp->tail->next = rp;
		else {
			send_queued = 1;
			cxp->head = rp;
		}
		cxp->tail = rp;
		cxp->stats.nqueued++;
	}
#ifdef TCP_TRC_CX
	log_printf (RTPS_ID, 0, "*  Channel (%p) - messages queued!\r\n", (void *) cxp);
#endif
	if (!send_queued)
		return;

	/* Try to send immediately if possible. */
	if (cxp->cx_side == ICS_SERVER)
		sdt_data_queued (cxp);
	else
		cdt_data_queued (cxp);
}

/* tcp_channel_create -- Get/initiate a data connection on existing control
			 channel. This might lead to a new TCP data connection
			 being setup, or an already existing TCP data
			 connection being returned. */

static IP_CX *tcp_channel_create (unsigned     id,
			          Locator_t    *lp,
			          IP_CX        *ccxp,
				  GuidPrefix_t *prefix)
{
	IP_CX	*cxp;

	for (cxp = ccxp->clients; cxp; cxp = cxp->next) {
#ifdef TCP_TRC_CX
		rtps_ip_dump_cx (cxp, 1);
		log_printf (RTPS_ID, 0, "TCP(setup): check with %p - ", (void *) cxp);
#endif
		if (cxp->id == id &&
		    (lp->flags & LOCF_MODE) == (cxp->locator->locator.flags & LOCF_MODE) &&
		    cxp->tx_data) {
			if (lp->port == cxp->dst_port) {
#ifdef TCP_TRC_CX
				log_printf (RTPS_ID, 0, "match!\r\n");
#endif
				break;
			}
			else if (cxp->p_state > TDS_WPORTOK) {
#ifdef TCP_TRC_CX
				log_printf (RTPS_ID, 0, "mode exists but different port!\r\n");
#endif
				return (NULL);
			}
		}
#ifdef TCP_TRC_CX
		log_printf (RTPS_ID, 0, "no match.\r\n");
#endif
	}
	if (!cxp) {
		cxp = tcp_new_cx (id, lp, ccxp, prefix);
		if (!cxp) {
#ifdef TCP_TRC_CX
			log_printf (RTPS_ID, 0, "*  Can't create channel!\r\n");
#endif
			return (NULL);
		}
	}
#ifdef TCP_TRC_CX
	log_printf (RTPS_ID, 0, "*  Channel (%p) - found!\r\n", (void *) cxp);
#endif
	return (cxp);
}

static IP_CX *tcp_channel_lookup (unsigned     id,
				  Locator_t    *lp,
				  IP_CX        *ccxp,
				  GuidPrefix_t *prefix)
{
	IP_CX	*cxp;

	for (cxp = ccxp->clients; cxp; cxp = cxp->next)
		if (cxp->id == id &&
		    cxp->tx_data &&
		    cxp->dst_port == lp->port &&
		    cxp->locator->locator.kind == lp->kind &&
		    cxp->has_prefix &&
		    guid_prefix_eq (cxp->dst_prefix, *prefix))
			return (cxp);

	return (NULL);
}

static int tcp_setup_from_prefix (unsigned     id,
				  Locator_t    *lp,
				  RMBUF        *msgs,
				  GuidPrefix_t *prefix)
{
	IP_CX		*cxp, *ccxp, *xcxp;
#ifdef TCP_FWD_FALLBACK
	IP_CX		*rcxp;
#endif
	unsigned	i;
	GuidPrefix_t	nprefix;
#ifdef TCP_TRC_CX
	char		buf [30];
#endif

	/* Find a control connection that can be used. */
#ifdef TCP_TRC_CX
	log_printf (RTPS_ID, 0, "* Prefix found: %s\r\n", guid_prefix_str (prefix, buf));
#endif
	xcxp = NULL;
#ifdef TCP_FWD_FALLBACK
	rcxp = NULL;
#endif
	nprefix = *prefix;
	guid_normalise (&nprefix);
	cxp = NULL;
	for (i = 0; i < TCP_MAX_CLIENTS; i++) {
		ccxp = tcp_client [i];
		if (!ccxp)
			break;

#ifdef TCP_FWD_FALLBACK
		if (ccxp->dst_forward)
			rcxp = ccxp;
#endif
		if (ccxp->locator->locator.kind != lp->kind)
			continue;

		if (ccxp->has_prefix &&
		    guid_prefix_eq (ccxp->dst_prefix, nprefix)) {
			xcxp = ccxp;
			break;
		}
		cxp = tcp_channel_lookup (id, lp, ccxp, prefix);
		if (cxp) {
			xcxp = ccxp;
			break;
		}
	}
	if (!xcxp && lp->kind == LOCATOR_KIND_TCPv4 && tcpv4_server)
		for (ccxp = tcpv4_server->clients; ccxp; ccxp = ccxp->next) {
#ifdef TCP_FWD_FALLBACK
			if (!rcxp && ccxp->dst_forward)
				rcxp = ccxp;
#endif
			if (ccxp->has_prefix &&
			    guid_prefix_eq (ccxp->dst_prefix, nprefix)) {
				xcxp = ccxp;
				break;
			}
			cxp = tcp_channel_lookup (id, lp, ccxp, prefix);
			if (cxp) {
				xcxp = ccxp;
				break;
			}
		}
#ifdef DDS_IPV6
	else if (!xcxp && lp->kind == LOCATOR_KIND_TCPv6 && tcpv6_server)
		for (ccxp = tcpv6_server->clients; ccxp; ccxp = ccxp->next) {
#ifdef TCP_FWD_FALLBACK
			if (!rcxp && ccxp->dst_forward)
				rcxp = ccxp;
#endif
			if (ccxp->has_prefix &&
			    guid_prefix_eq (ccxp->dst_prefix, nprefix)) {
				xcxp = ccxp;
				break;
			}
			cxp = tcp_channel_lookup (id, lp, ccxp, prefix);
			if (cxp) {
				xcxp = ccxp;
				break;
			}
		}
#endif
#ifdef TCP_FWD_FALLBACK
	if (!xcxp)
		xcxp = rcxp;
#endif
	if (!xcxp)
		return (0);

	if (!cxp)
		cxp = tcp_channel_create (id, lp, xcxp, prefix);
	if (cxp) {
		lp->handle = cxp->handle;
#ifdef TCP_TRC_CX
		log_printf (RTPS_ID, 0, "*  Channel (%p) - enqueue messages (%p)!\r\n", (void *) cxp, (void *) msgs);
#endif
		tcp_enqueue_msgs (cxp, msgs);
	}
	return (1);
}

static unsigned tcp_channel_enqueue (IP_CX    *cxp,
				     RMBUF    *msgs,
				     IP_CX    *tp [],
				     unsigned n)
{
	unsigned	i;

	for (i = 0; i < n; i++)
		if (tp [i] == cxp)
			return (n);

#ifdef TCP_TRC_CX
	log_printf (RTPS_ID, 0, "*  Channel (%p) - enqueue messages (%p)!\r\n", (void *) cxp, (void *) msgs);
#endif
	tcp_enqueue_msgs (cxp, msgs);
	if (n < MAX_IP_Q_DESTS)
		tp [n++] = cxp;
	return (n);
}

static void tcp_setup_data (unsigned id, Locator_t *lp, RMBUF *msgs)
{
	IP_CX		*cxp, *ccxp;
	unsigned	i, n;
	int		has_prefix;
	GuidPrefix_t	prefix;
	IP_CX		*table [MAX_IP_Q_DESTS];

#ifdef TCP_TRC_CX
	log_printf (RTPS_ID, 0, "*Data => %s (msgs=%p)!\r\n", locator_str (lp), (void *) msgs);
#endif
	has_prefix = info_destination_prefix (msgs, &prefix);
	if (has_prefix) {

		/* Use prefix to send data to the correct control channel.
		   If the prefix doesn't belong to an existing control channel,
		   but we have a connection to a forwarder, send it to that
		   one.  In the other case, we simply drop the data until a
		   proper control channel becomes available since we will retry
		   on DDS level until connectivity is established. */
		if (!tcp_setup_from_prefix (id, lp, msgs, &prefix))
			goto no_prefix;
	}
	else {

	    no_prefix:

		/* Since we don't have a prefix, send it on all control
		   channels, mimicking multicast behaviour. This means also
		   that data will potentially be queued on several control
		   channels simultaneously. */
		n = 0;
		for (i = 0; i < TCP_MAX_CLIENTS; i++) {
			ccxp = tcp_client [i];
			if (!ccxp)
				break;

			if (ccxp->locator->locator.kind != lp->kind)
				continue;

			cxp = tcp_channel_create (id, lp, ccxp, NULL);
			if (cxp)
				n = tcp_channel_enqueue (cxp, msgs, table, n);
		}
		if (lp->kind == LOCATOR_KIND_TCPv4 && tcpv4_server)
			for (ccxp = tcpv4_server->clients; ccxp; ccxp = ccxp->next) {
				cxp = tcp_channel_create (id, lp, ccxp, NULL);
				if (cxp)
					n = tcp_channel_enqueue (cxp, msgs, table, n);
			}
#ifdef DDS_IPV6
		else if (lp->kind == LOCATOR_KIND_TCPv6 && tcpv6_server) {
			for (ccxp = tcpv6_server->clients; ccxp; ccxp = ccxp->next) {
				cxp = tcp_channel_create (id, lp, ccxp, NULL);
				if (cxp)
					n = tcp_channel_enqueue (cxp, msgs, table, n);
			}
		}
#endif
	}
}

/* tcp_send_spdp_forward -- Send an RTPS SPDP message via the specified
			    server->client control connection. */

void tcp_send_meta_forward (unsigned id, Locator_t *lp, IP_CX *ccxp, RMBUF *msgs)
{
	IP_CX	*cxp;

	/* Find a data connection suitable for meta communication. */
	for (cxp = ccxp->clients; cxp; cxp = cxp->next)
		if (cxp->id == id &&
		    (cxp->locator->locator.flags & (LOCF_META | LOCF_MCAST)) == 
						   (LOCF_META | LOCF_MCAST) &&
		    cxp->tx_data)
			break;

	if (!cxp) {

		/* If no meta data connection found, create a new one. */
		cxp = tcp_new_cx (id, lp, ccxp, NULL);
		if (!cxp)
			return;

		memcpy (cxp->dst_addr, lp->address, 16);
		cxp->has_dst_addr = 1;
	}

	/* Enqueue and send the metadata messages. */
	tcp_enqueue_msgs (cxp, msgs);
}

int tcp_is_server (LocatorKind_t kind, unsigned char *addr, unsigned port)
{
	unsigned	i;
	IP_CX		*cxp;

	ARG_NOT_USED (port)

	for (i = 0; i < TCP_MAX_CLIENTS; i++) {
		cxp = tcp_client [i];
		if (!cxp)
			break;

		if (cxp->locator->locator.kind == kind &&
		    !memcmp (cxp->locator->locator.address, addr, 16))
			return (1);
	}
	return (0);
}

/* tcp_send_meta_reverse -- Send an RTPS SPDP message via reverse channel
			    setup to clients. */

void tcp_send_meta_reverse (unsigned id, Locator_t *lp, IP_CX *server, RMBUF *msgs)
{
	IP_CX		*ccxp, *cxp, *tx_cxp;

	tx_cxp = NULL;
	for (ccxp = server->clients; ccxp; ccxp = ccxp->next) {
		if (ccxp->p_state != TCS_CONTROL ||
		    tcp_is_server (ccxp->locator->locator.kind,
		    	           ccxp->dst_addr,
			           ccxp->dst_port))
			continue;

		for (cxp = ccxp->clients; cxp; cxp = cxp->next)
			if (cxp->id == id &&
			    lp->port == cxp->dst_port &&
			    cxp->tx_data)
				break;

		if (!cxp) {
			cxp = tcp_new_cx (id, lp, ccxp, NULL);
			if (!cxp)
				continue;

			memcpy (cxp->dst_addr, lp->address, 16);
			cxp->has_dst_addr = 1;
		}
		if (tx_cxp)
			tcp_enqueue_msgs (tx_cxp, msgs);
		tx_cxp = cxp;
	}
	if (tx_cxp)
		tcp_enqueue_msgs (tx_cxp, msgs);
}

#ifdef LOG_TCP_SEND
#define ttx_print(s)		log_printf (RTPS_ID, 0, s)
#define ttx_print1(s,a)		log_printf (RTPS_ID, 0, s, a)
#define ttx_print2(s,a1,a2)	log_printf (RTPS_ID, 0, s, a1, a2)
#else
#define ttx_print(s)
#define ttx_print1(s,a)
#define ttx_print2(s,a1,a2)
#endif

/* rtps_tcp_send -- Send RTPS messages on TCP connection(s). */

void rtps_tcp_send (unsigned id, Locator_t *lp, LocatorList_t *next, RMBUF *msgs)
{
	IP_CX		*cxp, *ccxp, *tx_cxp;
	unsigned	i;
	int		forward = 0;
#ifdef LOG_TCP_SEND
	char		buf [128];
#endif

	ARG_NOT_USED (next)

	if ((lp->flags & (LOCF_META | LOCF_MCAST)) == (LOCF_META | LOCF_MCAST)) {
		ttx_print1 ("rtps_tcp_send: meta-tx request for %u.\r\n", lp->port);
		tx_cxp = NULL;

		/* Meta connection needed -- check connections to server. */
		for (i = 0; i < TCP_MAX_CLIENTS; i++) {
			ccxp = tcp_client [i];
			if (!ccxp)
				break;

			if (ccxp->locator->locator.kind == lp->kind) {

				/* Send data on existing server connection. */
				if (tx_cxp) {
					ttx_print ("send_meta_fwd(1)!\r\n");
					tcp_send_meta_forward (id, lp, tx_cxp, msgs);
				}
				tx_cxp = ccxp;
				forward = 1;
			}
		}

		/* If we are a server, try to use reverse channel. */
		if (lp->kind == LOCATOR_KIND_TCPv4 && tcpv4_server) {
			if (tcpv4_server->clients) {
				if (tx_cxp) {
					ttx_print ("send_meta_fwd(2)!\r\n");
					tcp_send_meta_forward (id, lp, tx_cxp, msgs);
				}
				tx_cxp = tcpv4_server;
				forward = 0;
			}
		}
#ifdef DDS_IPV6
		else if (lp->kind == LOCATOR_KIND_TCPv6 && tcpv6_server) {
			if (tcpv6_server->clients) {
				if (tx_cxp) {
					ttx_print ("send_meta_fwd(3)!\r\n");
					tcp_send_meta_forward (id, lp, tx_cxp, msgs);
				}
				tx_cxp = tcpv6_server;
				forward = 0;
			}
		}
#endif
		if (tx_cxp) {
			if (forward) {
				ttx_print ("send_meta_fwd(4)!\r\n");
				tcp_send_meta_forward (id, lp, tx_cxp, msgs);
			}
			else {
				ttx_print ("send_meta_rev!\r\n");
				tcp_send_meta_reverse (id, lp, tx_cxp, msgs);
			}
		}
	}
	else {
#ifdef LOG_TCP_SEND
		ttx_print ("rtps_tcp_send: data-tx request for ");
		if ((lp->kind & LOCATOR_KINDS_IPv4) != 0)
			ttx_print2 ("%s:%u.", 
				    inet_ntop (AF_INET, lp->address + 12, buf, 128),
			         	lp->port);
		else if ((lp->kind & LOCATOR_KINDS_IPv6) != 0)
			ttx_print2 ("%s:%u.", 
				inet_ntop (AF_INET6, lp->address, buf, 128),
				lp->port);
		else
			ttx_print2 ("?(%u)...:%u", lp->kind, lp->port);

		ttx_print1 (" h:%d", lp->handle);
		if ((lp->flags & LOCF_DATA) != 0)
			ttx_print (" UD");
		if ((lp->flags & LOCF_META) != 0)
			ttx_print (" MD");
		if ((lp->flags & LOCF_UCAST) != 0)
			ttx_print (" UC");
		if ((lp->flags & LOCF_MCAST) != 0)
			ttx_print (" MC");
		ttx_print ("\r\n");
#endif
		/* If the handle is setup correctly, use it as a shortcut
		   to the actual data connection. */
		if (lp->handle &&
		    (cxp = rtps_ip_from_handle (lp->handle)) != NULL) {
			ttx_print ("rtps_tcp_send: found from handle!\r\n");
			tcp_enqueue_msgs (cxp, msgs);
			return;
		}
		lp->handle = 0;

# if 0
		/* Shortcut to connection doesn't work, use locator data
		   to find data context if it exists. */
		if ((cxp = rtps_ip_lookup_peer (id, lp)) != NULL) {
			ttx_print ("rtps_tcp_send: found by peer lookup!\r\n");
			lp->handle = cxp->handle;
			tcp_enqueue_msgs (cxp, msgs);
			return;
		}
		ttx_print ("rtps_tcp_send: no such destination yet!\r\n");
# endif

		/* Data connection doesn't exist yet - create it and enqueue
		   the messages. */
		tcp_setup_data (id, lp, msgs);
	}
}

/* rtps_tcp_cleanup_cx -- Release all resources related to the given connection
                          (and potential paired and client connections. */

void rtps_tcp_cleanup_cx (IP_CX *cxp)
{
	tcp_close_fd (cxp);
}

/* rtps_tcp_mcast_locator_get -- Setup a TCP Meta Multicast locator for a given
				 domain participant. */

static Locator_t *rtps_tcp_mcast_locator_get (unsigned id, Locator_t *lp)
{
	Domain_t	*dp;
	DDS_ReturnCode_t error;

	memset (lp, 0, sizeof (Locator_t));
	lp->kind = LOCATOR_KIND_TCPv4;
	memcpy (lp->address + 12, tcp_mcast_ip, 4);
	dp = domain_get (id, 0, &error);
	if (!dp)
		return (NULL);

	lp->port = tcp_v4_pars.port_pars.pb +
	           tcp_v4_pars.port_pars.dg * dp->domain_id +
	           tcp_v4_pars.port_pars.d0;
	lp->flags = LOCF_META | LOCF_MCAST;
	return (lp);
}

/* rtps_tcp_add_mcast_locator -- Add the predefined TCP Meta Multicast locator. */

void rtps_tcp_add_mcast_locator (Domain_t *dp)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;

	foreach_locator (dp->dst_locs, rp, np)
		if ((np->locator.kind & LOCATOR_KINDS_TCP) != 0) {
			rtps_tcpv4_add_locator (dp->domain_id, np, dp->index, 0);
			break;
		}
}

/* rtps_tcp_update -- Update the TCP addresses. */

static unsigned rtps_tcp_update (LocatorKind_t kind, Domain_t *dp, int done)
{
	IP_CX		*cxp;
	unsigned	n;
	Locator_t	mc_loc;

	n = rtps_ip_update (kind, dp, done);
	if (!dp)
		return (0);

	rtps_tcp_mcast_locator_get (dp->index, &mc_loc);
	cxp = rtps_ip_lookup (dp->index, &mc_loc);
	if (!done) {	/* Mark multicast TCP locator as non-redundant for now. */
		if (cxp)
			cxp->redundant = 0;
	}
	else if (cxp && n == 1)	{ /* Multicast TCP locator is the only remaining! */
		rtps_ip_rem_locator (cxp->id, cxp->locator);
		n--;
	}
	else if (!cxp && n)
		rtps_tcp_add_mcast_locator (dp);

	VALIDATE_CXS ();
	return (n);
}

/* rtps_tcp_peer -- Get the neighbour GUID prefix for the given domain id and
		    participant id. */

int rtps_tcp_peer (unsigned     handle,
		   DomainId_t   domain_id,
		   unsigned     pid,
		   GuidPrefix_t *prefix)
{
	IP_CX		*cxp;

	if ((cxp = rtps_ip_from_handle (handle)) != NULL) {
		*prefix = cxp->parent->dst_prefix;
		guid_finalize (prefix, domain_id, pid);
		return (0);
	}
	return (DDS_RETCODE_ALREADY_DELETED);
}

static RTPS_TRANSPORT rtps_tcpv4 = {
	LOCATOR_KIND_TCPv4,
	rtps_tcpv4_init,
	rtps_tcpv4_final,
	rtps_tcp_pars_set,
	rtps_tcp_pars_get,
	rtps_tcpv4_locators_get,
	rtps_tcpv4_add_locator,
	rtps_tcpv4_rem_locator,
	rtps_tcp_update,
	rtps_ip_send
};

static IP_CTRL	rtps_tcpv4_control = {
	&rtps_tcpv4,
	rtps_tcpv4_enable,
	rtps_tcpv4_disable
};

/* rtps_tcpv4_attach -- Attach the TCPv4 protocol in order to send RTPS over
			TCPv4 messages. */

int rtps_tcpv4_attach (void)
{
	int	error;

	act_printf ("rtps_tcpv4_attach()\r\n");
	
	error = rtps_transport_add (&rtps_tcpv4);
	if (error)
		return (error);

	if (!tcp_v4_pars.port_pars.pb ||
	    !tcp_v4_pars.port_pars.dg ||
	    !tcp_v4_pars.port_pars.pg)
		rtps_parameters_set (LOCATOR_KIND_TCPv4, NULL);

	ipv4_proto.control [IPK_TCP] = &rtps_tcpv4_control;
	ipv4_proto.nprotos++;

	ip_attached |= LOCATOR_KIND_TCPv4;

	if (ipv4_proto.enabled)
		rtps_tcpv4_enable ();

	return (DDS_RETCODE_OK);
}

/* rtps_tcpv4_detach -- Detach the previously attached TCPv4 protocol. */

void rtps_tcpv4_detach (void)
{
	act_printf ("rtps_tcpv4_detach()\r\n");

	if (!ipv4_proto.control [IPK_TCP])
		return;

	if (tcp_v4_pars.enabled)
		rtps_tcpv4_disable ();

	ipv4_proto.control [IPK_TCP] = NULL;
	ipv4_proto.nprotos--;
	ip_attached &= ~LOCATOR_KIND_TCPv4;
	if (server_args && server_args != server_buf)
		xfree (server_args);
	server_args = NULL;
	if (public_args && public_args != public_buf)
		xfree (public_args);
	public_args = NULL;

#ifdef DDS_SECURITY
	if ((ip_attached & LOCATOR_KINDS_TCP) == 0)
		rtps_tls_finish ();
#endif
	rtps_transport_remove (&rtps_tcpv4);
}

#ifdef DDS_IP
	if (tcp_v6_pars.sport) {

		/* Start server. */
		ret = rtps_tcp_server_start (AF_INET6);
		if (ret)
			return (ret);
	}
	if (tcp_v6_pars.sport)
		rtps_tcp_server_stop (AF_INET6);

#endif
#else

int tcp_available = 0;

#endif

