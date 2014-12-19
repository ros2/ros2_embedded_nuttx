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

#include "test.h"
#include "ta_type.h"

DDS_TypeSupport dds_HelloWorld_ts;

static DDS_TypeSupport_meta msg_data_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 1, "HelloWorldData", sizeof (struct msg_data_st), 0, 3, 0, NULL },
	{ CDR_TYPECODE_ULONGLONG,  0, "counter", 0, offsetof (struct msg_data_st, counter), 0, 0, NULL },
	{ CDR_TYPECODE_ARRAY,  1, "key", sizeof (unsigned [5]), offsetof (struct msg_data_st, key), 5, 0, NULL },
	{ CDR_TYPECODE_ULONG,  1, NULL, 0, 0, 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 0, "message", 200, offsetof (struct msg_data_st, message), 0, 0, NULL }
};

DDS_ReturnCode_t new_HelloWorldData_type (void)
{
	dds_HelloWorld_ts = DDS_DynamicType_register (msg_data_tsm);
        if (!dds_HelloWorld_ts)
                return (DDS_RETCODE_ERROR);
	else
		return (DDS_RETCODE_OK);
}

void free_HelloWorldData_type (void)
{
	DDS_DynamicType_free (dds_HelloWorld_ts);
	dds_HelloWorld_ts = NULL;
}

DDS_ReturnCode_t attach_HelloWorldData_type (DDS_DomainParticipant part)
{
	return (DDS_DomainParticipant_register_type (part, dds_HelloWorld_ts, TYPE_NAME));
}

DDS_ReturnCode_t detach_HelloWorldData_type (DDS_DomainParticipant part)
{
	return (DDS_DomainParticipant_unregister_type (part, dds_HelloWorld_ts, TYPE_NAME));
}

DDS_ReturnCode_t register_HelloWorldData_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	error = new_HelloWorldData_type ();
	if (error != DDS_RETCODE_OK)
		return (error);

	error = DDS_DomainParticipant_register_type (part, dds_HelloWorld_ts, TYPE_NAME);
	return (error);
}

void unregister_HelloWorldData_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	error = DDS_DomainParticipant_unregister_type (part, dds_HelloWorld_ts, TYPE_NAME);
	if (error) {
		printf ("DDS_DomainParticipant_unregister_type() failed! (error=%u)\r\n", error);
		return;
	}
	free_HelloWorldData_type ();
}

void test_type (void)
{
	DDS_DomainParticipant	part;
	const char		*cp;
	DDS_ReturnCode_t	error;

	dbg_printf ("Type functions ");
	if (trace)
		fflush (stdout);

	part = DDS_DomainParticipantFactory_create_participant (
						30, NULL, NULL, 0);
	fail_unless (part != NULL);
	v_printf ("\r\n - Test type registration.\r\n");
	error = register_HelloWorldData_type (part);
	fail_unless (error == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (part);
	v_printf (" - Test type release/re-registration.\r\n");
	error = register_HelloWorldData_type (part);
	fail_unless (error == DDS_RETCODE_OK);
	v_printf (" - Test type registration in domain.\r\n");
	error = DDS_DomainParticipant_delete_typesupport (part, dds_HelloWorld_ts);
	fail_unless (error == DDS_RETCODE_OK);
	error = register_HelloWorldData_type (part);
	fail_unless (error == DDS_RETCODE_OK);
	v_printf (" - Test type name retrieval.\r\n");
	cp = DDS_TypeSupport_get_type_name (dds_HelloWorld_ts);
	fail_unless (!strcmp (cp, TYPE_NAME));
	unregister_HelloWorldData_type (part);
	delay ();
	error = DDS_DomainParticipantFactory_delete_participant (part);
	fail_unless (error == DDS_RETCODE_OK);

	dbg_printf (" - success!\n");
}


