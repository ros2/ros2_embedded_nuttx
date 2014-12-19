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

/*#######################################################################
#                       HEADER (INCLUDE) SECTION                        #
########################################################################*/

#include <stdlib.h>

#include "core.h"
#include "list.h"

/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/

typedef struct list_item_s list_item_t;

struct list_item_s {
    void *data;
    list_item_t *next;
};

struct list_s {
    list_item_t *head;
    int nelem;
};

/*#######################################################################
#                   STATIC FUNCTION DECLARATION                         #
########################################################################*/

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

list_t *list_new(void)
{
    list_t *list = NULL;

    list = calloc(1, sizeof(list_t));
    return list;
}

void list_free(list_t *list)
{
    if (NULL != list) {
        list_item_t *it = list->head;

        while (NULL != it) {
            list_item_t *next = it->next;

            free(it);
            it = next;
        }
        free(list);
    }
}

int list_length(const list_t *list)
{
    int nelem = -1;

    if (NULL != list) {
        nelem = list->nelem;
    }
    return nelem;
}

qeo_retcode_t list_add(list_t *list, void *data)
{
    qeo_retcode_t rc = QEO_OK;
    list_item_t *item = NULL;

    VALIDATE_NON_NULL(list);
    VALIDATE_NON_NULL(data);
    item = calloc(1, sizeof(list_item_t));
    if (NULL == item) {
        rc = QEO_ENOMEM;
    }
    else {
        item->data = data;
        item->next = list->head;
        list->head = item;
        list->nelem++;
    }
    return rc;
}

qeo_retcode_t list_remove(list_t *list, const void *data)
{
    qeo_retcode_t rc = QEO_EBADSTATE; /* not found */

    VALIDATE_NON_NULL(list);
    VALIDATE_NON_NULL(data);
    if (list->nelem > 0) {
        list_item_t *it = list->head;
        list_item_t *item = NULL;

        if (data == it->data) {
            list->head = it->next;
            item = it;
        }
        else {
            while (NULL != it->next) {
                if (data == it->next->data) {
                    item = it->next;
                    it->next = item->next;
                    break;
                }
                it = it->next;
            }
        }
        if (NULL != item) {
            free(item);
            list->nelem--;
            rc = QEO_OK;
        }
    }
    return rc;
}

qeo_retcode_t list_foreach(list_t *list,
                           list_iterate_callback cb,
                           uintptr_t userdata)
{
    list_iterate_action_t action;
    list_item_t *prev = NULL, *it;

    VALIDATE_NON_NULL(list);
    VALIDATE_NON_NULL(cb);
    it = list->head;
    while (NULL != it) {
        action = cb(it->data, userdata);
        if (LIST_ITERATE_ABORT == action) {
            break;
        }
        else if (LIST_ITERATE_DELETE == action) {
            if (NULL == prev) {
                /* remove head */
                list->head = it->next;
                free(it);
                it = list->head;
            }
            else {
                prev->next = it->next;
                free(it);
                it = prev->next;
            }
            list->nelem--;
        }
        else { /* LIST_ITERATE_CONTINUE */
            prev = it;
            it = it->next;
        }
    }
    return QEO_OK;
}
