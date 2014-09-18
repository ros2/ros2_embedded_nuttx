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

#ifndef __ta_disc_h_
#define	__ta_disc_h_

#define	NBUILTINS 4	/* Max. # of builtin Discovery Readers. */

typedef enum {		/* Discovery Reader type. */
	Participant,
	Topic,
	Pub,
	Sub
} Builtin_t;

extern const char *builtin_names [];
extern DDS_InstanceHandle_t last_d0 [];
extern DDS_DataReaderListener_on_data_available builtin_data_avail [];

#endif /* !__ta_disc_h_ */

