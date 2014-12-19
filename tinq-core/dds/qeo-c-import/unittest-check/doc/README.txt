Last update: Mar 28, 2011

Unit tests
----------

** Overview of the unit test framework

Note: the names "unit test" and "test case" are used interchangeable in this
text. The unittest tool itself uses the test case (tcase) terminology.
The unittestCheck component contains the code of the unittest utility program.
It uses the open source "Check" infrastructure to simplify test writing.
To be able to use this program in your component, you need to depend on it in
your component's Makefile_component. E.g. for the rebusCRUD component:
rebusCRUD_TEST_DEPS += log check unittestCheck

When you compile a test build of your component, the unittest utility should
be located in the bin directory of your test build.

How you structure the source code of your unit tests within the component is not
fixed. However, it might be useful to use a structure similar to the one used
for the unit tests of these components: dds, ddsAL and rebusCRUD.
The base directory for a component's unit tests is the directory "test/unit"
(cfr. the core middleware build environment). 

Unit tests are divided into test cases that are a part of test suites
(cfr. check). All test suites are located in the subdirectory testsuites.
A specific test suite is contained in a subdirectory with prefix "suite_", a
testcase is located inside a subdirectory of the test suite directory, prefixed
with "tcase_". For each test suite, the C files can be selected that you want
to compile into the unit test. This way, different files can be selected for
different test suites. These files are selected in the Makefile_component file
of the component. Example taken from the RebusCRUD's Makefile_component file:

rebusCRUD.librebuscrud_SRCS          := $(addprefix librebusCRUD:src/, rebuscrud.c) \
                                        $(addprefix test/unit/testsuites/stubs_, resources.c) \
                                        $(addprefix test/unit/testsuites/suite_rebuscrud/, main.c) \
                                        $(addprefix test/unit/testsuites/suite_rebuscrud/tcase_simpletests/, process_samples.c crud_request.c header_copy.c call_provider.c) \
                                        $(addprefix test/unit/mocks/manualMock, dds.c log.c) \
                                        $(addprefix test/unit/mocks/Mock, rebusdds.c dds.c ddsAL_extras.c)
rebusCRUD.librebuscrud_LOCALINCLUDES := test/unit test/unit/mocks src sample $(check_INCLUDES) $(rebusCRUD_INCLUDES) $(log_INCLUDES) $(ddsAL_INCLUDES) $(unittestCheck_INCLUDES)
rebusCRUD.librebuscrud_LOCALDEFINES  := $(REBUS_CFLAGS) $(ddsAL_CFLAGS)
rebusCRUD.librebuscrud_LDADD         := $(check_LIBS)
rebusCRUD.librebuscrud_LDDEPS        := librebusCRUD.so

In the above example, src/rebuscrud.c is the actual source file we wish to unit
test. All the other files in rebusCRUD.librebuscrud_SRCS consist of the actual
unit tests and stubs. the LDADD build variable should at least contain the
libraries needed by check: $(check_LIBS)

To actually compile the test suite for a test build, the library has to be added
to $TEST_PARTS_INSTALL, e.g.:
rebusCRUD_TEST_PARTS_INSTALL +=
	librebuscrud.so@$$(libdir/)unittests/librebuscrud.so:755

Each test suite is compiled into a shared object. In the above example, this is
librebuscrud.so.
The unittest program is a small generic program, used to dlopen these .so files
and run the test cases.

The code of a test suite must contain an initialization function that is
executed when the shared object is loaded. This is achieved by adding the
attribute "constructor" to the function. This function calls
register_testsuite() in order to register the test suite to the main unittest
program. Example:

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}

For more details, we refer to the already existing unit tests (see components
dds, ddsAL and rebusCRUD) and the header file unittest.h.

** Debugging unit tests

Using the build target recursive_test_install, the unittest program and the
.so files for each test suite will be installed in the $INSTALL_DIR.
The build target coverage_test_run is not well suited for debugging your unit
tests. Only use that target to verify if all unit tests pass.

The unittest utility program can be used directly to execute specific unit
tests. For more specific information, run the unittest command with the option
"-h".

By default, the unit test main program will fork before running a test case.
Because ddd doesn't handle this fork very well, we need to disable forking when
debugging in ddd. Running one specific test case in ddd without forking :
First make sure you are using the correct check library:
# Go to the directory containing the unittest binary:
# ddd ./unittest
Before running the program, first set the command line arguments, e.g.:
--suitedir ../lib/unittests cdr --tcasecoll simpletests --tcase cdr_sequences3 --nf --nml

With the above command line arguments, the test case named cdr_sequences3, which
is a part of the test case collection simpletests of the test suite cdr will
be executed without forking and without memory checking.
Also before starting the test, put a breakpoint after the dlopen in unittest.c.
When the program reaches this breakpoint, you will be able to open the source
files compiled for the dynamically linked test suite and you can set breakpoints
in the specific test case.

Typing in the command line arguments can be a hastle. You can let the unittest
tool print the needed command line arguments for you.
To print the arguments for each test case, use this command:
#./unittest --suitedir ../lib/unittests/ --all -v --exec -h
Once you have many unit tests, the output of the above command can be big.
You can delimit the output by specifying a specific test suite, e.g.:
#./unittest --suitedir ../lib/unittests/ rebus -v --exec -h
You can even narrow it down to a specific test case collection:
#./unittest --suitedir ../lib/unittests/ rebus -v --exec -h --tcasecoll simpletests

Setting the breakpoint each time you start ddd can also be a nuissance. You can
easily automate this by using a .gdbinit file, located where your unittest
program is located.
This file might e.g. look like this if you're starting ddd a lot lately for the
"test request" unit test (test_request is the function containing the unit test):
--->
set args --suitedir ../lib/unittests/ rebusreqrsp --tcasecoll "process samples" --tcase "test request" --nf --nml
break 414
run
break test_request
<---
Note that line 414 contains the call to srunner_run_all(). This line number
might change in the future without this being reflected in this readme. You can
pick any line number after the dlopen and before the call to srunner_run_all(). 

Memory leak checking:
If mtrace is installed the unittest tool will automatically check for memory
leaks in each testcase. This feature is not available on all targets.
It is best to run the tests with the option --nml (no memory leak testing) if
the --nf (no fork) option is also specified in order to prevent false positives
caused by the check framework.
A file named unittest_mcheck_out is created that contains information about the
found memory leaks (if any). A file named unittest_mcheck is created that
contains all allocated and freed memory locations. These files are most useful
when exactly one unit test is run.
For RHEL4, the mtrace rpm is locally available at: ~deschuymerb/tmp/mtrace-2.2.5-17.i586.rpm
Install it with
#sudo rpm -i --force --nodeps mtrace-2.2.5-17.i586.rpm
For RHEL6, the mtrace utility is part of glibc-utils, install it with:
#sudo yum install glibc-utils.i686

Running with valgrind:
Excerpt from the valgrind website (http://valgrind.org/help/projects.html):
"Valgrind unloads debugging information for shared objects when they are
unloaded with dlclose(). If the shared object has a memory leak, the stack trace
for Memcheck's error message at termination will be missing entries, which makes
fixing the leak difficult."
The option --ndc (no dlclose) can be used to prevent the unittest tool from
dlclosing the opened shared object file that contains a test suite. This allows
valgrind to print a more useful stack trace.
