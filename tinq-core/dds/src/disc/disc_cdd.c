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

/* disc_cdd.h -- Implements the Central Discovery functions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "log.h"
#include "error.h"
#include "dds.h"
#include "dcps.h"
#include "disc.h"
#include "disc_cfg.h"
#include "disc_priv.h"
#include "disc_ep.h"
#include "disc_sedp.h"
#include "disc_cdd.h"

#ifdef SIMPLE_DISCOVERY
#endif

