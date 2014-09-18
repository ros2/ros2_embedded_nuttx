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

/* sec_disc.h -- DDS Security - Discovery types and data structure definitions. */

#ifndef __sec_disc_h_
#define __sec_disc_h_

#define	KIND_SEC_AUTH_HANDSHAKE		{ 0x00, 0x01, 0x00, 0x01 }
#define	KIND_SEC_PART_CRYPTO_TOKENS	{ 0x00, 0x01, 0x00, 0x02 }
#define	KIND_SEC_PART_WRITER_TOKENS	{ 0x00, 0x01, 0x00, 0x03 }
#define	KIND_SEC_PART_READER_TOKENS	{ 0x00, 0x01, 0x00, 0x04 }

typedef struct participant_crypto_token_msg_st {
	GUID_t		sending_guid;
	GUID_t		receiving_guid;
	Tokens_t	crypto_tokens;
} ParcicipantCryptoTokensMsg;

typedef struct datawriter_crypto_token_msg_st {
	GUID_t		writer_guid;
	GUID_t		readerwriter_guid;
	Tokens_t	crypto_tokens;
} DatawriterCryptoTokensMsg;

typedef struct datareader_crypto_token_msg_st {
	GUID_t		readerwriter_guid;
	GUID_t		writer_guid;
	Tokens_t	crypto_tokens;
} DatareaderCryptoTokensMsg;

#endif /* !__sec_disc_h_ */

