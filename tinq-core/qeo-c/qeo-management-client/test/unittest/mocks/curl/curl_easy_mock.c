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
#include <curl/curl.h>
#include <curl/easy.h>
#include <qeo/log.h>
#include "unittest/unittest.h"
#include "curl_easy_mock.h"

/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/
typedef struct {
    int initcalled;
    int performcalled;
    int setoptcalled;
    int getinfocalled;
    int cleancalled;
    int resetcalled;
} curl_mock_struct;

/*#######################################################################
#                   STATIC FUNCTION DECLARATION                         #
########################################################################*/

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/
curl_mock_struct s_ctx;
CURLcode s_global_init;
bool s_easy_init;
CURLcode s_setopt;
CURLcode s_getinfo;
long s_httpstatuscode;
int s_httpstatuscodecnt;
CURLcode s_perform;
curl_ssl_ctx_callback s_ssl_cb;
void *s_ssl_data;
curl_write_callback s_data_cb;
void *s_write_data;
void *s_read_data;
curl_read_callback s_read_data_cb;
curl_write_callback s_header_cb;
void *s_header_data;
static bool s_first;
char* s_root_resource=
        "{\n"
        "\t\"href\" : \"http://join.qeo.org/\",\n"
        "\t\"management\" : {\n"
        "\t\t\"href\" : \"https://join.qeo.org:8442/qeo-rest-service\"\n"
        "\t},\n"
        "\t\"PKI\" : {\n"
        "\t\t\"scep\" : {\n"
        "\t\t\t\"href\" : \"https://join.qeo.org:8442/ra/scep/pkiclient.exe\"\n"
        "\t\t}\n"
        "\t},\n"
        "\t\"location\" : {\n"
        "\t\t\"forwarders\" : {\n"
        "\t\t\t\"href\" : \"https://join.qeo.org:8443/pull/forwarders\"\n"
        "\t\t}\n"
        "\t},\n"
        "\t\"policy\" : {\n"
        "\t\t\"check\" : {\n"
        "\t\t\t\"href\" : \"http://join.qeo.org/pull/checkpolicy\"\n"
        "\t\t},\n"
        "\t\t\"pull\" : {\n"
        "\t\t\t\"href\" : \"https://join.qeo.org:8443/pull/policy\"\n"
        "\t\t}\n"
        "\t}\n"
        "}\n";
char* s_message = NULL;
bool s_skipfirst = false;

char s_put_message[4096];
size_t s_put_message_size = 0;

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

static void check_int_called(int expected, int real, int linenumber)
{
    if (expected == CURL_EASY_MOCK_CHECK_CALLED){
        fail_if(real <= 0, "This function is not called enough.(linenumber: %d)", linenumber);
    } else if (expected >= 0){
//        Uncomment to trigger segfault on fail. USe gdb to get the backtrace of where it went wrong...
//        if (expected != real) {
//            fprintf(stderr, "Failing test Expected %d is different from real value %d.(linenumber: %d)", expected, real, linenumber);
//            long long int r = real;
//            char* s = (char*) r;
//            real = strlen(s);
//        }
        fail_unless(expected == real, "Expected %d is different from real value %d.(linenumber: %d)", expected, real, linenumber);
    }
}

/*#######################################################################
#                   PUBLIC CURL FUNCTION IMPLEMENTATION                 #
########################################################################*/

CURLcode curl_global_init(long flags){
    return s_global_init;
}

void curl_global_cleanup(void){

}

CURL *curl_easy_init(void){
    if (s_easy_init == false) return NULL;
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.initcalled++;
    curl_easy_mock_clean();
    s_first = true;
    return (void*) &s_ctx;
}

#undef curl_easy_setopt
CURLcode curl_easy_setopt(CURL *curl, CURLoption tag, ...)
{
  fail_unless(curl == &s_ctx);
  va_list arg;
  va_start(arg, tag);
  ((curl_mock_struct*) curl)->setoptcalled++;
  if (tag == CURLOPT_SSL_CTX_FUNCTION){
      s_ssl_cb=(curl_ssl_ctx_callback) va_arg(arg,curl_ssl_ctx_callback);
  }
  if (tag == CURLOPT_SSL_CTX_DATA){
      s_ssl_data=va_arg(arg,void*);
  }
  if (tag == CURLOPT_WRITEFUNCTION){
      s_data_cb=(curl_write_callback) va_arg(arg,curl_write_callback);
  }
  if (tag == CURLOPT_READFUNCTION){
      s_read_data_cb=(curl_read_callback) va_arg(arg,curl_read_callback);
  }
  if (tag == CURLOPT_READDATA){
        s_read_data=va_arg(arg,void*);
    }
  if (tag == CURLOPT_WRITEDATA){
      s_write_data=va_arg(arg,void*);
  }
  if (tag == CURLOPT_HEADERFUNCTION){
      s_header_cb=va_arg(arg,void*);
  }
  if (tag == CURLOPT_WRITEHEADER){
      s_header_data=va_arg(arg,void*);
  }
  va_end(arg);
  return s_setopt;
}

#undef curl_easy_getinfo
CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ...){
    fail_unless(curl == &s_ctx);
    va_list arg;
    va_start(arg, info);
    ((curl_mock_struct*) curl)->getinfocalled++;
    if (info == CURLINFO_RESPONSE_CODE){
        if (s_httpstatuscodecnt <= 0){
            long *statuscode = va_arg(arg, long*);
            *statuscode = s_httpstatuscode;
        } else {
            long *statuscode = va_arg(arg, long*);
            *statuscode = 0;
            s_httpstatuscodecnt--;
        }
    }
    va_end(arg);
    return s_getinfo;
}

CURLcode curl_easy_perform(CURL *curl){
    fail_unless(curl == &s_ctx);
    ((curl_mock_struct*) curl)->performcalled++;
    if (s_ssl_cb){
        s_ssl_cb(curl, (void*) 1, s_ssl_data);
    }
    if (s_read_data_cb) {
        s_read_data_cb(s_put_message, sizeof(s_put_message), 1, s_read_data);
    }
    if (s_data_cb) {
        if ((s_message) && ((s_skipfirst == false) || (s_first == false))) {
            qeo_log_i("Data callback returning custom message<%s>", s_message);
            s_data_cb(s_message, sizeof(char), strlen(s_message), s_write_data);
        }
        else {
            if (s_first) {
                qeo_log_i("Data callback returning default root resource");
                s_data_cb(s_root_resource, sizeof(char), strlen(s_root_resource), s_write_data);
            }
            else {
                qeo_log_i("Data callback stubbed");

                s_data_cb(NULL, 0, 0, s_write_data);
            }
        }
        s_first = false;
    }
    if (s_header_cb) {
        s_header_cb("X-qeo-correlation: blabla", 10, 5, s_header_data);
    }
    return s_perform;
}

void curl_easy_reset(CURL *curl){
    fail_unless(curl == &s_ctx);
    ((curl_mock_struct*) curl)->resetcalled++;
    s_ssl_cb = NULL;
    s_ssl_data = NULL;
}

void curl_easy_cleanup(CURL *curl){
    fail_unless(curl == &s_ctx);
    ((curl_mock_struct*) curl)->cleancalled++;
}

const char *curl_easy_strerror(CURLcode cc){
    return "TODO";
}

struct curl_slist *curl_slist_append(struct curl_slist * list,
                                                 const char * header){
    fail_if(header == NULL);
    return (struct curl_slist *)7;
}

void curl_slist_free_all(struct curl_slist *list){
    fail_unless(list == (struct curl_slist *)7);
}

/*#######################################################################
#                   PUBLIC MOCK FUNCTION IMPLEMENTATION                 #
########################################################################*/


void curl_easy_mock_expect_called(int init, int setopt, int getinfo, int perform, int reset, int clean){
    check_int_called(init, s_ctx.initcalled, __LINE__);
    check_int_called(setopt, s_ctx.setoptcalled, __LINE__);
    check_int_called(getinfo, s_ctx.getinfocalled, __LINE__);
    check_int_called(perform, s_ctx.performcalled, __LINE__);
    check_int_called(reset, s_ctx.resetcalled, __LINE__);
    check_int_called(clean, s_ctx.cleancalled, __LINE__);
}

void curl_easy_mock_ignore_and_return(CURLcode global_init, bool easy_init, CURLcode setopt, CURLcode getinfo, CURLcode perform){
    s_global_init = global_init;
    s_easy_init = easy_init;
    s_setopt = setopt;
    s_getinfo = getinfo;
    s_perform = perform;
}

void curl_easy_mock_return_getinfo(long httpstatuscode, int cnt){
    s_httpstatuscode = httpstatuscode;
    s_httpstatuscodecnt = cnt;
}

void curl_easy_mock_return_data(char* message, bool skipfirst){
    s_message = message;
    s_skipfirst = skipfirst;
}

char* curl_easy_mock_get_uploaded_data(){
    return s_put_message;
}

void curl_easy_mock_clean(void){
    s_ssl_cb = NULL;
    s_ssl_data = NULL;
    s_data_cb = NULL;
    s_write_data = NULL;
    s_read_data_cb = NULL;
    s_read_data = NULL;
    s_httpstatuscode = 0;
    s_httpstatuscodecnt = 0;
    s_put_message_size = 0;
    s_skipfirst = false;
}


