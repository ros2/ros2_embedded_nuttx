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


#ifndef COMMON_H_
#define COMMON_H_

#include <qeocore/factory.h>
#include <qeocore/api.h>

void init_factory(qeo_factory_t *factory);

int count_instances(qeocore_reader_t *reader);

#endif
