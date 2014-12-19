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

#ifndef LIST_H_
#define LIST_H_

#include <stdint.h>
#include <qeo/error.h>

typedef struct list_s list_t;

list_t *list_new(void);

void list_free(list_t *list);

int list_length(const list_t *list);

qeo_retcode_t list_add(list_t *list,
                       void *data);

qeo_retcode_t list_remove(list_t *list,
                          const void *data);

typedef enum {
    LIST_ITERATE_CONTINUE,      /**< continue iteration */
    LIST_ITERATE_ABORT,         /**< abort iteration */
    LIST_ITERATE_DELETE,        /**< delete item from list */
} list_iterate_action_t;

typedef list_iterate_action_t (*list_iterate_callback)(void *data,
                                                       uintptr_t userdata);

qeo_retcode_t list_foreach(list_t *list,
                           list_iterate_callback cb,
                           uintptr_t userdata);

#endif /* LIST_H_ */
