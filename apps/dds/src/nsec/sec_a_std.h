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

/* sec_a_std.h -- Standard, i.e. default security plugin for the PKI-RSA and
		  DSA-DH security methods. */

#ifndef __sec_a_std_h_
#define __sec_a_std_h_

int sec_auth_add_pki_rsa (void);

/* Install the PKI-RSA authentication/encryption plugin code. */

int sec_auth_add_dsa_dh (void);

/* Install the DSA-DH authentication/encryption plugin code. */

DDS_ReturnCode_t aps_get_kx (SharedSecret_t secret,
			     unsigned char  *kx_key,
			     unsigned char  *kx_mac_key);

/* Get the encryption/decryption and HMAC keys. */

#endif /* !__sec_a_std_h_ */


