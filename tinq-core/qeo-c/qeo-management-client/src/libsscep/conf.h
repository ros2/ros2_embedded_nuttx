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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *   Copyright (c) Jarkko Turkulainen 2003. All rights reserved. 
 *   See the 'sscep License' chapter in the file COPYRIGHT for copyright notice
 *   and original licensing information.
 */


/* Network timeout */
#define	TIMEOUT		120

/* Polling interval seconds */
#define	POLL_TIME	6

/* Max polling seconds */
#define	MAX_POLL_TIME	60

/* Max polling count */
#define	MAX_POLL_COUNT	10

/* CA identifier */
#define	CA_IDENTIFIER	""

/* Self signed certificate expiration */
#define SELFSIGNED_EXPIRE_DAYS	365

/* Transaction id for GetCert and GetCrl methods */
#define TRANS_ID_GETCERT	"SSCEP transactionId"

