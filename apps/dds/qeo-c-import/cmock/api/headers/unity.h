#include <check.h>
#include <string.h>

/* This file maps unity test macros to check test macros (used by CMock) */
#ifndef UNITY_LINE_TYPE
#define UNITY_LINE_TYPE unsigned short
#endif

#ifndef UNITY_COUNTER_TYPE
#define UNITY_COUNTER_TYPE unsigned short
#endif

#define TEST_LINE_NUM 0

//#define TEST_FAIL(m) fail(m)
//#define TEST_ASSERT_EQUAL_MESSAGE(a,b,m) fail_unless(a==b, m)
//#define TEST_ASSERT_EQUAL_INT_MESSAGE(a,b,m) fail_unless(a==b, m)
//#define TEST_ASSERT_EQUAL_HEX8_MESSAGE(a,b,m) fail_unless(a==b, m)
//#define TEST_ASSERT_EQUAL_STRING_MESSAGE(a,b,m) fail_unless(0 == strcmp(a,b), m)
//#define TEST_ASSERT_NULL(p) fail_unless(NULL==p)

#define UNITY_TEST_ASSERT(c,l,m) fail_unless(c, m)
#define UNITY_TEST_ASSERT_NULL(p,l,m) fail_unless(NULL==p)
#define UNITY_TEST_ASSERT_NOT_NULL(p,l,m) fail_unless(NULL!=p, m)
#define UNITY_TEST_ASSERT_EQUAL_MEMORY(a,b,n,l,m) fail_unless((a == NULL) ? (b == NULL) : ((b == NULL) ? 0 : (0 == memcmp(a,b,n))), m)
#define UNITY_TEST_ASSERT_EQUAL_STRING(a,b,l,m) fail_unless(((a == b) || ((a != NULL) && (b != NULL) && 0 == strcmp(a,b))), m)
#define UNITY_TEST_ASSERT_EQUAL_INT(a,b,l,m) fail_unless(a == b, m)
#define UNITY_TEST_ASSERT_EQUAL_INT8(a,b,l,m) fail_unless(a == b, m)
#define UNITY_TEST_ASSERT_EQUAL_PTR(a,b,l,m) fail_unless(a == b, m)
#define UNITY_TEST_ASSERT_EQUAL_HEX32(a,b,l,m) fail_unless(a==b, m)
#define UNITY_TEST_ASSERT_EQUAL_HEX16(a,b,l,m) fail_unless(a==b, m)
#define UNITY_TEST_ASSERT_EQUAL_HEX8_ARRAY(e,a,n,l,m) fail_unless(0 == memcmp(e,a,n), m)
#define UNITY_TEST_ASSERT_EQUAL_FLOAT(a,b,l,m) fail_unless(a==b, m)

#define UNITY_TEST_ASSERT_EQUAL_INT8_ARRAY(e,a,n,l,m) {\
    int i; \
    for( i = 0; i < n; ++i ) { \
      if (e[i] != a[i]) break; \
    } \
    fail_unless(i == n, m); \
  } \

#define UNITY_TEST_ASSERT_EQUAL_STRING_ARRAY(e,a,n,l,m) {\
    int i; \
    for( i = 0; i < n; ++i ) { \
      if ((e[n] != a[n]) && (0 != strcmp(e[n], a[n]))) break; \
    } \
    fail_unless(i == n, m); \
  } \


#ifdef NO_TEST_ASSERT_EQUAL_MEMORY_MESSAGE
#define TEST_ASSERT_EQUAL_MEMORY_MESSAGE(a,b,n,m) (void)a;(void)b
#else
/* compilation of this fails with the mock of TwinOaks' CoreDX */
#define TEST_ASSERT_EQUAL_MEMORY_MESSAGE(a,b,n,m) fail_unless(0 == memcmp(a,b,n), m)
#endif

/* Unsupported checks (always succeed) */

//#define TEST_ASSERT_EQUAL_MEMORY_ARRAY_MESSAGE(a,b,n,d,m) (void)a;(void)b;(void)d
