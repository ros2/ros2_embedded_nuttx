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

#define _GNU_SOURCE
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <platform_api/platform_api.h>

#include <qeo/api.h>
#include "verbose.h"

#define log_self(fmt, ...) log_verbose("%" PRIx64 " - " fmt, _self, ##__VA_ARGS__)

typedef struct {
    int64_t key;
} test_type_t;

const DDS_TypeSupport_meta _tsm[] =
{
   { .tc = CDR_TYPECODE_STRUCT, .name = "test.type",
     .flags = TSMFLAG_MUTABLE|TSMFLAG_GENID|TSMFLAG_KEY, .size = sizeof(test_type_t), .nelem = 1 },
   { .tc = CDR_TYPECODE_LONGLONG, .name = "key", .flags = TSMFLAG_KEY, .offset = offsetof(test_type_t, key) },
};

static sem_t _pol_sync;
static sem_t _data_sync;

#define USER_FIRST 0x106
#define USER_LAST 0x107
#define USER_INVALID 0x108

static int _num = 3;
static int _rd_policy_updates = 0;
static int _wr_policy_updates = 0;

static int64_t _other = 0;
static int64_t _self = 0;

static char _storage_path[PATH_MAX];

static qeo_policy_perm_t get_perm(int64_t other,
                                  bool reverse /* use reverse dir */)
{
    qeo_policy_perm_t perm = QEO_POLICY_DENY;

    perm = (other == _other ? QEO_POLICY_ALLOW : QEO_POLICY_DENY);
    log_self("policy update for uid %" PRIx64 " -> : %s",
            other, (QEO_POLICY_ALLOW == perm ? "allow" : "deny"));
    return perm;
}

static qeo_policy_perm_t on_policy_update(const qeo_policy_identity_t *identity,
                                          int *policy_updates,
                                          bool reverse)
{
    qeo_policy_perm_t perm = QEO_POLICY_DENY;

    if (NULL == identity) {
        /* end-of-update */
        sem_post(&_pol_sync);
    }
    else {
        int64_t uid = qeo_policy_identity_get_uid(identity);

        perm = get_perm(uid, reverse);
        (*policy_updates)++;
    }
    return perm;
}

/**
 * If going forward allow reading from the one with UID one lower.
 * If going backward allow reading from the one with UID one higher.
 */
static qeo_policy_perm_t reader_on_policy_update(const qeo_state_change_reader_t *reader,
                                                 const qeo_policy_identity_t *identity,
                                                 uintptr_t userdata)
{
    log_self("reader policy update for %" PRIx64 ": %" PRIx64, _self, qeo_policy_identity_get_uid(identity));
    return on_policy_update(identity, &_rd_policy_updates, false);
}

/**
 * If going forward allow writing to the one with UID one higher.
 * If going backward allow writing to the one with UID one lower..
 */
static qeo_policy_perm_t writer_on_policy_update(const qeo_state_writer_t *writer,
                                                 const qeo_policy_identity_t *identity,
                                                 uintptr_t userdata)
{
    log_self("writer policy update for %" PRIx64 ": %" PRIx64, _self, qeo_policy_identity_get_uid(identity));
    return on_policy_update(identity, &_wr_policy_updates, true);
}

static void on_data(const qeo_state_change_reader_t *reader,
                    const void *data,
                    uintptr_t userdata)
{
    test_type_t *tt = (test_type_t *)data;

    assert(USER_INVALID != _self);
    log_self("got %" PRIx64 " expected %" PRIx64, tt->key, _other);
    assert(_other == tt->key);
    sem_post(&_data_sync);
}

static void set_self(int64_t self)
{
    _self = USER_FIRST + self - 1;
    if (_self != USER_INVALID) {
        _other = (USER_FIRST == _self ? USER_LAST : USER_FIRST);
    }
}

static void init_home_qeo(int64_t self)
{
    char *dir;

    dir = getenv("QEO_STORAGE_DIR");
    if (NULL == dir) {
        fprintf(stderr, "error: QEO_STORAGE_DIR not set\n");
        abort();
    }
    assert(-1 != snprintf(_storage_path, sizeof(_storage_path),
                          "%s/user_%" PRId64 "/", dir, self));
    qeo_platform_set_device_storage_path(_storage_path);
}

static void run_test()
{
    qeo_factory_t *factory;
    qeo_state_change_reader_t *reader;
    qeo_state_change_reader_listener_t r_cbs = {
        .on_data = on_data,
        .on_policy_update = reader_on_policy_update
    };
    qeo_state_writer_t *writer;
    qeo_state_writer_listener_t w_cbs = {
        .on_policy_update = writer_on_policy_update
    };
    test_type_t tt = { .key = _self };

    log_self("start application for user %" PRIx64, _self);
    /* initialize */
    sem_init(&_pol_sync, 0, 0);
    sem_init(&_data_sync, 0, 0);
    assert(NULL != (factory = qeo_factory_create()));
    assert(NULL != (writer = qeo_factory_create_state_writer(factory, _tsm, &w_cbs, 0)));
    assert(NULL != (reader = qeo_factory_create_state_change_reader(factory, _tsm, &r_cbs, 0)));

    /* wait for initial policy update (reader and writer) */
    if (_self != USER_INVALID) {
        sem_wait(&_pol_sync);
        sem_wait(&_pol_sync);
        log_self("reader/writer created and policies updated");
    }
    _rd_policy_updates = _wr_policy_updates = 0;
    /* write data */
    sleep(2);
    log_self("writing data");
    assert(QEO_OK == qeo_state_writer_write(writer, &tt));
    /* wait for data reception */
    if (_self != USER_INVALID) {
        sem_wait(&_data_sync);
        log_self("forward data received");
    }
    else {
        /* sleep for about 10 seconds and check that nothing has been received */
        sleep(10);
    }

    /* clean up */
    sleep(2);
    qeo_state_change_reader_close(reader);
    qeo_state_writer_close(writer);
    qeo_factory_close(factory);
    sem_destroy(&_data_sync);
    sem_destroy(&_pol_sync);
}

int main(int argc, const char **argv)
{
    pid_t child[_num];
    int status;

    /* fork processes */
    for (int i = 1; i <= _num; i++) {
        child[i] = fork();
        assert(-1 != child[i]);
        if (0 == child[i]) {
            set_self(i);
            init_home_qeo(i);
            run_test();
            return 0;
        }
    }
    /* wait for termination */
    for (int i = 1; i <= _num; i++) {
        assert(-1 != wait(&status));
        assert(0 == status);
    }
    return 0;
}
