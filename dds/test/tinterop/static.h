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

/* static.h -- Static shapes type handler. */

#ifndef __static_h_
#define __static_h_

void register_static_type (DDS_DomainParticipant part);

/* Register a static shapes type. */

void unregister_static_type (void);

/* Unregister and delete the static type. */

unsigned static_writer_create (const char *topic);

/* Create a static shapes writer. */

void static_writer_delete (unsigned h);

/* Delete a static shapes writer. */

void static_writer_write (unsigned h, const char *color, unsigned x, unsigned y);

/* Write sample data on a static writer. */

unsigned static_reader_create (const char *topic);

/* Create a static shapes reader. */

void static_reader_delete (unsigned h);

/* Delete a static shapes reader. */

#endif /* !__static_h_ */

