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

#ifndef FORWARDER_H_
#define FORWARDER_H_

#include <qeocore/api.h>
#include "core.h"

qeo_retcode_t fwd_init_pre_auth(qeo_factory_t *factory);
qeo_retcode_t fwd_init_post_auth(qeo_factory_t *factory);

qeo_retcode_t fwd_server_reconfig(qeo_factory_t *factory,
                                  const char *ip_address,
                                  int port);


void fwd_destroy(qeo_factory_t *factory);

#endif /* FORWARDER_H_ */
