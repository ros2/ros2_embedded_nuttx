#ifndef __UNITTEST_H__
#define __UNITTEST_H__
#include <check.h>

/** Available flags when registering a test suite */
typedef enum {
    unittestcheck_suiteflags_mtrace_nofixture = 1, /*< Don't include the fixture setup/teardown functions in mtrace coverage. */
} unittestcheck_suiteflags;

/**
 * Structure used to register a test suite.
 */
typedef struct testSuiteInfo {
    Suite *suite; /**< Pointer to the test suite. */
    char *name; /**< Name of the test suite. */
    char *desc; /**< Description of the test suite. */
    struct testSuiteInfo *next; /**< Next structure in the linked list. */
    int flags; /*< Additional flags, see ::unittestcheck_suiteflags. */
} testSuiteInfo;

/**
 * Structure used to register a test case collection.
 */
typedef struct testCaseInfo {
    void (*register_testcase)(Suite *s); /**< Function that registers the test
                                              case collection. */
    char *name; /**< Name of the test case collection. */
} testCaseInfo;

/**
 * Structure used to describe a single test case
 */
typedef struct singleTestCaseInfo {
    TFun function; /**< The function to be executed by check. */
    char *name; /**< The name of the test case. */
    int  start; /**< Start value of a loop testcase.
                     (Only use for loop test cases.) */
    int  end; /**< End value of a loop testcase.
                   (Only use for loop test cases.) */
} singleTestCaseInfo;

/**
 * This function registers the test suite described by \a testsuite, including
 * all its test case collections.  This function should be called by the
 * constructor of the test suite library.  Depending on the way the unit test
 * program is run, not all test cases might be registered for execution.
 *
 * \param[in] testsuite The allocated but empty test suite.
 * \param[in] testcases The null pointer terminated array of test case
 *                      collections.
 * \param[in] init The function executed before a unit test is run (fixture setup function).
 * \param[in] fini The function executed after a unit test has run (fixture teardown function).
 */
void register_testsuite(testSuiteInfo *testsuite,
                        testCaseInfo *testcases,
                        void (*init)(void),
                        void (*fini)(void));

/**
 * This function adds the test case collection \a tc to the test suite
 * registered with ::register_testsuite.  The test case collection should be
 * empty. All test cases in the collection are described in the null pointer
 * terminated array \a tests.  This function should be called by the designated
 * registration function of the test case collection pointed to by the
 * \a register_testcase member of ::testCaseInfo.  Depending on the way the unit
 * test program is run, not all test cases might be registered for execution.
 *
 * \param[in] tc The allocated but empty test case collection.
 * \param[in] tests The null pointer terminated array of to be registered test
 *            cases.
 */
void tcase_addtests(TCase *tc, singleTestCaseInfo *tests);

/**
 * This function adds the test case collection \a tc to the test suite
 * registered with ::register_testsuite.  The test case collection should be
 * empty. All test cases in the collection are described in the null pointer
 * terminated array \a tests.  This function should be called by the designated
 * registration function of the test case collection pointed to by the
 * \a register_testcase member of ::testCaseInfo.  Depending on the way the unit
 * test program is run, not all test cases might be registered for execution.
 * The \a tests are added as check loop tests. This means each testcase is
 * called for each value starting from start to end.
 *
 * \param[in] tc The allocated but empty test case collection.
 * \param[in] tests The null pointer terminated array of to be registered
 *            loop test cases.
 */
void tcase_addlooptests(TCase *tc, singleTestCaseInfo *tests);

/**
 * Disable memory leak checks from within a test case.  By default memory leak
 * checks are enabled.  You can disable them globally by passing the \c --nml
 * argument on the command-line.  This function allows you to disable the checks
 * for a single test case.
 */
void leakcheck_disable(void);

/**
 * Enable memory leak checks from within a test case.  By default memory leak
 * checks are enabled but you could disable them globally by passing the
 * \c --nml argument on the command-line.  This function allows you to enable
 * the checks for a single test case.
 */
void leakcheck_enable(void);

#endif
