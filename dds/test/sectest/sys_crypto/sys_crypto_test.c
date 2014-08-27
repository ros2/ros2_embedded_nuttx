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

/* This is not clean, but the most easy way to test static functions */
#include "../../apps/nsecplug/sp_sys_crypto.c"

static void print_hex (unsigned char *bs, unsigned int n)
{
	unsigned int i;
	
	for (i = 0; i < n; i++)
		printf ("%02x", bs [i]);

	printf ("\r\n");
}

int main (int argc, const char *argv [])
{
	unsigned char secret [32];
	unsigned char nonce [16];
	unsigned char hash1 [32];
	unsigned char hash2 [32];
	size_t        hash1_len = 0, hash2_len = 0;
	unsigned char *msg = "This is teststring with a very long long long ling lign lgin lgnisdlfkljdsn gl;ksjhdakjs gkljshadgk jsadglkjs hadkgj hsakljghsakjghlaskjgdh";
	unsigned char *badmsg = "This is a bad teststring ...";
	unsigned char *data = "To be encrypted ...!But is it really ...";
	unsigned      handle;
	size_t        data_len = 20;
	int           nb_parts = 2;
	unsigned char IV [16] = "1234567800000000";
	unsigned char IV2 [16] = "876543210000000";
	unsigned char encrypted [512];
	size_t        enc_len = 512;
	unsigned char decrypted [512];
	size_t        dec_len = 512;
	unsigned char hmac [32], hmac2 [32];
	size_t        hmac_len = 0, hmac2_len = 0;

	memset (encrypted, 0, enc_len);
	memset (decrypted, 0, dec_len);
	
	printf ("Using ssl version %s\r\n", SSLeay_version(SSLEAY_VERSION));	

	/* SHA256 Hash Testing */
	if (sp_sys_hash_sha256 (msg, strlen ((const char *)msg), &hash1 [0], &hash1_len)) {
		printf ("PROBLEM hashing");
		exit (1);
	} else {
		printf ("First hash was calculated:  ");
		print_hex (&hash1 [0], hash1_len);
	}
	if (sp_sys_hash_sha256 (msg, strlen ((const char *)msg), &hash2 [0], &hash2_len)) {
		printf ("PROBLEM hashing");
		exit (1);
	} else {
		printf ("Second hash was calculated: ");
		print_hex (&hash2 [0], hash2_len);
	}

	if (sp_sys_compare_hash (&hash1 [0], hash1_len, &hash2 [0], hash2_len)) {
		printf ("PROBLEM comparing hashes\r\n");
		exit (1);
	} else
		printf ("Hashes are the same\r\n");

	/* Create a bad hash */
	if (sp_sys_hash_sha256 (badmsg, strlen ((const char *)badmsg), &hash1 [0], &hash1_len)) {
		printf ("PROBLEM hashing");
		exit (1);
	} else {
		printf ("First hash was calculated: ");
		print_hex (&hash1 [0], hash1_len);
	}
	if (sp_sys_compare_hash (&hash1 [0], hash1_len, &hash2 [0], hash2_len)) 
		printf ("This fail is to be expected\r\n");
	else {
		printf ("PROBLEM Hashes are the same\r\n");
		exit (1);
	}

	/* SHA1 Hash Testing */
	if (sp_sys_hash_sha1 (msg, strlen ((const char *)msg), &hash1 [0], &hash1_len)) {
		printf ("PROBLEM hashing");
		exit (1);
	} else {
		printf ("First hash was calculated: ");
		print_hex (&hash1 [0], hash1_len);
	}
	if (sp_sys_hash_sha1 (msg, strlen ((const char *)msg), &hash2 [0], &hash2_len)) {
		printf ("PROBLEM hashing");
		exit (1);
	} else {
		printf ("Second hash was calculated: ");
		print_hex (&hash2 [0], hash2_len);
	}
	
	if (sp_sys_compare_hash (&hash1 [0], hash1_len, &hash2 [0], hash2_len)) {
		printf ("PROBLEM comparing hashes\r\n");
		exit (1);
	} else
		printf ("Hashes are the same\r\n");

	/* Create a bad hash */
	if (sp_sys_hash_sha1 (badmsg, strlen ((const char *)badmsg), &hash1 [0], &hash1_len)) {
		printf ("PROBLEM hashing");
		exit (1);
	} else {
		printf ("First hash was calculated: ");
		print_hex (&hash1 [0], hash1_len);
	}
	if (sp_sys_compare_hash (&hash1 [0], hash1_len, &hash2 [0], hash2_len)) 
		printf ("This fail is to be expected\r\n");
	else {
		printf ("PROBLEM Hashes are the same\r\n");
		exit (1);
	}
	if (sp_sys_generate_random (32, &secret [0])) {
		printf ("Problem generating shared secret\r\n");
		exit (1);
	} else {
		printf ("\r\nSecret is ");
		print_hex (&secret [0], 32);
	}
	
	if (sp_sys_generate_random (16, &nonce [0])) {
		printf ("Problem generating nonce\r\n");
		exit (1);
	} else {
		printf ("\r\nNonce is ");
		print_hex (&nonce [0], 16);
	}

	/*************************************/
	/* 128 bit encryption and decryption */
	/*************************************/

	/* Test 128 bit aes encryption */

	printf ("data is\r\n");
	print_hex (data, data_len * nb_parts);

	if ((sp_sys_aes_ctr_begin (IV, 0, &secret [0], 128))) {
		printf ("Problem initializing encryption structure\r\n");
		exit (1);
	}
	if ((sp_sys_aes_ctr_update (data, data_len, &encrypted [0], &enc_len))) {
		printf ("Problem encrypting");
		exit (1);
	}

	printf ("result after first 20 byte encryption\r\n");
	print_hex (encrypted, enc_len * nb_parts);
	if ((sp_sys_aes_ctr_update (&data [data_len], data_len, 
					    &encrypted [enc_len], &enc_len))) {
		printf ("Problem encrypting");
		exit (1);
	}
	printf ("result after second 20 byte encryption \r\n");
	print_hex (encrypted, enc_len * nb_parts);
	if ((sp_sys_aes_ctr_end ())) {
		printf ("Problem closing down encryption struct\r\n");
		exit (1);
	}
	enc_len = 20;
	printf ("Encryption succeeded\r\n");

	/* Test 128 bit aes decryption */

	if ((sp_sys_aes_ctr_begin (IV, 0, &secret [0], 128))) {
		printf ("Problem setting up decryption struct\r\n");
		exit (1);
	}
	if ((sp_sys_aes_ctr_update (&encrypted [0], enc_len, &decrypted [0], &dec_len))) {
		printf ("Problem decrypting\r\n");
		exit (1);
	}
	printf ("decrypted is \r\n");
	print_hex (decrypted, dec_len * nb_parts);

	if ((sp_sys_aes_ctr_update (&encrypted [dec_len], enc_len, 
					    &decrypted [dec_len], &dec_len))) {
		printf ("Problem decrypting");
		exit (1);
	}
	printf ("decrypted is \r\n");
	print_hex (decrypted, dec_len * nb_parts);

	if ((sp_sys_aes_ctr_end ())) {
		printf ("Problem closing down decryption struct\r\n");
		exit (1);
	}

	if (memcmp (data, decrypted, data_len * nb_parts)) {
		printf ("Encrypted and decrypted do not match\r\n");
		exit (1);
	}

	printf ("AES 128 bit decryption succeedded\r\n");

	/*************************************/
	/* 256 bit encryption and decryption */
	/*************************************/

	/* Test 256 bit aes encryption */

	memset (encrypted, 0, data_len * nb_parts);
	memset (decrypted, 0, data_len * nb_parts);
	enc_len = dec_len = 512;
	

	printf ("data is\r\n");
	print_hex (data, data_len * nb_parts);

	if ((sp_sys_aes_ctr_begin (IV2, 5509, &secret [0], 256))) {
		printf ("Problem initializing encryption structure\r\n");
		exit (1);
	}
	if ((sp_sys_aes_ctr_update (data, data_len, &encrypted [0], &enc_len))) {
		printf ("Problem encrypting");
		exit (1);
	}

	printf ("result after first 20 byte encryption\r\n");
	print_hex (encrypted, enc_len * nb_parts);
	if ((sp_sys_aes_ctr_update (&data [data_len], data_len, 
					    &encrypted [enc_len], &enc_len))) {
		printf ("Problem encrypting");
		exit (1);
	}
	printf ("result after second 20 byte encryption \r\n");
	print_hex (encrypted, enc_len * nb_parts);
	if ((sp_sys_aes_ctr_end ())) {
		printf ("Problem closing down encryption struct\r\n");
		exit (1);
	}
	enc_len = 20;
	printf ("Encryption succeeded\r\n");

	/* Test 256 bit aes decryption */

	if ((sp_sys_aes_ctr_begin (IV2, 5509, &secret [0], 256))) {
		printf ("Problem setting up decryption struct\r\n");
		exit (1);
	}
	if ((sp_sys_aes_ctr_update (&encrypted [0], enc_len, &decrypted [0], &dec_len))) {
		printf ("Problem decrypting\r\n");
		exit (1);
	}
	printf ("decrypted is \r\n");
	print_hex (decrypted, dec_len * nb_parts);

	if ((sp_sys_aes_ctr_update (&encrypted [dec_len], enc_len, 
					    &decrypted [dec_len], &dec_len))) {
		printf ("Problem decrypting");
		exit (1);
	}
	printf ("decrypted is \r\n");
	print_hex (decrypted, dec_len * nb_parts);

	if ((sp_sys_aes_ctr_end ())) {
		printf ("Problem closing down decryption struct\r\n");
		exit (1);
	}

	if (memcmp (data, decrypted, data_len * nb_parts)) {
		printf ("Encrypted and decrypted do not match\r\n");
		exit (1);
	}

	printf ("decryption succeedded\r\n");

	/*************/
	/* sha1 hmac */
	/*************/

	if (sp_sys_hmac_sha1_begin ("Thisisabadkey", 13)) {
		printf ("Hmac failed to initialize!");
		exit (1);
	}
	if (sp_sys_hmac_sha1_continue ("This is a string to check", 26)) {
		printf ("Hmac failed to continue\r\n");
		exit (1);
	}
	if (sp_sys_hmac_sha1_end (&hmac [0], &hmac_len)) {
		printf ("Hmac result failed\r\n");
		exit (1);
	}
	printf ("Hmac is \r\n");
	print_hex (hmac, hmac_len);

	/***************/
	/* sha256 hmac */
	/***************/

	if (sp_sys_hmac_sha256_begin ("Thisisabadkey", 13)) {
		printf ("Hmac failed to initialize!");
		exit (1);
	}
	if (sp_sys_hmac_sha256_continue ("This is a string to check", 26)) {
		printf ("Hmac failed to continue\r\n");
		exit (1);
	}
	if (sp_sys_hmac_sha256_end (&hmac2 [0], &hmac2_len)) {
		printf ("Hmac result failed\r\n");
		exit (1);
	}
	printf ("Hmac is \r\n");
	print_hex (hmac2, hmac2_len);

	/* Validate */

	if (sp_sys_hmac_sha256_begin ("Thisisabadkey", 13)) {
		printf ("Hmac failed to initialize!");
		exit (1);
	}
	if (sp_sys_hmac_sha256_continue ("This is a string to check", 26)) {
		printf ("Hmac failed to continue\r\n");
		exit (1);
	}
	if (sp_sys_hmac_sha256_end (&hmac [0], &hmac_len)) {
		printf ("Hmac result failed\r\n");
		exit (1);
	}

	printf ("Hmac is \r\n");
	print_hex (hmac, hmac_len);

	if ((memcmp (hmac, hmac2, hmac_len))) {
		printf ("Hmac do not verify\r\n");
		exit (1);
	}

	printf ("Test succeeded\r\n");
}
