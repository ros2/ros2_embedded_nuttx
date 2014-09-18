#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <dirent.h>
#include <ctype.h>
#include <locale.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef MCHECK_SUPPORT
#include <mcheck.h>
#endif
#include <fcntl.h>
#include "unittest/unittest.h"

/** The maximal filename length +1 */
#define FILENAMELEN 256
#define MTRACE_FILE_FULL "./unittest_mcheck"
#define MTRACE_FILE "./unittest_mcheck_out"
/** \hideinitializer */
static SRunner *sr = NULL;
/** Is non-NULL when a specific test case was specified (--tcase). */
static char *specific_tcase;
/** Is non-NULL when a specific test case collection was specified (--tcasecoll). */
static char *specific_tcasecoll;
/** Equals 1 if option --nml was specified, 0 otherwise. */
#ifdef MCHECK_SUPPORT
static int memory_leak_checks = 1;
#else
static int memory_leak_checks = 0;
#endif
/** Equals ::CK_NOFORK if option --nf was specified, ::CK_FORK otherwise. */
static enum fork_status fs = CK_FORK;
/** Directory containing test suite libraries. */
static char *suites_dir = ".";
struct libraries;
/** Structure containing the information about a to be opened shared library. */
typedef struct libraries {
    char fname[FILENAMELEN]; /**< The test library filename. */
    char name[FILENAMELEN]; /**< Name of the suite. */
    struct libraries *next; /**< Pointer to the next library structure in the
                                 linked list. */
} libraries;
/** \hideinitializer */
static libraries shared_libs_sentry = {
        .next = NULL,
};
/** The linked list of shared libraries to be opened. \hideinitializer */
static libraries *shared_libs = &shared_libs_sentry;
/** \hideinitializer */
static struct option options[] =
{
    { "nf"       , no_argument, 0, 2 },
    { "all"      , no_argument, 0, 3 },
    { "tcase"    , required_argument, 0, 4},
    { "tcasecoll", required_argument, 0, 5},
    { "nml"      , no_argument, 0, 6},
    { "ndc"      , no_argument, 0, 8},
    { "suitedir" , required_argument, 0, 7 },
    { "help"     , no_argument, 0, 'h' },
    { "version"  , no_argument, 0, 'V' },
    { "verbose"  , no_argument, 0, 'v' },
    { "exec"     , no_argument, 0, 'e' },
    { 0 }
};
/** \hideinitializer */
static char *help_strings[] =
{
    "If this option is used, no fork is used for the test suites",
    "Runs all test suites located in the test suite directory",
    "Only runs the specified test case from the selected test suite collections",
    "Only runs the test cases from the specified test suite collection",
    "Don't do memory leak checking using mtrace",
    "Don't dlclose the .so (not compatible with --all). Useful for obtaining a\n"
    "stack trace with valgrind.",
    "Location for test suit libraries (current directory by default)",
    "Shows this info",
    "Prints the program version",
    "Verbose output (in combination with -h)",
    "Print the list of unit tests in argument format (in combination with -h and -v)",
};
static int verbose, help_only, help_exec, no_dlclose;
static char *help_string;

/** The function executed in "no fork mode" before a test case is run as a part
 *  of the checked fixture setup.  Registered by ::register_testsuite. */
static void (*tcase_init)(void);
static void (*tcase_fini)(void);
/** Set to 1 if mtrace coverage should not include the registered fixtures */
static int mtrace_inside_fixture;

void register_testsuite(testSuiteInfo *testsuite,
                        testCaseInfo *testcases,
                        void (*init)(void),
                        void (*fini)(void))
{
    int i = 0, registered = 0;
    char *end;

    tcase_init = init;
    tcase_fini = fini;
    if (testsuite->flags & unittestcheck_suiteflags_mtrace_nofixture)
        mtrace_inside_fixture = 1;
    else
        mtrace_inside_fixture = 0;
    if (!sr)
        sr = srunner_create(NULL);
    testsuite->suite = suite_create(testsuite->name);
    if (help_exec)
        end = help_string + strlen(help_string);
    while (testcases[i].register_testcase) {
        if (!specific_tcasecoll || !strcmp(specific_tcasecoll, testcases[i].name)) {
            if (help_exec) {
                strcat(help_string, " --tcasecoll \"");
                strcat(help_string, testcases[i].name);
                strcat(help_string, "\"");
            } else if (help_only)
                printf("\t%s\n", testcases[i].name);
            testcases[i].register_testcase(testsuite->suite);
            registered = 1;
            if (help_exec)
                *end = '\0';
        }
        i++;
    }
    if (!registered)
        printf("No test case collection registered for test suite %s\n", testsuite->name);
    srunner_add_suite(sr, testsuite->suite);
}

void leakcheck_enable(void)
{
#if MCHECK_SUPPORT == 1
    mtrace();
#endif
}

void leakcheck_disable(void)
{
#if MCHECK_SUPPORT == 1
    muntrace();
#endif
}

/** The checked fixture setup function.  It always executes ::tcase_setup_init.
 *  It also initializes mtrace if memory leak checks aren't disabled. */
static void tcase_setup(void)
{
    if (!mtrace_inside_fixture) {
        if (memory_leak_checks)
            leakcheck_enable();
    }
    if (tcase_init) {
        tcase_init();
    }
    if (mtrace_inside_fixture) {
        if (memory_leak_checks)
            leakcheck_enable();
    }
}

/** The checked fixture teardown function.  If memory leak checks are enabled,
 *  it verifies the results from mtrace. */
static void tcase_teardown(void)
{
    static char *p[] = {"unittest", "unittest_mcheck", NULL};
    int fp, status;

    if (!mtrace_inside_fixture) {
        if (tcase_fini) {
            tcase_fini();
        }
    }
    if (memory_leak_checks) {
        leakcheck_disable();
        switch (fork()) {
            case 0:
                fp = open(MTRACE_FILE, O_RDWR | O_TRUNC | O_CREAT, 0644);
                dup2(fp,STDOUT_FILENO);
                execv("/usr/bin/mtrace", p);
                exit(-1);
            default:
                wait(&status);
                /* mtrace returns 1 if there is a memory leak */
                if (WEXITSTATUS(status) == 1) {
                    fail_unless(status == 0, "memory leak");
                } else if (WEXITSTATUS(status) == 255) {
                    printf("Skipping memory leak check: couldn't execute /usr/bin/mtrace.\n"
                            "See INSTALL.txt for information about installing mtrace.\n");
                }
        }
    }
    if (mtrace_inside_fixture) {
        if (tcase_fini) {
            tcase_fini();
        }
    }
}

static void _tcase_addtests(TCase *tc, singleTestCaseInfo *tests, int loop)
{
    int i = 0;

    tcase_add_checked_fixture (tc, tcase_setup, tcase_teardown);
    while (tests[i].function) {
        if (help_exec)
            printf("%s --tcase \"%s\"\n", help_string, tests[i].name);
        else if (help_only)
            printf("\t\t%s\n", tests[i].name);
        else if (!specific_tcase || !strcmp(specific_tcase, tests[i].name))
            _tcase_add_test(tc, tests[i].function, tests[i].name,
                    0, loop ? tests[i].start : 0, loop ? tests[i].end : 1);
        i++;
    }
}

void tcase_addtests(TCase *tc, singleTestCaseInfo *tests)
{
    _tcase_addtests(tc, tests, 0);
}

void tcase_addlooptests(TCase *tc, singleTestCaseInfo *tests)
{
    _tcase_addtests(tc, tests, 1);
}

static void print_avail_suites(void)
{
    libraries *curlib = shared_libs;
    char buf[4096];

    help_string = buf;
    printf("Available test suites:\n");
    if (verbose && !help_exec)
        printf("Layout:\n[test suite]\n\t[test case collection]\n\t\t[test case]\n");
    while (curlib->next) {
        void *handle;

        help_string[0] = '\0';
        if (!help_exec)
            printf("%s\n", curlib->name);
        else
            strcat(help_string, curlib->name);
        if (verbose) {
            if ((handle = dlopen(curlib->fname, RTLD_LAZY)) == NULL) {
                fprintf(stderr, "dlopen didn't work for file %s: %s\n", curlib->fname, dlerror());
                exit(-1);
            }
        }
        curlib = curlib->next;
    }
}

/** Prints usage information to standard output. */
static void print_help(char **argv)
{
    int i = 0;

    printf("Usage: %s [options] [testsuites]\nOptions:\n", argv[0]);
    while (options[i].name) {
        printf("--%s", options[i].name);
        if (isalpha(options[i].val))
            printf(" (-%c)", options[i].val);
        if (options[i].has_arg == required_argument)
            printf(" arg");
        printf(": %s\n", help_strings[i]);
        i++;
    }
    printf("Return value: 0 if all tests passed, -1 otherwise.\n");
    if (shared_libs->next)
        print_avail_suites();
    else
        printf("Use option --all to see all test suites\n");
}

/** Adds a new library to the linked list of test suite libraries. */
static void add_suite(const char *file)
{
    libraries *newlib = (libraries *)malloc(sizeof(libraries));
    char *p;

    assert(NULL != newlib);
    strcpy(newlib->fname, file);
    p = strrchr(file, '/');
    if (!p) {
        printf("Error parsing the test suite directory, make sure it is set properly.\n");
        exit(-1);
    }
    strncpy(newlib->name, p+4, FILENAMELEN-1);
    p = strchr(newlib->name, '.');
    if (!p) {
        printf("Error parsing the test suite directory, make sure it is set properly.\n");
        exit(-1);
    }
    *p = '\0';
    newlib->next = shared_libs;
    shared_libs = newlib;
}

int main(int argc, char **argv)
{
    char file[FILENAMELEN];
    int c;
    libraries *current_shared_lib;
    DIR *dir;
    struct dirent *so;
    FILE *fp;
    int failed = 0;

    putenv("MALLOC_TRACE="MTRACE_FILE_FULL);
    if (argc < 2) {
        print_help(argv);
        exit(-1);
    }
    while ((c = getopt_long(argc, argv, "-hev", options, NULL)) != -1) {
        int ret;
        switch (c) {
        case 'v':
            verbose = 1;
            break;
        case 'h':
            help_only = 1;
            break;
        case 'e':
            help_exec = 1;
            break;
        case 2: /* no fork */
            fs = CK_NOFORK;
            break;
        case 3: /* all */
            dir = opendir(suites_dir);
            assert(NULL != dir);
            while ((so = readdir(dir))) {
                if (so->d_name[0] == '.')
                    continue;
                strcpy(file, suites_dir);
                strcat(file, "/");
                if (strlen(file)+strlen(so->d_name) >= FILENAMELEN) {
                    fprintf(stderr, "suite name length too long: %s (%zu>%zu)\n", so->d_name,
                            strlen(so->d_name), FILENAMELEN-4-strlen(file));
                    exit(-1);
                }
                strcat(file, so->d_name);
                add_suite(file);
            }
            ret = closedir(dir);
            assert(0 == ret);
            break;
        case 4: /* specific test case */
            specific_tcase = optarg;
            break;
        case 5: /* specific test case collection */
            specific_tcasecoll = optarg;
            break;
        case 6: /* no memory leak checks */
            memory_leak_checks = 0;
            break;
        case 7: /* suites directory */
            suites_dir = optarg;
            break;
        case 8: /* no dlclose */
            no_dlclose = 1;
            break;
        case 1: /* assume it's a suite name */
            strcpy(file, suites_dir);
            strcat(file, "/lib");
            if (strlen(file)+strlen(optarg)+3 >= FILENAMELEN) {
                fprintf(stderr, "suite name length too long: %s (%zu>%zu)\n", optarg,
                        strlen(optarg), FILENAMELEN-4-strlen(file));
                exit(-1);
            }
            strcat(file, optarg);
            strcat(file, ".so");
            if ((fp = fopen(file, "r")) == NULL) {
                fprintf(stderr, "Couldn't open library %s\n", file);
                exit(-1);
            }
            ret = fclose(fp);
            assert(0 == ret);
            add_suite(file);
        }
    }

    if (help_exec && (!help_only || !verbose)) {
        printf("Option -e (--exec) only valid in combination with -h and -v\n");
        exit(0);
    }
    if (help_only) {
        print_help(argv);
        exit(0);
    }
    current_shared_lib = shared_libs;
    if (current_shared_lib->next == NULL) {
        printf("No test suites selected, exiting. Use option -h for help.\n");
        exit(0);
    }
    if (no_dlclose && current_shared_lib->next->next != NULL) {
        printf("Option --ndc is only supported when only one test suite is selected.\n");
        exit(0);
    }
    setlocale(LC_ALL, "en_US.UTF-8");
    while (current_shared_lib->next) {
        char buf[128];
        void *handle;

        /* silently ignore failure to remove these files */
        remove(MTRACE_FILE);
        remove(MTRACE_FILE_FULL);
        if ((handle = dlopen(current_shared_lib->fname, RTLD_LAZY | RTLD_GLOBAL)) == NULL) {
            fprintf(stderr, "dlopen didn't work for file %s: %s\n", current_shared_lib->fname, dlerror());
            exit(-1);
        }
        snprintf(buf, sizeof(buf), "testresults-%s.xml", current_shared_lib->name);
        printf("Suite = %s\n", buf);
        srunner_set_xml(sr, buf);
        srunner_set_fork_status(sr, fs);
        srunner_run_all(sr, CK_NORMAL);
        if (srunner_ntests_failed(sr) != 0)
            failed = 1;
        srunner_free(sr);
        sr = NULL;
        if (!no_dlclose)
            dlclose(handle);
        current_shared_lib = current_shared_lib->next;
    }
    while (shared_libs != &shared_libs_sentry) {
        libraries *tmp = shared_libs->next;

        free(shared_libs);
        shared_libs = tmp;
    }
    if (failed)
        return -1;
    return 0;
}
