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

#ifndef __ta_data_h_
#define	__ta_data_h_

DDS_DomainParticipant data_prologue (void);

/* Setup 3 participants with multiple Writers and Readers per participant.
   Returns the first created participant. */

void data_epilogue (void);

/* Cleanup the 3 participant scenario. */

void data_xtopic_add (void);

/* Add an extra topic + reader in the last participant. */

void data_xtopic_remove (void);

/* Remove the extra topic type. */

extern void test_data (void);

#endif /* !__ta_data_h_ */

