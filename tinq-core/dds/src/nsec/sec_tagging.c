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

/* sec_tagging.h -- Implements the security tagging functionality. */

#include <stdio.h>
#include "log.h"
#include "error.h"
#include "nsec.h"

#ifdef DDS_SECURITY

DDS_ReturnCode_t DDS_DataWriter_add_data_tag (DDS_DataWriter w,
					      const char     *tag)
{
	return (DDS_DataWriter_add_property (w, "Secure_DDS_Tag", tag));
}

DDS_ReturnCode_t DDS_DataReader_get_data_tag (DDS_DataReader r,
					      char **tag,
					      DDS_InstanceHandle handle)
{
	return (DDS_DataReader_get_property (r, handle, "Secure_DDS_Tag", tag));
}

#endif

