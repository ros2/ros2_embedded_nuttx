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

/* test.h -- Interface to the test functionality. */

#ifndef __test_h_
#define	__test_h_

void simulate_reader (unsigned n, unsigned sleep_time);

/* Called regularly in writer mode to perform test sequences. */

void simulate_writer (unsigned n, unsigned sleep_time);

/* Called regularly in reader mode to perform test sequences. */

#endif /* !__test_h_ */

