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

#include <stdint.h>
#include "test.h"

static DDS_TypeSupport_meta tst1_msg_tsm [] = {
        { CDR_TYPECODE_STRUCT,   1, "Tst1Msg", sizeof (struct tst1_msg_st), 0, 4, 0, NULL },
	{ CDR_TYPECODE_OCTET,    1, "c1", 0, offsetof (struct tst1_msg_st, c1), 0, 0, NULL },
        { CDR_TYPECODE_LONGLONG, 1, "ll", 0, offsetof (struct tst1_msg_st, ll), 0, 0, NULL },
        { CDR_TYPECODE_OCTET,    1, "c2", 0, offsetof (struct tst1_msg_st, c2), 0, 0, NULL },
        { CDR_TYPECODE_SEQUENCE, 0, "data", 0, offsetof (struct tst1_msg_st, data), 0, 0, NULL },
        { CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL }
};

static DDS_TypeSupport_meta tst2_msg_tsm [] = {
        { CDR_TYPECODE_STRUCT,   1, "Tst2Msg", sizeof (struct tst2_msg_st), 0, 5, 0, NULL },
	{ CDR_TYPECODE_OCTET,    1, "c1", 0, offsetof (struct tst2_msg_st, c1), 0, 0, NULL },
        { CDR_TYPECODE_LONG,     1, "w", 0, offsetof (struct tst2_msg_st, w), 0, 0, NULL },
        { CDR_TYPECODE_CSTRING,  1, "n", 21, offsetof (struct tst2_msg_st, n), 5, 0, NULL },
        { CDR_TYPECODE_SHORT,    1, "s", 0, offsetof (struct tst2_msg_st, s), 0, 0, NULL },
        { CDR_TYPECODE_SEQUENCE, 0, "data", 0, offsetof (struct tst2_msg_st, data), 0, 0, NULL },
        { CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL }
};

static DDS_TypeSupport_meta tst3_msg_tsm [] = {
        { CDR_TYPECODE_STRUCT,   1, "Tst3Msg", sizeof (struct tst3_msg_st), 0, 6, 0, NULL },
	{ CDR_TYPECODE_OCTET,    1, "c1", 0, offsetof (struct tst3_msg_st, c1), 0, 0, NULL },
        { CDR_TYPECODE_SHORT,    1, "s", 0, offsetof (struct tst3_msg_st, s), 0, 0, NULL },
        { CDR_TYPECODE_LONG,     1, "l", 0, offsetof (struct tst3_msg_st, l), 0, 0, NULL },
        { CDR_TYPECODE_CSTRING,  1, "n", 13, offsetof (struct tst3_msg_st, n), 5, 0, NULL },
        { CDR_TYPECODE_LONGLONG, 1, "ll", 0, offsetof (struct tst3_msg_st, ll), 0, 0, NULL },
        { CDR_TYPECODE_SEQUENCE, 0, "data", 0, offsetof (struct tst3_msg_st, data), 0, 0, NULL },
        { CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL }
};

static DDS_TypeSupport_meta tst4_msg_tsm [] = {
        { CDR_TYPECODE_STRUCT,   1, "Tst4Msg", sizeof (struct tst4_msg_st), 0, 4, 0, NULL },
	{ CDR_TYPECODE_OCTET,    1, "c1", 0, offsetof (struct tst4_msg_st, c1), 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING,	 1, "n", 0, offsetof (struct tst4_msg_st, n), 0, 0, NULL },
        { CDR_TYPECODE_SHORT,    1, "s", 0, offsetof (struct tst4_msg_st, s), 0, 0, NULL },
        { CDR_TYPECODE_SEQUENCE, 0, "data", 0, offsetof (struct tst4_msg_st, data), 0, 0, NULL },
        { CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL }
};


static DDS_TypeSupport *tst1_msg_ts, *tst2_msg_ts, *tst3_msg_ts, *tst4_msg_ts;

DDS_ReturnCode_t Tst1MsgTypeSupport_register_type (
			DDS_DomainParticipant part,
			const char *type_name)
{
	DDS_ReturnCode_t	error;

	tst1_msg_ts = DDS_DynamicType_register (tst1_msg_tsm);
        if (!tst1_msg_ts)
                return (DDS_RETCODE_ERROR);

        error = DDS_DomainParticipant_register_type (part, tst1_msg_ts, type_name);
        return (error);
}

DDS_ReturnCode_t Tst2MsgTypeSupport_register_type (
			DDS_DomainParticipant part,
			const char *type_name)
{
	DDS_ReturnCode_t	error;

	tst2_msg_ts = DDS_DynamicType_register (tst2_msg_tsm);
        if (!tst2_msg_ts)
                return (DDS_RETCODE_ERROR);

        error = DDS_DomainParticipant_register_type (part, tst2_msg_ts, type_name);
        return (error);
}

DDS_ReturnCode_t Tst3MsgTypeSupport_register_type (
			DDS_DomainParticipant part,
			const char *type_name)
{
	DDS_ReturnCode_t	error;

	tst3_msg_ts = DDS_DynamicType_register (tst3_msg_tsm);
        if (!tst3_msg_ts)
                return (DDS_RETCODE_ERROR);

        error = DDS_DomainParticipant_register_type (part, tst3_msg_ts, type_name);
        return (error);
}

DDS_ReturnCode_t Tst4MsgTypeSupport_register_type (
			DDS_DomainParticipant part,
			const char *type_name)
{
	DDS_ReturnCode_t	error;

	tst4_msg_ts = DDS_DynamicType_register (tst4_msg_tsm);
        if (!tst4_msg_ts)
                return (DDS_RETCODE_ERROR);

        error = DDS_DomainParticipant_register_type (part, tst4_msg_ts, type_name);
        return (error);
}

void test_unregister_types (void)
{
	DDS_DynamicType_free (tst1_msg_ts);
	DDS_DynamicType_free (tst2_msg_ts);
	DDS_DynamicType_free (tst3_msg_ts);
	DDS_DynamicType_free (tst4_msg_ts);
}

