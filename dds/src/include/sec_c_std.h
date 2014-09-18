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

/* sec_c_std.h -- Support for Standard DDS security.
		  Crypto handling plugin definitions. */

#ifndef __sec_c_std_h_
#define __sec_c_std_h_

/* Transformation identifier types for standard operation: */
#define	HMAC_SHA1		{ 0, 0, 1, 0 }
#define	HMAC_SHA256		{ 0, 0, 1, 1 }
#define	AES128_HMAC_SHA1	{ 0, 0, 2, 0 }
#define	AES256_HMAC_SHA256	{ 0, 0, 2, 1 }

extern const char *crypt_std_str [];

int sec_crypto_add_std (void);

/* Add the standard crypto plugin. */

#endif /* !__sec_c_std_h_ */

