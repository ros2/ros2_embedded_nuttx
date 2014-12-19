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

#ifndef __UNITY_H__
#define __UNITY_H__

#include <check.h>
#include <stdint.h>
#include <string.h>

/* This file maps unity test macros to check test macros (used by CMock) */

#define UNITY_DO_NOT_CHECK ((void*)-1)

/* hack to prevent unused variable warning */
#define TEST_LINE_NUM 0; (void)cmock_line;

#define UNITY_LINE_TYPE unsigned short
#define UNITY_COUNTER_TYPE unsigned short
#define UNITY_TEST_ASSERT(c,l,m) fail_unless((c), (m))
#define UNITY_TEST_ASSERT_NOT_NULL(p,l,m) fail_unless(NULL!=(p), (m))
#define UNITY_TEST_ASSERT_EQUAL_STRING(a,b,l,m) fail_unless((NULL == (a) && NULL == (b)) || 0 == strcmp((a),(b)), (m))
#define UNITY_TEST_ASSERT_EQUAL_INT8(a,b,l,m) fail_unless((a) == (b), (m))
#define UNITY_TEST_ASSERT_EQUAL_INT(a,b,l,m) fail_unless((a) == (b), (m))
#define UNITY_TEST_ASSERT_EQUAL_INT16(a,b,l,m) fail_unless((a) == (b), (m))
#define UNITY_TEST_ASSERT_EQUAL_FLOAT(a,b,l,m) fail_unless((a) == (b), (m))
#define UNITY_TEST_ASSERT_EQUAL_PTR(a,b,l,m) fail_unless((void *)(a) == (void *)(b), (m))
#define UNITY_TEST_ASSERT_EQUAL_HEX8(a,b,l,m) fail_unless((a) == (b), (m))
#define UNITY_TEST_ASSERT_EQUAL_HEX16(a,b,l,m) fail_unless((a) == (b), (m))
#define UNITY_TEST_ASSERT_EQUAL_HEX32(a,b,l,m) fail_unless((a) == (b), (m))
#define UNITY_TEST_ASSERT_NULL(p,l,m) fail_unless(NULL==(p), (m))
#define UNITY_TEST_ASSERT_EQUAL_INT_ARRAY(a,b,n,l,m) fail_unless(0 == memcmp((a),(b),(n)*sizeof(int)), (m))
#define UNITY_TEST_ASSERT_EQUAL_INT8_ARRAY(a,b,n,l,m) fail_unless(0 == memcmp((a),(b),(n)*sizeof(int8_t)), (m))
#define UNITY_TEST_ASSERT_EQUAL_INT16_ARRAY(a,b,n,l,m) fail_unless(0 == memcmp((a),(b),(n)*sizeof(int16_t)), (m))
#define UNITY_TEST_ASSERT_EQUAL_INT32_ARRAY(a,b,n,l,m) fail_unless(0 == memcmp((a),(b),(n)*sizeof(int32_t)), (m))
#define UNITY_TEST_ASSERT_EQUAL_HEX8_ARRAY(a,b,n,l,m) fail_unless(0 == memcmp((a),(b),(n)*sizeof(uint8_t)), (m))
#define UNITY_TEST_ASSERT_EQUAL_HEX16_ARRAY(a,b,n,l,m) fail_unless(0 == memcmp((a),(b),(n)*sizeof(uint16_t)), (m))
#define UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY(a,b,n,l,m) fail_unless(0 == memcmp((a),(b),(n)*sizeof(int32_t)),(m))
#define UNITY_TEST_ASSERT_EQUAL_FLOAT_ARRAY(a,b,n,l,m) fail_unless(0 == memcmp((a),(b),(n)*sizeof(float)), (m))

#ifdef NO_TEST_ASSERT_EQUAL_MEMORY_MESSAGE
#define UNITY_TEST_ASSERT_EQUAL_MEMORY(a,b,n,l,m) (void)a;(void)b
#else
/* compilation of this fails with the mock of TwinOaks' CoreDX */
#define UNITY_TEST_ASSERT_EQUAL_MEMORY(a,b,n,l,m) fail_unless((NULL == (a) && NULL == (b)) || 0 == memcmp((a),(b),(n)), (m))
#endif

/* Unused: always fail */
#define UNITY_TEST_ASSERT_EQUAL_STRING_ARRAY(a,b,n,l,m) fail_unless(0)

#endif
