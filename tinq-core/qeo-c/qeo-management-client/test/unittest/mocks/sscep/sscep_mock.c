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
#include <stdarg.h>
#include <stdbool.h>
#include <qeo/log.h>
#include "unittest/unittest.h"
#include "sscep_mock.h"

/*#######################################################################
 #                       TYPES SECTION                                   #
 ########################################################################*/
typedef struct
{
    int initcalled;
    int performcalled;
    int cleancalled;
} sscep_mock_struct;

/*#######################################################################
 #                   STATIC FUNCTION DECLARATION                         #
 ########################################################################*/

/*#######################################################################
 #                       STATIC VARIABLE SECTION                         #
 ########################################################################*/
sscep_mock_struct s_ctx;
bool s_init;
int s_perform1;
STACK_OF(X509) *s_certs1;
int s_perform2;
STACK_OF(X509) *s_certs2;

/*#######################################################################
 #                   STATIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/
static void check_int_called(int expected, int real, int linenumber)
{
    if (expected == SSCEP_MOCK_CHECK_CALLED) {
        fail_if(real <= 0, "This function is not called enough.(linenumber: %d)", linenumber);
    }
    else if (expected >= 0) {
        ck_assert_int_eq(real, expected);
    }
}

static int sscep_perform_handler(cert_cb cb, int ret, STACK_OF(X509) *certs, void *cookie)
{
    int i = 0;

    if (certs != NULL ) {
        for (i = 0; i < sk_X509_num(certs); ++i) {

            cb(X509_dup(sk_X509_value(certs, i)), cookie);
        }
    }
    return ret;
}

/*#######################################################################
 #                   PUBLIC SSCEP FUNCTION IMPLEMENTATION                #
 ########################################################################*/
sscep_ctx_t sscep_init(int verbose, int debug)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.initcalled++;
    if (s_init == true)
        return (sscep_ctx_t)&s_ctx;
    else
        return NULL ;
}

int sscep_perform(sscep_ctx_t ctx, sscep_operation_info_t operation, cert_cb cb, void *cookie)
{
    fail_unless(ctx == (sscep_ctx_t)&s_ctx);
    switch ((s_ctx.performcalled)++) {
        case 0:
            return sscep_perform_handler(cb, s_perform1, s_certs1, cookie);
        case 1:
            return sscep_perform_handler(cb, s_perform2, s_certs2, cookie);
    }
    fail();
    return -1;
}

void sscep_cleanup(sscep_ctx_t ctx)
{
    if (ctx != NULL){
        fail_unless(ctx == (sscep_ctx_t)&s_ctx);
        s_ctx.cleancalled++;
    }
}

/*#######################################################################
 #                   PUBLIC MOCK FUNCTION IMPLEMENTATION                 #
 ########################################################################*/

void sscep_mock_expect_called(int init, int perform, int clean)
{
    check_int_called(init, s_ctx.initcalled, __LINE__);
    check_int_called(perform, s_ctx.performcalled, __LINE__);
    check_int_called(clean, s_ctx.cleancalled, __LINE__);
}

void sscep_mock_ignore_and_return(bool init,
                                  int perform1,
                                  STACK_OF(X509) *certs1,
                                  int perform2,
                                  STACK_OF(X509) *certs2)
{
    s_init = init;
    s_perform1 = perform1;
    s_certs1 = certs1;
    s_perform2 = perform2;
    s_certs2 = certs2;
}
