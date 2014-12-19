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

#ifndef ENTITY_STORE_H_
#define ENTITY_STORE_H_

#include <qeo/error.h>
#include "core.h"

qeo_retcode_t entity_store_init(void);

qeo_retcode_t entity_store_add(entity_t *entity);

qeo_retcode_t entity_store_remove(const entity_t *entity);

qeo_retcode_t entity_store_fini(void);

qeo_retcode_t entity_store_update_user_data(const qeo_factory_t *factory);

#endif /* ENTITY_STORE_H_ */
