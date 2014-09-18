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

#ifndef __dcps_pub_h_
#define __dcps_pub_h_

void delete_publisher_entities (Domain_t *dp, Publisher_t *up);

int dcps_update_writer_qos (Skiplist_t *list, void *node, void *arg);

void dcps_suspended_publication_add (Publisher_t *pp, Writer_t *wp, int new);

void dcps_suspended_publication_remove (Publisher_t *pp, Writer_t *wp);

#endif /* !__dcps_pub_h_ */

