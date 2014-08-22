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

#ifndef __sp_db_h_
#define __sp_db_h_

#include "msecplug/msecplug.h"

extern struct ms_domains_st	domains;
extern MSDomain_t		*domain_handles [MAX_DOMAINS];
extern unsigned			num_domains;

extern ENGINE			*engines [MAX_ENGINES];
extern int			engine_counter;

extern struct ms_participants_st participants;
extern MSParticipant_t		*id_handles [MAX_ID_HANDLES];
extern unsigned			num_ids;

#endif /* !__sp_db_h_ */

