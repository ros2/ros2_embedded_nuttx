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


/*
 * Routines for pkcs7_issuer_and_subject data type needed when
 * sending GETCertInitial requests.
 */

#include "sscep.h"
#include "ias.h"

int i2d_pkcs7_issuer_and_subject(pkcs7_issuer_and_subject *a,
	     unsigned char **pp) {

	M_ASN1_I2D_vars(a);
	M_ASN1_I2D_len(a->issuer,i2d_X509_NAME);
	M_ASN1_I2D_len(a->subject,i2d_X509_NAME);
	M_ASN1_I2D_seq_total();
	M_ASN1_I2D_put(a->issuer,i2d_X509_NAME);
	M_ASN1_I2D_put(a->subject,i2d_X509_NAME);
	M_ASN1_I2D_finish();
}

pkcs7_issuer_and_subject *
d2i_pkcs7_issuer_and_subject(pkcs7_issuer_and_subject **a,
		unsigned char **pp, long length) {

	M_ASN1_D2I_vars(a, pkcs7_issuer_and_subject *,
		pkcs7_issuer_and_subject_new);
	M_ASN1_D2I_Init();
	M_ASN1_D2I_start_sequence();
	M_ASN1_D2I_get(ret->issuer,d2i_X509_NAME);
	M_ASN1_D2I_get(ret->subject,d2i_X509_NAME);
	M_ASN1_D2I_Finish(a,pkcs7_issuer_and_subject_free, 99);
}

pkcs7_issuer_and_subject *pkcs7_issuer_and_subject_new(void) {

	pkcs7_issuer_and_subject *ret=NULL;
	ASN1_CTX c;
	M_ASN1_New_Malloc(ret,pkcs7_issuer_and_subject);
	M_ASN1_New(ret->issuer,X509_NAME_new);
	M_ASN1_New(ret->subject,X509_NAME_new);
	return(ret);
	M_ASN1_New_Error(199);
}

void pkcs7_issuer_and_subject_free(pkcs7_issuer_and_subject *a) {

	if (a == NULL) return;
	X509_NAME_free(a->issuer);
	X509_NAME_free(a->subject);
	OPENSSL_free(a);
}

