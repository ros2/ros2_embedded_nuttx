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

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "unittest/unittest.h"

#include "list.h"

START_TEST(test_list_api_inargs)
{
    list_t *list = (list_t *)0xdead0000;
    void *data = (void *)0xdead0001;
    list_iterate_callback cb = (list_iterate_callback)0xdead0002;

    list_free(NULL);
    fail_unless(QEO_EINVAL == list_add(NULL, data));
    fail_unless(QEO_EINVAL == list_add(list, NULL));
    fail_unless(QEO_EINVAL == list_remove(NULL, data));
    fail_unless(QEO_EINVAL == list_remove(list, NULL));
    fail_unless(QEO_EINVAL == list_foreach(NULL, cb, 0));
    fail_unless(QEO_EINVAL == list_foreach(list, NULL, 0));
    fail_unless(NULL != (list = list_new()));
    fail_unless(QEO_EBADSTATE == list_remove(list, data));
    list_free(list);
}
END_TEST

static list_iterate_action_t count_cb(void *data,
                                      uintptr_t userdata)
{
    int *cnt = (int *)userdata;
    list_iterate_action_t action;

    if (NULL == cnt) {
        action =LIST_ITERATE_ABORT;
    }
    else {
        (*cnt)++;
        action = LIST_ITERATE_CONTINUE;
    }
    return action;
}

static int count(list_t *list)
{
    int cnt = 0;

    fail_unless(QEO_OK == list_foreach(list, count_cb, (uintptr_t)&cnt));
    return cnt;
}

static void populate_add_1(list_t *list,
                           bool do_strdup,
                           char *str)
{
    char *s;

    s = (do_strdup ? strdup(str) : str);
    fail_unless(NULL != s);
    fail_unless(QEO_OK == list_add(list, s));

}

static void populate(list_t *list,
                     bool do_strdup)
{
    populate_add_1(list, do_strdup, "item_1");
    fail_unless(1 == count(list));
    fail_unless(1 == list_length(list));
    populate_add_1(list, do_strdup, "item_2");
    fail_unless(2 == count(list));
    fail_unless(2 == list_length(list));
    populate_add_1(list, do_strdup, "item_3");
    fail_unless(3 == count(list));
    fail_unless(3 == list_length(list));
}

START_TEST(test_list_add)
{
    list_t  *list;

    fail_unless(NULL != (list = list_new()));
    /* test add and iterate */
    populate(list, false);
    /* test iterate abort */
    fail_unless(QEO_OK == list_foreach(list, count_cb, 0));
    list_free(list);
}
END_TEST

static void list_add_remove_1(int num, ...)
{
    list_t *list;
    va_list args;

    va_start(args, num);
    /* prepare */
    fail_unless(NULL != (list = list_new()));
    populate(list, false);
    /* test */
    for (int i = 0; i < num; i++) {
        fail_unless(QEO_OK == list_remove(list, va_arg(args, char *)));
        fail_unless(num - i - 1 == count(list));
        fail_unless(num - i - 1 == list_length(list));
    }
    /* clean up */
    fail_unless(0 == count(list));
    fail_unless(0 == list_length(list));
    list_free(list);
    va_end(args);
}

START_TEST(test_list_add_remove)
{
    list_add_remove_1(3, "item_1", "item_2", "item_3");
    list_add_remove_1(3, "item_2", "item_1", "item_3");
    list_add_remove_1(3, "item_2", "item_3", "item_1");
    list_add_remove_1(3, "item_3", "item_2", "item_1");
}
END_TEST

START_TEST(test_list_remove_error)
{
    list_t *list;

    fail_unless(NULL != (list = list_new()));
    fail_unless(QEO_EBADSTATE == list_remove(list, "item_2"));
    fail_unless(QEO_OK == list_add(list, "item_1"));
    fail_unless(QEO_EBADSTATE == list_remove(list, "item_2"));
    list_free(list);
}
END_TEST

static list_iterate_action_t iterate_delete_cb(void *data,
                                               uintptr_t userdata)
{
    char *item = (char *)data;
    const char *needle = (const char *)userdata;

    if (NULL == needle) {
        /* remove all */
        free(item);
        return LIST_ITERATE_DELETE;
    }
    else if (0 == strcmp(item, needle)) {
        /* found needle */
        free(item);
        return LIST_ITERATE_DELETE;
    }
    return LIST_ITERATE_CONTINUE;
}

START_TEST(test_list_iterate_delete)
{
    list_t *list;

    /* test delete all */
    fail_unless(NULL != (list = list_new()));
    populate(list, true);
    fail_unless(QEO_OK == list_foreach(list, iterate_delete_cb, 0));
    fail_unless(0 == count(list));
    fail_unless(0 == list_length(list));
    /* test delete specific */
    populate(list, true);
    fail_unless(QEO_OK == list_foreach(list, iterate_delete_cb, (uintptr_t)"item_2"));
    fail_unless(2 == count(list));
    fail_unless(2 == list_length(list));
    fail_unless(QEO_OK == list_foreach(list, iterate_delete_cb, (uintptr_t)"item_1"));
    fail_unless(1 == count(list));
    fail_unless(1 == list_length(list));
    /* already removed, should not change anything */
    fail_unless(QEO_OK == list_foreach(list, iterate_delete_cb, (uintptr_t)"item_1"));
    fail_unless(1 == count(list));
    fail_unless(1 == list_length(list));
    fail_unless(QEO_OK == list_foreach(list, iterate_delete_cb, (uintptr_t)"item_3"));
    fail_unless(0 == count(list));
    fail_unless(0 == list_length(list));
    list_free(list);
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    { .name = "input args", .function = test_list_api_inargs },
    { .name = "add", .function = test_list_add },
    { .name = "add - remove", .function = test_list_add_remove },
    { .name = "remove - error", .function = test_list_remove_error },
    { .name = "iterate with delete", .function = test_list_iterate_delete },
    {NULL}
};

void register_type_support_tests(Suite *s)
{
    TCase *tc = tcase_create("list tests");
    tcase_addtests(tc, tests);
    suite_add_tcase (s, tc);
}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_type_support_tests, .name = "list" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "list",
        .desc = "list tests",
};

/* called before every test case starts */
static void init_tcase(void)
{
}

/* called after every test case finishes */
static void fini_tcase(void)
{
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
