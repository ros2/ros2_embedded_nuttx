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

#ifndef DEVICEINFO_WRITER_H_
#define DEVICEINFO_WRITER_H_

#include <qeocore/api.h>

qeocore_writer_t* qeo_deviceinfo_publish(qeo_factory_t *factory);

void qeo_deviceinfo_destruct(qeocore_writer_t *devinfo_writer);


#endif /* DEVICEINFO_WRITER_H_ */
