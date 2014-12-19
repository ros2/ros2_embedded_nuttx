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


/* Macros */

#define i2d_pkcs7_issuer_and_subject_bio(bp, ias) \
	ASN1_i2d_bio((i2d_of_void *) i2d_pkcs7_issuer_and_subject, bp, (unsigned char *)ias)
#define i2d_PKCS7_ISSUER_AND_SERIAL_bio(bp, ias)  \
	ASN1_i2d_bio(i2d_PKCS7_ISSUER_AND_SERIAL, bp, (unsigned char *)ias)

/* Routines */
int i2d_pkcs7_issuer_and_subject(pkcs7_issuer_and_subject *, unsigned char **);
pkcs7_issuer_and_subject *
d2i_pkcs7_issuer_and_subject(pkcs7_issuer_and_subject **, unsigned char **,
	long length);
pkcs7_issuer_and_subject *pkcs7_issuer_and_subject_new(void);
void pkcs7_issuer_and_subject_free(pkcs7_issuer_and_subject *);


