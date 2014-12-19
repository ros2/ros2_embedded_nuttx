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

#ifndef USER_DATA_H_
#define USER_DATA_H_

#include <qeocore/api.h>

qeo_retcode_t reader_user_data_update(const qeocore_reader_t *reader);
qeo_retcode_t writer_user_data_update(const qeocore_writer_t *writer);

#endif /* USER_DATA_H_ */
