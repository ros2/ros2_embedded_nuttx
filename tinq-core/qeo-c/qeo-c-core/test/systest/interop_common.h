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

#ifndef INTEROP_COMMON_H_
#define INTEROP_COMMON_H_

#include <semaphore.h>
#include <qeocore/api.h>

typedef struct {
    sem_t sync;
    qeocore_type_t *(*get_type)(qeo_factory_t *factory, const DDS_TypeSupport_meta *tsm);
    void *(*get_data)(const qeocore_writer_t *writer);
    void (*free_data)(void *data);
    void (*validate_data)(const qeocore_data_t *data, int key_only);
} vtable_t;

extern vtable_t _vtable_static;
extern vtable_t _vtable_dynamic;

void on_data_available(const qeocore_reader_t *reader,
                       const qeocore_data_t *data,
                       uintptr_t userdata);

void do_test(qeo_factory_t *factory,
             int flags,
             vtable_t *vtable);

#endif /* INTEROP_COMMON_H_ */
