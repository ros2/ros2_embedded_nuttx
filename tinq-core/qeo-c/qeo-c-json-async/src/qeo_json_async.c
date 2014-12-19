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

/*########################################################################
#  HEADER (INCLUDE) SECTION                                             #
########################################################################*/
#ifndef DEBUG
#define NDEBUG
#endif
#include <qeo/jsonasync.h>
#include <qeojson/api.h>
#include <qeo/log.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <search.h>
#include <jansson.h>
#include <utlist.h>
#include <sys/types.h>
#include <sys/socket.h>

/*########################################################################
#  TYPES and DEFINES SECTION                                            #
########################################################################*/
#define QEO_JSON_ASYNC_MAGIC    0xac1dcafe

typedef enum {
    CMD_TYPE_CREATE,
    CMD_TYPE_CLOSE,
    CMD_TYPE_WRITE,
    CMD_TYPE_REMOVE,
    CMD_TYPE_POLICY_UPDATE,
    CMD_TYPE_POLICY_REQUEST,
    CMD_TYPE_GET,
    CMD_TYPE_NUM,
} qeo_json_async_cmd_type_t;

typedef struct {
    qeo_json_async_cmd_type_t cmd_type;
    char                      *options;
} qeo_json_async_cmd_t;

struct qeo_json_async_ctx_s {
#ifndef NDEBUG
    unsigned long             magic;
#endif
    uintptr_t                 userdata;
    int                       socket_pair[2];
    pthread_t                 async_thread;
    qeo_json_async_listener_t listener;
};

struct str_int_tuple_s {
    const char  *strval;
    uintptr_t   intval;
};


typedef qeo_retcode_t (*entity_action_cb)(uintptr_t   entity,
                                          const char  *data);

typedef uintptr_t (*entity_create_cb)(const qeo_json_async_ctx_t  *ctx,
                                      qeo_factory_t               *factory,
                                      const char                  *typeDesc,
                                      bool                        policyEnabled);

typedef qeo_retcode_t (*entity_enable_cb)(uintptr_t entity);

typedef bool (*qeo_json_async_dispatch_callback)(const qeo_json_async_ctx_t *ctx,
                                                 struct str_int_tuple_s     *id,
                                                 const json_t               *json);

typedef enum {
    OBJ_TYPE_FACTORY,
    OBJ_TYPE_EVENT_READER,
    OBJ_TYPE_EVENT_WRITER,
    OBJ_TYPE_STATE_READER,
    OBJ_TYPE_STATE_WRITER,
    OBJ_TYPE_STATECHANGE_READER,
    OBJ_TYPE_ITERATOR,
    OBJ_TYPE_DEVICE_ID,
    OBJ_TYPE_NUM,
} qeo_json_async_obj_type_t;

struct factory_user_data_s {
    const qeo_json_async_ctx_t  *ctx;
    char                        *id;
};

struct qeo_entity_el_s {
    uintptr_t               qeo_entity;
    uintptr_t               parent;
    struct qeo_entity_el_s  *next;
};

#define FACTORYSTR              "factory"
#define EVENTREADERSTR          "eventReader"
#define EVENTWRITERSTR          "eventWriter"
#define STATEREADERSTR          "stateReader"
#define STATEWRITERSTR          "stateWriter"
#define STATECHANGEREADERSTR    "stateChangeReader"
#define ITERATORSTR             "iterator"
#define DEVICEIDSTR             "deviceId"

#define IDSTR                   "id"
#define FACTORYIDSTR            "factoryId"
#define READERIDSTR             "readerId"
#define OBJTYPESTR              "objType"
#define TYPEDESCSTR             "typeDesc"
#define DATASTR                 "data"
#define POLICYSTR               "enablePolicy"
#define IDENTITYSTR             "identity"

#define CREATESTR               "create"
#define CLOSESTR                "close"
#define WRITESTR                "write"
#define REMOVESTR               "remove"
#define POLICY_UPDATESTR        "policyUpdate"
#define POLICY_REQUESTSTR       "requestPolicy"
#define GETSTR                  "get"

#define ERRORSTR                "error"
#define NOMOREDATASTR           "noMoreData"
#define UPDATESTR               "update"
#define ITERATESTR              "iterate"

/*########################################################################
#  STATIC FUNCTION PROTOTYPES                                           #
########################################################################*/

static bool create_factory(const qeo_json_async_ctx_t *ctx,
                           struct str_int_tuple_s     *id,
                           const json_t               *json);
static bool create_eventReader(const qeo_json_async_ctx_t *ctx,
                               struct str_int_tuple_s     *id,
                               const json_t               *json);
static bool create_eventWriter(const qeo_json_async_ctx_t *ctx,
                               struct str_int_tuple_s     *id,
                               const json_t               *json);
static bool create_stateReader(const qeo_json_async_ctx_t *ctx,
                               struct str_int_tuple_s     *id,
                               const json_t               *json);
static bool create_stateWriter(const qeo_json_async_ctx_t *ctx,
                               struct str_int_tuple_s     *id,
                               const json_t               *json);
static bool create_stateChangeReader(const qeo_json_async_ctx_t *ctx,
                                     struct str_int_tuple_s     *id,
                                     const json_t               *json);
static bool create_run_iterator(const qeo_json_async_ctx_t  *ctx,
                                struct str_int_tuple_s      *id,
                                const json_t                *json);

static bool close_factory(const qeo_json_async_ctx_t  *ctx,
                          struct str_int_tuple_s      *id,
                          const json_t                *json);
static bool close_eventReader(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json);
static bool close_eventWriter(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json);
static bool close_stateReader(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json);
static bool close_stateWriter(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json);
static bool close_stateChangeReader(const qeo_json_async_ctx_t  *ctx,
                                    struct str_int_tuple_s      *id,
                                    const json_t                *json);

static bool write_eventWriter(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json);
static bool write_stateWriter(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json);

static bool remove_stateWriter(const qeo_json_async_ctx_t *ctx,
                               struct str_int_tuple_s     *id,
                               const json_t               *json);

static bool update_policy_eventReader(const qeo_json_async_ctx_t  *ctx,
                                      struct str_int_tuple_s      *id,
                                      const json_t                *json);
static bool update_policy_eventWriter(const qeo_json_async_ctx_t  *ctx,
                                      struct str_int_tuple_s      *id,
                                      const json_t                *json);
static bool update_policy_stateReader(const qeo_json_async_ctx_t  *ctx,
                                      struct str_int_tuple_s      *id,
                                      const json_t                *json);
static bool update_policy_stateWriter(const qeo_json_async_ctx_t  *ctx,
                                      struct str_int_tuple_s      *id,
                                      const json_t                *json);
static bool update_policy_stateChangeReader(const qeo_json_async_ctx_t  *ctx,
                                            struct str_int_tuple_s      *id,
                                            const json_t                *json);

static bool request_policy_eventReader(const qeo_json_async_ctx_t *ctx,
                                       struct str_int_tuple_s     *id,
                                       const json_t               *json);
static bool request_policy_eventWriter(const qeo_json_async_ctx_t *ctx,
                                       struct str_int_tuple_s     *id,
                                       const json_t               *json);
static bool request_policy_stateReader(const qeo_json_async_ctx_t *ctx,
                                       struct str_int_tuple_s     *id,
                                       const json_t               *json);
static bool request_policy_stateWriter(const qeo_json_async_ctx_t *ctx,
                                       struct str_int_tuple_s     *id,
                                       const json_t               *json);
static bool request_policy_stateChangeReader(const qeo_json_async_ctx_t *ctx,
                                             struct str_int_tuple_s     *id,
                                             const json_t               *json);
static bool get_deviceId(const qeo_json_async_ctx_t *ctx,
                         struct str_int_tuple_s     *id,
                         const json_t               *json);

static void callback_factory_init_done(qeo_factory_t  *factory,
                                       uintptr_t      userdata);

static void callback_event_reader_on_data(const qeo_json_event_reader_t *reader,
                                          const char                    *json_data,
                                          uintptr_t                     userdata);
static void callback_event_reader_no_more_data(const qeo_json_event_reader_t  *reader,
                                               uintptr_t                      userdata);
static void callback_event_reader_on_policy_update(const qeo_json_event_reader_t  *reader,
                                                   const char                     *json_data,
                                                   uintptr_t                      userdata);

static void callback_state_reader_update_callback(const qeo_json_state_reader_t *reader,
                                                  uintptr_t                     userdata);
static void callback_state_reader_on_policy_update(const qeo_json_state_reader_t  *reader,
                                                   const char                     *json_policy,
                                                   uintptr_t                      userdata);

static void callback_state_change_reader_on_data(const qeo_json_state_change_reader_t *reader,
                                                 const char                           *json_data,
                                                 uintptr_t                            userdata);
static void callback_state_change_reader_on_no_more_data(const qeo_json_state_change_reader_t *reader,
                                                         uintptr_t                            userdata);
static void callback_state_change_reader_on_remove_callback(const qeo_json_state_change_reader_t  *reader,
                                                            const char                            *json_data,
                                                            uintptr_t                             userdata);
static void callback_state_change_reader_on_policy_update(const qeo_json_state_change_reader_t  *reader,
                                                          const char                            *json_policy,
                                                          uintptr_t                             userdata);

static void callback_state_writer_on_policy_update(const qeo_json_state_writer_t  *writer,
                                                   const char                     *json_policy,
                                                   uintptr_t                      userdata);

static void callback_event_writer_on_policy_update(const qeo_json_event_writer_t  *writer,
                                                   const char                     *json_policy,
                                                   uintptr_t                      userdata);

/*########################################################################
#  STATIC VARIABLE SECTION                                              #
########################################################################*/

/* Listener objects */
const static qeo_json_factory_listener_t _factory_listener = { .on_factory_init_done = callback_factory_init_done };

const static qeo_json_event_reader_listener_t _event_reader_listener =
{
    .on_data          = callback_event_reader_on_data,
    .on_no_more_data  = callback_event_reader_no_more_data,
    .on_policy_update = callback_event_reader_on_policy_update
};
const static qeo_json_event_writer_listener_t _event_writer_listener =
{
    .on_policy_update = callback_event_writer_on_policy_update
};
const static qeo_json_state_reader_listener_t _state_reader_listener =
{
    .on_update        = callback_state_reader_update_callback,
    .on_policy_update = callback_state_reader_on_policy_update
};
const static qeo_json_state_change_reader_listener_t _state_change_reader_listener =
{
    .on_data          = callback_state_change_reader_on_data,
    .on_no_more_data  = callback_state_change_reader_on_no_more_data,
    .on_remove        = callback_state_change_reader_on_remove_callback,
    .on_policy_update = callback_state_change_reader_on_policy_update
};
const static qeo_json_state_writer_listener_t _state_writer_listener =
{
    .on_policy_update = callback_state_writer_on_policy_update
};

/* lists with all valid pointers */
static struct qeo_entity_el_s *_valid_entities[OBJ_TYPE_NUM]; /* note, we don't put the iterators in lists as they are short-lived */

const static qeo_json_async_dispatch_callback _dispatch_table[CMD_TYPE_NUM][OBJ_TYPE_NUM] =
{
    {
        create_factory,
        create_eventReader,
        create_eventWriter,
        create_stateReader,
        create_stateWriter,
        create_stateChangeReader,
        create_run_iterator, /*special case... */
        NULL,
    },
    {
        close_factory,
        close_eventReader,
        close_eventWriter,
        close_stateReader,
        close_stateWriter,
        close_stateChangeReader,
        NULL,
        NULL,
    },
    {
        NULL,
        NULL,
        write_eventWriter,
        NULL,
        write_stateWriter,
        NULL,
        NULL,
        NULL,
    },
    {
        NULL,
        NULL,
        NULL,
        NULL,
        remove_stateWriter,
        NULL,
        NULL,
        NULL,
    },
    {
        NULL,
        update_policy_eventReader,
        update_policy_eventWriter,
        update_policy_stateReader,
        update_policy_stateWriter,
        update_policy_stateChangeReader,
        NULL,
        NULL,
    },
    {
        NULL,
        request_policy_eventReader,
        request_policy_eventWriter,
        request_policy_stateReader,
        request_policy_stateWriter,
        request_policy_stateChangeReader,
        NULL,
    },
    {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        get_deviceId,
    },
};

static struct str_int_tuple_s _cmd_str_int[] =
{
    {
        .strval = CREATESTR,
        .intval = CMD_TYPE_CREATE
    },
    {
        .strval = CLOSESTR,
        .intval = CMD_TYPE_CLOSE
    },
    {
        .strval = WRITESTR,
        .intval = CMD_TYPE_WRITE
    },
    {
        .strval = REMOVESTR,
        .intval = CMD_TYPE_REMOVE
    },
    {
        .strval = POLICY_UPDATESTR,
        .intval = CMD_TYPE_POLICY_UPDATE
    },
    {
        .strval = POLICY_REQUESTSTR,
        .intval = CMD_TYPE_POLICY_REQUEST
    },
    {
        .strval = GETSTR,
        .intval = CMD_TYPE_GET
    },
};

static struct str_int_tuple_s _obj_str_int[] =
{
    {
        .strval = FACTORYSTR,
        .intval = OBJ_TYPE_FACTORY
    },
    {
        .strval = EVENTREADERSTR,
        .intval = OBJ_TYPE_EVENT_READER
    },
    {
        .strval = EVENTWRITERSTR,
        .intval = OBJ_TYPE_EVENT_WRITER
    },
    {
        .strval = STATEREADERSTR,
        .intval = OBJ_TYPE_STATE_READER
    },
    {
        .strval = STATEWRITERSTR,
        .intval = OBJ_TYPE_STATE_WRITER
    },
    {
        .strval = STATECHANGEREADERSTR,
        .intval = OBJ_TYPE_STATECHANGE_READER
    },
    {
        .strval = ITERATORSTR,
        .intval = OBJ_TYPE_ITERATOR
    },
    {
        .strval = DEVICEIDSTR,
        .intval = OBJ_TYPE_DEVICE_ID
    },
};


/*#######################################################################
 # STATIC FUNCTION IMPLEMENTATION                                        #
 ########################################################################*/
static bool is_listener_okay(const qeo_json_async_listener_t *listener)
{
    if (listener == NULL) {
        return false;
    }

    if (listener->on_qeo_json_event == NULL) {
        return false;
    }

    return true;
}

#ifndef NDEBUG
static bool is_valid_ctx(const qeo_json_async_ctx_t *ctx)
{
    if (ctx->magic != QEO_JSON_ASYNC_MAGIC) {
        qeo_log_e("ASSERT");
        return false;
    }

    if (ctx->socket_pair[0] == 0) {
        qeo_log_e("ASSERT");
        return false;
    }

    if (ctx->socket_pair[1] == 0) {
        qeo_log_e("ASSERT");
        return false;
    }

    if (ctx->async_thread == 0) {
        qeo_log_e("ASSERT");
        return false;
    }

    if (is_listener_okay(&ctx->listener) == false) {
        qeo_log_e("ASSERT");
        return false;
    }

    return true;
}

#endif

static int cmp_str_int_tuple_by_str(struct str_int_tuple_s  *s1,
                                    struct str_int_tuple_s  *s2)
{
    return strcmp(s1->strval, s2->strval);
}

static int cmp_str_int_tuple_by_intval(struct str_int_tuple_s *s1,
                                       struct str_int_tuple_s *s2)
{
    if (s1->intval == s2->intval) {
        return 0;
    }

    return 1;
}

static struct str_int_tuple_s *find_str_int(struct str_int_tuple_s  table[],
                                            size_t                  table_size,
                                            const char              *strval,
                                            int                     intval)
{
    struct str_int_tuple_s key =
    {
        .strval = strval,
        .intval = intval
    };

    if (strval != NULL) {
        return (struct str_int_tuple_s *)lfind(&key, table, &table_size, sizeof(table[0]), (int (*)(const void *, const void *))cmp_str_int_tuple_by_str);
    }
    else {
        return (struct str_int_tuple_s *)lfind(&key, table, &table_size, sizeof(table[0]), (int (*)(const void *, const void *))cmp_str_int_tuple_by_intval);
    }
}

static qeo_json_async_cmd_type_t cmd_str_to_cmd_type(const char *cmdstr)
{
    struct str_int_tuple_s *found = find_str_int(_cmd_str_int, sizeof(_cmd_str_int) / sizeof(_cmd_str_int[0]), cmdstr, -1);

    if (found == NULL) {
        return CMD_TYPE_NUM;
    }

    return (qeo_json_async_cmd_type_t)found->intval;
}

static qeo_json_async_obj_type_t obj_str_to_obj_type(const char *objstr)
{
    struct str_int_tuple_s *found = find_str_int(_obj_str_int, sizeof(_obj_str_int) / sizeof(_obj_str_int[0]), objstr, -1);

    if (found == NULL) {
        return OBJ_TYPE_NUM;
    }

    return (qeo_json_async_obj_type_t)found->intval;
}

static void async_event_callback(const qeo_json_async_ctx_t *ctx,
                                 const char                 *id,
                                 const char                 *event,
                                 const char                 *json_data)
{
    assert(is_valid_ctx(ctx));
    ctx->listener.on_qeo_json_event(ctx, ctx->userdata, id, event, json_data);
}

static bool add_to_list(qeo_json_async_obj_type_t type,
                        uintptr_t                 entity,
                        uintptr_t                 parent)
{
    struct qeo_entity_el_s *el = calloc(1, sizeof(*el));

    if (el == NULL) {
        return false;
    }

    el->qeo_entity  = entity;
    el->parent      = parent;

    LL_PREPEND(_valid_entities[type], el);
    return true;
}

static struct qeo_entity_el_s *in_list(qeo_json_async_obj_type_t  type,
                                       uintptr_t                  entity)
{
    struct qeo_entity_el_s *el = NULL;

    LL_SEARCH_SCALAR(_valid_entities[type], el, qeo_entity, entity);

    return el;
}

static void remove_from_list(qeo_json_async_obj_type_t  type,
                             struct qeo_entity_el_s     *el)
{
    LL_DELETE(_valid_entities[type], el);
    free(el);
}

static void callback_factory_init_done(qeo_factory_t  *factory,
                                       uintptr_t      userdata)
{
    assert(userdata != 0);

    struct factory_user_data_s *fuser_data = (struct factory_user_data_s *)userdata;

    if ((NULL != factory) && (add_to_list(OBJ_TYPE_FACTORY, (uintptr_t)factory, 0) == true)) {
        char data[32];
        snprintf(data, sizeof(data), "{\"id\":\"%" PRIxPTR "\"}", (uintptr_t)factory);
        async_event_callback(fuser_data->ctx, fuser_data->id, CREATESTR, data);
    }
    else {
        const char *message = "{\"error\":\"Failed to init factory\"}";
        async_event_callback(fuser_data->ctx, fuser_data->id, ERRORSTR, message);
    }

    free(fuser_data->id);
    free(fuser_data);
}

static void common_on_data(uintptr_t  reader,
                           const char *json_data,
                           uintptr_t  userdata)
{
    assert(userdata != 0);

    char id[32];
    snprintf(id, sizeof(id), "%" PRIxPTR, (uintptr_t)reader);
    async_event_callback((const qeo_json_async_ctx_t *)userdata, id, DATASTR, json_data);
}

static void common_on_remove(uintptr_t  reader,
                             const char *json_data,
                             uintptr_t  userdata)
{
    assert(userdata != 0);

    char id[32];
    snprintf(id, sizeof(id), "%" PRIxPTR, (uintptr_t)reader);
    async_event_callback((const qeo_json_async_ctx_t *)userdata, id, REMOVESTR, json_data);
}

static void callback_event_reader_on_data(const qeo_json_event_reader_t *reader,
                                          const char                    *json_data,
                                          uintptr_t                     userdata)
{
    assert(userdata != 0);
    return common_on_data((uintptr_t)reader, json_data, userdata);
}

static void callback_state_change_reader_on_data(const qeo_json_state_change_reader_t *reader,
                                                 const char                           *json_data,
                                                 uintptr_t                            userdata)
{
    assert(userdata != 0);
    return common_on_data((uintptr_t)reader, json_data, userdata);
}

static void common_no_more_data(uintptr_t reader,
                                uintptr_t userdata)
{
    assert(userdata != 0);

    char id[32];
    snprintf(id, sizeof(id), "%" PRIxPTR, (uintptr_t)reader);
    async_event_callback((const qeo_json_async_ctx_t *)userdata, id, NOMOREDATASTR, "{}");
}

static void callback_event_reader_no_more_data(const qeo_json_event_reader_t  *reader,
                                               uintptr_t                      userdata)
{
    return common_no_more_data((uintptr_t)reader, userdata);
}

static void callback_event_reader_on_policy_update(const qeo_json_event_reader_t  *reader,
                                                   const char                     *json_policy,
                                                   uintptr_t                      userdata)
{
    assert(userdata != 0);

    char id[32];
    snprintf(id, sizeof(id), "%" PRIxPTR, (uintptr_t)reader);
    async_event_callback((const qeo_json_async_ctx_t *)userdata, id, POLICY_UPDATESTR, json_policy);
}

static void callback_state_change_reader_on_no_more_data(const qeo_json_state_change_reader_t *reader,
                                                         uintptr_t                            userdata)
{
    return common_no_more_data((uintptr_t)reader, userdata);
}

static void callback_state_reader_update_callback(const qeo_json_state_reader_t *reader,
                                                  uintptr_t                     userdata)
{
    assert(userdata != 0);

    char id[32];
    snprintf(id, sizeof(id), "%" PRIxPTR, (uintptr_t)reader);
    async_event_callback((const qeo_json_async_ctx_t *)userdata, id, UPDATESTR, "{}");
}

static void callback_state_reader_on_policy_update(const qeo_json_state_reader_t  *reader,
                                                   const char                     *json_policy,
                                                   uintptr_t                      userdata)
{
    assert(userdata != 0);

    char id[32];
    snprintf(id, sizeof(id), "%" PRIxPTR, (uintptr_t)reader);
    async_event_callback((const qeo_json_async_ctx_t *)userdata, id, POLICY_UPDATESTR, json_policy);
}

static void callback_state_change_reader_on_remove_callback(const qeo_json_state_change_reader_t  *reader,
                                                            const char                            *json_data,
                                                            uintptr_t                             userdata)
{
    assert(userdata != 0);
    return common_on_remove((uintptr_t)reader, json_data, userdata);
}

static void callback_state_change_reader_on_policy_update(const qeo_json_state_change_reader_t  *reader,
                                                          const char                            *json_policy,
                                                          uintptr_t                             userdata)
{
    assert(userdata != 0);

    char id[32];
    snprintf(id, sizeof(id), "%" PRIxPTR, (uintptr_t)reader);
    async_event_callback((const qeo_json_async_ctx_t *)userdata, id, POLICY_UPDATESTR, json_policy);
}

static void callback_state_writer_on_policy_update(const qeo_json_state_writer_t  *writer,
                                                   const char                     *json_policy,
                                                   uintptr_t                      userdata)
{
    assert(userdata != 0);

    char id[32];
    snprintf(id, sizeof(id), "%" PRIxPTR, (uintptr_t)writer);
    async_event_callback((const qeo_json_async_ctx_t *)userdata, id, POLICY_UPDATESTR, json_policy);
}

static void callback_event_writer_on_policy_update(const qeo_json_event_writer_t  *writer,
                                                   const char                     *json_policy,
                                                   uintptr_t                      userdata)
{
    assert(userdata != 0);

    char id[32];
    snprintf(id, sizeof(id), "%" PRIxPTR, (uintptr_t)writer);
    async_event_callback((const qeo_json_async_ctx_t *)userdata, id, POLICY_UPDATESTR, json_policy);
}

static bool create_factory(const qeo_json_async_ctx_t *ctx,
                           struct str_int_tuple_s     *id,
                           const json_t               *json)
{
    bool                        retval      = false;
    struct factory_user_data_s  *user_data  = NULL;
    char                        *identity   = NULL;

    do {
        user_data = calloc(1, sizeof(*user_data));
        if (user_data == NULL) {
            qeo_log_e("No memory");
            break;
        }

        user_data->id = strdup(id->strval);
        if (user_data->id == NULL) {
            qeo_log_e("No memory");
            break;
        }

        user_data->ctx = ctx;
        json_t *jsonIdentity = json_object_get(json, IDENTITYSTR);
        if (NULL == jsonIdentity) {
            if (QEO_OK != qeo_json_factory_create(&_factory_listener, (uintptr_t)user_data)) {
                qeo_log_e("Could not create factory");
                break;
            }
        }
        else {
            if (!json_is_string(jsonIdentity)) {
                identity = json_dumps(jsonIdentity, 0);  /* free() this ! */

                if (QEO_OK != qeo_json_factory_create_by_id(identity, &_factory_listener, (uintptr_t)user_data)) {
                    qeo_log_e("Could not create factory for identity");
                    free(identity);
                    break;
                }
                free(identity);
            }
            else {
                if (QEO_OK != qeo_json_factory_create_by_id(json_string_value(jsonIdentity), &_factory_listener, (uintptr_t)user_data)) {
                    qeo_log_e("Could not create factory for identity");
                    break;
                }
            }
        }

        retval = true;
    } while (0);

    if (retval == false) {
        if (user_data != NULL) {
            free(user_data->id);
            free(user_data);
        }
    }

    return retval;
}

static const char *get_string_from_json(const json_t  *json,
                                        const char    *key)
{
    json_t *obj = json_object_get(json, key);

    if ((NULL == obj) || (!json_is_string(obj))) {
        return NULL;
    }

    return json_string_value(obj);
}

struct iterate_userdata {
    const qeo_json_async_ctx_t  *ctx;
    const char                  *id;
};

static qeo_iterate_action_t iterate_callback(const char *json_data,
                                             uintptr_t  userdata)
{
    const struct iterate_userdata *iu = (const struct iterate_userdata *)userdata;

    async_event_callback(iu->ctx, iu->id, ITERATESTR, json_data);

    return QEO_ITERATE_CONTINUE;
}

static bool create_run_iterator(const qeo_json_async_ctx_t  *ctx,
                                struct str_int_tuple_s      *id,
                                const json_t                *json)
{
    const char              *reader_id = get_string_from_json(json, READERIDSTR);
    static uintptr_t        iterator_counter;
    char                    data[32];
    char                    id_str[32];
    struct qeo_entity_el_s  *el = NULL;
    char                    *endptr;
    qeo_json_state_reader_t *reader;

    do {
        if (reader_id == NULL) {
            qeo_log_e("reader_id == NULL");
            break;
        }

        reader = (qeo_json_state_reader_t *)(uintptr_t)strtoull(reader_id, &endptr, 16);
        if ((reader == NULL) || (*endptr != '\0')) {
            break;
        }

        if ((el = in_list(OBJ_TYPE_STATE_READER, (uintptr_t)reader)) == NULL) {
            qeo_log_e("Invalid json: unknown reader");
            break;
        }


        snprintf(id_str, sizeof(id_str), "%" PRIxPTR, (uintptr_t)++ iterator_counter);
        snprintf(data, sizeof(data), "{\"id\":\"%s\"}", id_str);
        id->intval = iterator_counter;

        async_event_callback(ctx, id->strval, CREATESTR, data);

        const struct iterate_userdata iu =
        {
            .ctx  = ctx,
            .id   = id_str
        };

        if (qeo_json_state_reader_foreach(reader, iterate_callback, (uintptr_t)&iu) != QEO_OK) {
            qeo_log_e("Iterating (%" PRIxPTR ")failed..", iterator_counter);
        }

        /* TODO: DISCUSS WITH RONNY AND TOM IF WE CAN'T JUST DO ANOTHER ITERATESTR  */
        async_event_callback(ctx, id_str, CLOSESTR, "{}");
    } while (0);

    return true;
}

/* not for factories or iterators ! */
static bool create_entity(const qeo_json_async_ctx_t  *ctx,
                          struct str_int_tuple_s      *id,
                          const json_t                *json,
                          entity_create_cb            create_cb,
                          entity_enable_cb            enable_cb,
                          qeo_json_async_obj_type_t   obj_type)
{
    qeo_factory_t           *factory;
    char                    data[32];
    char                    *endptr;
    const char              *factory_id   = get_string_from_json(json, FACTORYIDSTR);
    char                    *typeDesc     = json_dumps(json_object_get(json, TYPEDESCSTR), 0); /* free() this ! */
    bool                    retval        = false;
    json_t                  *policyFlag   = json_object_get(json, POLICYSTR);
    bool                    policyEnabled = false;
    struct qeo_entity_el_s  *el           = NULL;
    uintptr_t               entity;

    do {
        /* TODO: INVESTIGATE THE FLAGS OF json_dumps() */
        if ((factory_id == NULL) || (typeDesc == NULL)) {
            break;
        }

        factory = (qeo_factory_t *)(uintptr_t)strtoull(factory_id, &endptr, 16);
        if ((factory == NULL) || (*endptr != '\0')) {
            break;
        }

        if ((el = in_list(OBJ_TYPE_FACTORY, (uintptr_t)factory)) == NULL) {
            qeo_log_e("Invalid json: unknown factory");
            break;
        }

        if ((NULL != policyFlag) && (json_is_true(policyFlag))) {
            policyEnabled = true;
        }

        entity = create_cb(ctx, factory, typeDesc, policyEnabled);
        if (0 == entity) {
            qeo_log_e("Create failed !");
            break;
        }

        if (add_to_list(obj_type, entity, (uintptr_t)factory) == false) {
            break;
        }

        snprintf(data, sizeof(data), "{\"id\":\"%" PRIxPTR "\"}", entity);
        async_event_callback(ctx, id->strval, CREATESTR, data);

        if (enable_cb(entity) != QEO_OK) {
            qeo_log_e("Could not enable entity \"%s\"!", data);
            /* do nothing for now */
        }

        retval = true;
    } while (0);

    free(typeDesc);

    return retval;
}

static uintptr_t create_eventReader_cb(const qeo_json_async_ctx_t *ctx,
                                       qeo_factory_t              *factory,
                                       const char                 *typeDesc,
                                       bool                       policyEnabled)
{
    qeo_json_event_reader_listener_t event_reader_listener_tmp = _event_reader_listener;

    if (false == policyEnabled) {
        event_reader_listener_tmp.on_policy_update = NULL;
    }

    return (uintptr_t)qeo_json_factory_create_event_reader(factory, typeDesc, &event_reader_listener_tmp, (uintptr_t)ctx);
}

static bool create_eventReader(const qeo_json_async_ctx_t *ctx,
                               struct str_int_tuple_s     *id,
                               const json_t               *json)
{
    return create_entity(ctx, id, json, create_eventReader_cb, (entity_enable_cb)qeo_json_event_reader_enable, OBJ_TYPE_EVENT_READER);
}

static uintptr_t create_eventWriter_cb(const qeo_json_async_ctx_t *ctx,
                                       qeo_factory_t              *factory,
                                       const char                 *typeDesc,
                                       bool                       policyEnabled)
{
    qeo_json_event_writer_listener_t event_writer_listener_tmp = _event_writer_listener;

    if (false == policyEnabled) {
        event_writer_listener_tmp.on_policy_update = NULL;
    }

    return (uintptr_t)qeo_json_factory_create_event_writer(factory, typeDesc, &event_writer_listener_tmp, (uintptr_t)ctx);
}

static bool create_eventWriter(const qeo_json_async_ctx_t *ctx,
                               struct str_int_tuple_s     *id,
                               const json_t               *json)
{
    return create_entity(ctx, id, json, create_eventWriter_cb, (entity_enable_cb)qeo_json_event_writer_enable, OBJ_TYPE_EVENT_WRITER);
}

static uintptr_t create_stateWriter_cb(const qeo_json_async_ctx_t *ctx,
                                       qeo_factory_t              *factory,
                                       const char                 *typeDesc,
                                       bool                       policyEnabled)
{
    qeo_json_state_writer_listener_t state_writer_listener_tmp = _state_writer_listener;

    if (false == policyEnabled) {
        state_writer_listener_tmp.on_policy_update = NULL;
    }

    return (uintptr_t)qeo_json_factory_create_state_writer(factory, typeDesc, &state_writer_listener_tmp, (uintptr_t)ctx);
}

static bool create_stateWriter(const qeo_json_async_ctx_t *ctx,
                               struct str_int_tuple_s     *id,
                               const json_t               *json)
{
    return create_entity(ctx, id, json, create_stateWriter_cb, (entity_enable_cb)qeo_json_state_writer_enable, OBJ_TYPE_STATE_WRITER);
}

static uintptr_t create_stateReader_cb(const qeo_json_async_ctx_t *ctx,
                                       qeo_factory_t              *factory,
                                       const char                 *typeDesc,
                                       bool                       policyEnabled)
{
    qeo_json_state_reader_listener_t state_reader_listener_tmp = _state_reader_listener;

    if (false == policyEnabled) {
        state_reader_listener_tmp.on_policy_update = NULL;
    }

    return (uintptr_t)qeo_json_factory_create_state_reader(factory, typeDesc, &state_reader_listener_tmp, (uintptr_t)ctx);
}

static bool create_stateReader(const qeo_json_async_ctx_t *ctx,
                               struct str_int_tuple_s     *id,
                               const json_t               *json)
{
    return create_entity(ctx, id, json, create_stateReader_cb, (entity_enable_cb)qeo_json_state_reader_enable, OBJ_TYPE_STATE_READER);
}

static uintptr_t create_stateChangeReader_cb(const qeo_json_async_ctx_t *ctx,
                                             qeo_factory_t              *factory,
                                             const char                 *typeDesc,
                                             bool                       policyEnabled)
{
    qeo_json_state_change_reader_listener_t state_change_reader_listener_tmp = _state_change_reader_listener;

    if (false == policyEnabled) {
        state_change_reader_listener_tmp.on_policy_update = NULL;
    }

    return (uintptr_t)qeo_json_factory_create_state_change_reader(factory, typeDesc, &state_change_reader_listener_tmp, (uintptr_t)ctx);
}

static bool create_stateChangeReader(const qeo_json_async_ctx_t *ctx,
                                     struct str_int_tuple_s     *id,
                                     const json_t               *json)
{
    return create_entity(ctx, id, json, create_stateChangeReader_cb, (entity_enable_cb)qeo_json_state_change_reader_enable, OBJ_TYPE_STATECHANGE_READER);
}

static bool do_entity_action(const qeo_json_async_ctx_t *ctx,
                             struct str_int_tuple_s     *id,
                             const json_t               *json,
                             entity_action_cb           action_cb)
{
    bool  retval    = false;
    char  *datastr  = NULL;

    do {
        datastr = json_dumps(json_object_get(json, DATASTR), 0);  /* free() this ! */
        if (datastr == NULL) {
            break;
        }

        if (QEO_OK != action_cb(id->intval, datastr)) {
            qeo_log_e("Could not perform action");
            break;
        }

        retval = true;
    } while (0);

    free(datastr);

    return retval;
}

static bool write_eventWriter(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json)
{
    return do_entity_action(ctx, id, json, (entity_action_cb)qeo_json_event_writer_write);
}

static bool write_stateWriter(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json)
{
    return do_entity_action(ctx, id, json, (entity_action_cb)qeo_json_state_writer_write);
}

static bool remove_stateWriter(const qeo_json_async_ctx_t *ctx,
                               struct str_int_tuple_s     *id,
                               const json_t               *json)
{
    return do_entity_action(ctx, id, json, (entity_action_cb)qeo_json_state_writer_remove);
}

static bool close_factory(const qeo_json_async_ctx_t  *ctx,
                          struct str_int_tuple_s      *id,
                          const json_t                *json)
{
    qeo_json_async_dispatch_callback dispatch_cb;

    for (int i = 0; i < sizeof(_valid_entities) / sizeof(_valid_entities[0]); ++i) {
        struct qeo_entity_el_s  *el;
        struct qeo_entity_el_s  *tmp;

        dispatch_cb = _dispatch_table[CMD_TYPE_CLOSE][i];

        LL_FOREACH_SAFE(_valid_entities[i], el, tmp)
        {
            if (id->intval == el->parent) {
                struct str_int_tuple_s delid;
                delid.intval = el->qeo_entity;
                dispatch_cb(ctx, &delid, NULL);


                LL_DELETE(_valid_entities[i], el);
                free(el);
            }
        }
    }

    qeo_log_d("Closing factory %" PRIxPTR, id->intval);
    qeo_json_factory_close((qeo_factory_t *)id->intval);

    return true;
}

static bool close_eventReader(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json)
{
    qeo_log_d("Closing event reader %" PRIxPTR, id->intval);
    qeo_json_event_reader_close((qeo_json_event_reader_t *)id->intval);

    return true;
}

static bool close_eventWriter(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json)
{
    qeo_log_d("Closing event writer %" PRIxPTR, id->intval);
    qeo_json_event_writer_close((qeo_json_event_writer_t *)id->intval);

    return true;
}

static bool close_stateWriter(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json)
{
    qeo_log_d("Closing state writer %" PRIxPTR, id->intval);
    qeo_json_state_writer_close((qeo_json_state_writer_t *)id->intval);

    return true;
}

static bool close_stateReader(const qeo_json_async_ctx_t  *ctx,
                              struct str_int_tuple_s      *id,
                              const json_t                *json)
{
    qeo_log_d("Closing state reader %" PRIxPTR, id->intval);
    qeo_json_state_reader_close((qeo_json_state_reader_t *)id->intval);
    /* TODO: check for iterators */

    return true;
}

static bool close_stateChangeReader(const qeo_json_async_ctx_t  *ctx,
                                    struct str_int_tuple_s      *id,
                                    const json_t                *json)
{
    qeo_log_d("Closing state reader %" PRIxPTR, id->intval);
    qeo_json_state_change_reader_close((qeo_json_state_change_reader_t *)id->intval);

    return true;
}

static bool update_policy_eventReader(const qeo_json_async_ctx_t  *ctx,
                                      struct str_int_tuple_s      *id,
                                      const json_t                *json)
{
    bool  retval    = false;
    char  *datastr  = NULL;

    datastr = json_dumps(json_object_get(json, DATASTR), 0);         /* free() this ! */

    if (qeo_json_event_reader_policy_update((const qeo_json_event_reader_t *)id->intval, datastr) == QEO_OK) {
        retval = true;
    }

    free(datastr);
    return retval;
}

static bool update_policy_eventWriter(const qeo_json_async_ctx_t  *ctx,
                                      struct str_int_tuple_s      *id,
                                      const json_t                *json)
{
    bool  retval    = false;
    char  *datastr  = NULL;

    datastr = json_dumps(json_object_get(json, DATASTR), 0);         /* free() this ! */

    if (qeo_json_event_writer_policy_update((const qeo_json_event_writer_t *)id->intval, datastr) == QEO_OK) {
        retval = true;
    }

    free(datastr);
    return retval;
}

static bool update_policy_stateReader(const qeo_json_async_ctx_t  *ctx,
                                      struct str_int_tuple_s      *id,
                                      const json_t                *json)
{
    bool  retval    = false;
    char  *datastr  = NULL;

    datastr = json_dumps(json_object_get(json, DATASTR), 0);         /* free() this ! */

    if (qeo_json_state_reader_policy_update((const qeo_json_state_reader_t *)id->intval, datastr) == QEO_OK) {
        retval = true;
    }

    free(datastr);
    return retval;
}

static bool update_policy_stateWriter(const qeo_json_async_ctx_t  *ctx,
                                      struct str_int_tuple_s      *id,
                                      const json_t                *json)
{
    bool  retval    = false;
    char  *datastr  = NULL;

    datastr = json_dumps(json_object_get(json, DATASTR), 0);         /* free() this ! */

    if (qeo_json_state_writer_policy_update((const qeo_json_state_writer_t *)id->intval, datastr) == QEO_OK) {
        retval = true;
    }

    free(datastr);
    return retval;
}

static bool update_policy_stateChangeReader(const qeo_json_async_ctx_t  *ctx,
                                            struct str_int_tuple_s      *id,
                                            const json_t                *json)
{
    bool  retval    = false;
    char  *datastr  = NULL;

    datastr = json_dumps(json_object_get(json, DATASTR), 0);         /* free() this ! */

    if (qeo_json_state_change_reader_policy_update((const qeo_json_state_change_reader_t *)id->intval, datastr) == QEO_OK) {
        retval = true;
    }

    free(datastr);
    return retval;
}

static bool request_policy_eventReader(const qeo_json_async_ctx_t *ctx,
                                       struct str_int_tuple_s     *id,
                                       const json_t               *json)
{
    if (qeo_json_event_reader_policy_update((const qeo_json_event_reader_t *)id->intval, NULL) == QEO_OK) {
        return true;
    }

    return false;
}

static bool request_policy_eventWriter(const qeo_json_async_ctx_t *ctx,
                                       struct str_int_tuple_s     *id,
                                       const json_t               *json)
{
    if (qeo_json_event_writer_policy_update((const qeo_json_event_writer_t *)id->intval, NULL) == QEO_OK) {
        return true;
    }

    return false;
}

static bool request_policy_stateReader(const qeo_json_async_ctx_t *ctx,
                                       struct str_int_tuple_s     *id,
                                       const json_t               *json)
{
    if (qeo_json_state_reader_policy_update((const qeo_json_state_reader_t *)id->intval, NULL) == QEO_OK) {
        return true;
    }

    return false;
}

static bool request_policy_stateWriter(const qeo_json_async_ctx_t *ctx,
                                       struct str_int_tuple_s     *id,
                                       const json_t               *json)
{
    if (qeo_json_state_writer_policy_update((const qeo_json_state_writer_t *)id->intval, NULL) == QEO_OK) {
        return true;
    }

    return false;
}

static bool request_policy_stateChangeReader(const qeo_json_async_ctx_t *ctx,
                                             struct str_int_tuple_s     *id,
                                             const json_t               *json)
{
    if (qeo_json_state_change_reader_policy_update((const qeo_json_state_change_reader_t *)id->intval, NULL) == QEO_OK) {
        return true;
    }

    return false;
}

static bool get_deviceId(const qeo_json_async_ctx_t *ctx,
                         struct str_int_tuple_s     *id,
                         const json_t               *json)
{
    bool  ret         = false;
    char  *device_id  = NULL;

    do {
        if (QEO_OK != qeo_json_get_device_id(&device_id)) {
            break;
        }

        async_event_callback(ctx, id->strval, DATASTR, device_id);

        ret = true;
    } while (0);

    free(device_id);

    return ret;
}

static void process_cmd(const qeo_json_async_ctx_t  *ctx,
                        const qeo_json_async_cmd_t  *cmd)
{
    bool  retval = false;
    char  *message;

    qeo_log_d("Cmd: %d, options: %s", cmd->cmd_type, cmd->options);
    json_t                            *json         = NULL;
    const char                        *obj_type_str = NULL;
    struct str_int_tuple_s            id            = {};
    struct qeo_entity_el_s            *el = NULL;
    qeo_json_async_obj_type_t         obj_type;
    qeo_json_async_dispatch_callback  dispatch_cb;
    char                              *endptr;

    do {
        json = json_loads(cmd->options, 0, NULL);
        if (json == NULL) {
            message = "Could not parse json";
            break;
        }

        if ((id.strval = get_string_from_json(json, IDSTR)) == NULL) {
            message = "Invalid json: no " IDSTR;
            break;
        }

        if ((obj_type_str = get_string_from_json(json, OBJTYPESTR)) == NULL) {
            message = "Invalid json: no " OBJTYPESTR;
            break;
        }

        obj_type = obj_str_to_obj_type(obj_type_str);
        if (obj_type == OBJ_TYPE_NUM) {
            message = "Invalid json: invalid " OBJTYPESTR;
            break;
        }

        dispatch_cb = _dispatch_table[cmd->cmd_type][obj_type];
        if (dispatch_cb == NULL) {
            message = "Unsupported operation";
            break;
        }

        if ((cmd->cmd_type != CMD_TYPE_CREATE) &&
            (cmd->cmd_type != CMD_TYPE_GET)) {
            id.intval = (uintptr_t)strtoull(id.strval, &endptr, 16);
            if ((id.intval == 0) || (*endptr != '\0')) {
                message = "Invalid json: invalid " IDSTR;
                break;
            }

            /* do lookup */
            if ((el = in_list(obj_type, id.intval)) == NULL) {
                message = "Invalid json: unknown " IDSTR;
                break;
            }
        }

        if (cmd->cmd_type == CMD_TYPE_CLOSE) {
            /* remove from list */
            remove_from_list(obj_type, el);
        }

        if (dispatch_cb(ctx, &id, json) == false) {
            /* TODO: IMPROVE MESSAGE */
            message = "{\"error\":\"Failed to perform cmd\"}";
            break;
        }


        retval = true;
    } while (0);

    if (retval == false) {
        qeo_log_e(message);
        async_event_callback(ctx, id.strval, "error", message);
    }

    json_decref(json);
}

static void free_cmd(qeo_json_async_cmd_t *cmd)
{
    free(cmd->options);
}

static void json_async_worker_thread(qeo_json_async_ctx_t *ctx)
{
    qeo_json_async_cmd_t cmd;

    while (read(ctx->socket_pair[1], &cmd, sizeof(cmd)) > 0) {
        assert(is_valid_ctx(ctx));
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        process_cmd(ctx, &cmd);
        free_cmd(&cmd);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }

    qeo_log_i("json_async worker thread about to stop");
}

/*########################################################################
#  PUBLIC FUNCTION IMPLEMENTATION                                       #
########################################################################*/

qeo_json_async_ctx_t *qeo_json_async_create(const qeo_json_async_listener_t *listener,
                                            uintptr_t                       userdata)
{
    qeo_retcode_t         ret   = QEO_EFAIL;
    qeo_json_async_ctx_t  *ctx  = NULL;


    if (is_listener_okay(listener) == false) {
        return NULL;
    }

    if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
        return NULL;
    }

    if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, ctx->socket_pair) != 0) {
        qeo_log_e("Could not construct socketpair (%s)", strerror(errno));
        free(ctx);
        return NULL;
    }

    do {
        int flags;
        if ((flags = fcntl(ctx->socket_pair[0], F_GETFL)) == -1) {
            qeo_log_e("Cannot read socket flags");
            break;
        }
        flags |= O_NONBLOCK;
        if (fcntl(ctx->socket_pair[0], F_SETFL, flags) < 0) {
            qeo_log_e("Cannot make write socket non-blocking (%s)", strerror(errno));
            break;
        }

        if (pthread_create(&ctx->async_thread, NULL, (void *(*)(void *))json_async_worker_thread, ctx) != 0) {
            qeo_log_e("Could not create thread");
            break;
        }

        ctx->listener = *listener;
        ctx->userdata = userdata;
#ifndef NDEBUG
        ctx->magic = QEO_JSON_ASYNC_MAGIC;
#endif

        assert(is_valid_ctx(ctx));

        ret = QEO_OK;
    } while (0);

    if (ret != QEO_OK) {
        close(ctx->socket_pair[0]);
        close(ctx->socket_pair[1]);
        free(ctx);
        ctx = NULL;
    }

    return ctx;
}

void qeo_json_async_close(qeo_json_async_ctx_t *ctx)
{
    int ret = 0;

    if (NULL == ctx) {
        qeo_log_e("NULL == ctx");
        return;
    }

    /* closing the sockets will make the thread stop */
    ret = close(ctx->socket_pair[0]);
    if (0 != ret) {
        qeo_log_e("close failed with error: %s", strerror(ret));
    }

    ret = close(ctx->socket_pair[1]);
    if (0 != ret) {
        qeo_log_e("close failed with error: %s", strerror(ret));
    }

    if (pthread_cancel(ctx->async_thread) != 0) {
        qeo_log_e("pthread_cancel failed");
    }

    free(ctx);
}

qeo_retcode_t qeo_json_async_call(qeo_json_async_ctx_t  *ctx,
                                  const char            *cmdstr,
                                  const char            *options)
{
    qeo_retcode_t         ret = QEO_EFAIL;
    qeo_json_async_cmd_t  cmd = {};

    if ((ctx == NULL) || (cmdstr == NULL) || (options == NULL)) {
        return QEO_EINVAL;
    }

    assert(is_valid_ctx(ctx));

    do {
        cmd.cmd_type = cmd_str_to_cmd_type(cmdstr);
        if (cmd.cmd_type == CMD_TYPE_NUM) {
            qeo_log_w("Invalid cmd:%s", cmdstr);
            break;
        }

        cmd.options = strdup(options);

        if (cmd.options == NULL) {
            ret = QEO_ENOMEM;
            qeo_log_e("No memory");
            break;
        }

        if (write(ctx->socket_pair[0], &cmd, sizeof(cmd)) < 0) {
            qeo_log_e("Could not write ! (%s)", strerror(errno));
            ret = QEO_EFAIL;
            break;
        }

        ret = QEO_OK;
    } while (0);

    if (ret != QEO_OK) {
        free_cmd(&cmd);
    }


    return ret;
}
