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

/* dynamic.h -- Dynamic shapes type handler. */

#ifndef __dynamic_h_
#define __dynamic_h_

void register_dynamic_type (DDS_DomainParticipant part);

/* Register a dynamic shapes type. */

void unregister_dynamic_type (void);

/* Unregister and delete the dynamic type. */

unsigned dynamic_writer_create (const char *topic);

/* Create a dynamic shapes writer. */

void dynamic_writer_delete (unsigned h);

/* Delete a dynamic shapes writer. */

void dynamic_writer_write (unsigned h, const char *color, unsigned x, unsigned y);

/* Write sample data on a dynamic writer. */

unsigned dynamic_reader_create (const char *topic);

/* Create a dynamic shapes reader. */

void dynamic_reader_delete (unsigned h);

/* Delete a dynamic shapes reader. */

#endif /* !__dynamic_h_ */

