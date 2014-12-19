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

#ifndef SSCEP_MOCK_H_
#define SSCEP_MOCK_H_
#include <stdbool.h>

#include "sscep_api.h"

#define SSCEP_MOCK_CHECK_CALLED 666

void sscep_mock_expect_called(int init, int perform, int clean);
void sscep_mock_ignore_and_return(bool init, int perform1, STACK_OF(X509) *certs1, int perform2, STACK_OF(X509) *certs2);

#endif /* SSCEP_MOCK_H_ */
