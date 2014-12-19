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

/********************** WARNING ********************************
 *  This is an automatically generated file, do not edit it directly
 * Instead generate it using the following command,
 * ./testdata/generate_testdata.sh json_messages.c
 *
 * TODO generate the functions to retrieve the data
 */

/*#######################################################################
#                       HEADER (INCLUDE) SECTION                        #
########################################################################*/
#include <stddef.h>
#include "unittest/unittest.h"


/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/

/*#######################################################################
#                   STATIC FUNCTION DECLARATION                         #
########################################################################*/

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/


char* services_messages[]={
"{\n"
"\t\"href\" : \"http://join.qeodev.org/\",\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"location\" : {\n"
"\t\t\"forwarders\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/forwarders\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"check\" : {\n"
"\t\t\t\"href\" : \"http://join.qeodev.org/pull/checkpolicy\"\n"
"\t\t},\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/policy\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
"{\n"
"\t\"href\" : \"http://join.qeodev.org/\",\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"location\" : {\n"
"\t\t\"forwarders\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/forwarders\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/policy\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
NULL};
char* error_messages[]={
"{\n"
"\t\"href\" : \"http://join.qeodev.org/\",\n"
"\t\t\"check\" : {\n"
"\t\t\t\"href\" : \"http://join.qeodev.org/pull/checkpolicy\"\n"
"\t\t},\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/policy\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
"{\n"
"\t\"href\" : \"http://join.qeodev.org/\",\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"location\" : {\n"
"\t\t\"forwarders\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/forwarders\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"check\" : {\n"
"\t\t\t\"href\" : \"http://join.qeodev.org/pull/checkpolicy\"\n"
"\t\t},\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/policy\"\n"
"\t\t}\n"
"\t}\n"
,
"\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"check\" : {\n"
"\t\t\t\"href\" : \"http://join.qeodev.org/pull/checkpolicy\"\n"
"\t\t}\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/policy\",\n"
"\t\t\t\"blab\" : \"blabla\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
"{\n"
"\t\"href\" : \"http://join.qeodev.org/\" :\"test\",\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"location\" : {\n"
"\t\t\"forwarders\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/forwarders\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"check\" : {\n"
"\t\t\t\"href\" : \"http://join.qeodev.org/pull/checkpolicy\"\n"
"\t\t},\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/policy\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
"{\n"
"<<<SDAFd\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"check\" : {\n"
"\t\t\t\"href\" : \"http://join.qeodev.org/pull/checkpolicy\"\n"
"\t\t},\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/policy\",\n"
"\t\t\t\"blab\" : \"blabla\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
NULL};
char* success_messages[]={
"{\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\",\n"
"\t\t\t\"deprecation\" : \"\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
"{\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t},\n"
"\t\t\"deprecation\" : \"\"\n"
"\t}\n"
"}\n",
"{\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"deprecation\" : \"\"\n"
"}\n",
"{\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"check\" : {\n"
"\t\t\t\"href\" : \"http://join.qeodev.org/pull/checkpolicy\"\n"
"\t\t},\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : 20\n"
"\t\t}\n"
"\t}\n"
"}\n",
"{\n"
"\t\"href\" : \"http://join.qeodev.org/\",\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"location\" : {\n"
"\t\t\"forwarders\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/forwarders\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"check\" : {\n"
"\t\t},\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/policy\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
"{\n"
"\t\"href\" : \"http://join.qeodev.org/\",\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"location\" : {\n"
"\t\t\"forwarders\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/forwarders\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"check\" : {\n"
"\t\t\t\"href\" : \"http://join.qeodev.org/pull/checkpolicy\"\n"
"\t\t},\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/policy\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
"{\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"check\" : {\n"
"\t\t\t\"href\" : \"http://join.qeodev.org/pull/checkpolicy\"\n"
"\t\t},\n"
"\t\t\"pull\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8443/pull/policy\",\n"
"\t\t\t\"blab\" : \"blabla\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
"{\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t},\n"
"\t\"policy\" : {\n"
"\t\t\"check\" : {\n"
"\t\t\t\"href\" : \"http://join.qeodev.org/pull/checkpolicy\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
"{\n"
"\t\"PKI\" : {\n"
"\t\t\"scep\" : {\n"
"\t\t\t\"href\" : \"https://join.qeodev.org:8442/ra/scep/pkiclient.exe\"\n"
"\t\t}\n"
"\t}\n"
"}\n",
NULL};

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/
char* get_success_messages(int id){
    return success_messages[id];
}

char* get_error_messages(int id){
    return error_messages[id];
}

char* get_services_messages(int id){
    return services_messages[id];
}
