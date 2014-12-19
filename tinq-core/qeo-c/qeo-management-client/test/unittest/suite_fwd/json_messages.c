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


char* success_register_fwd[]={
"{ \"locators\": [ ] }\n"
,
"{ \n"
"\t\"locators\": [ \n"
"\t\t{ \n"
"\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\"port\": 7400\n"
"\t\t}\n"
"\t]\n"
"}\n"
,
"{ \n"
"\t\"locators\": [ \n"
"\t\t{ \n"
"\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\"port\": 8080\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"TCPv6\",\n"
"\t\t\t\"address\": \"hostname\",\n"
"\t\t\t\"port\": 7400\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"UDPv4\",\n"
"\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\"port\": 6666\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"UDPv6\",\n"
"\t\t\t\"address\": \"fe80::250:bfff:feb7:c61d\",\n"
"\t\t\t\"port\": 7777\n"
"\t\t}\n"
"\t]\n"
"}\n"
,
"{ \n"
"\t\"locators\": [ \n"
"\t\t{ \n"
"\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\"address\": \"200\",\n"
"\t\t\t\"port\": 100\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"TCPv6\",\n"
"\t\t\t\"address\": \"201\",\n"
"\t\t\t\"port\": 101\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"UDPv4\",\n"
"\t\t\t\"address\": \"202\",\n"
"\t\t\t\"port\": 102\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"UDPv6\",\n"
"\t\t\t\"address\": \"203\",\n"
"\t\t\t\"port\": 103\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\"address\": \"204\",\n"
"\t\t\t\"port\": 104\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"TCPv6\",\n"
"\t\t\t\"address\": \"205\",\n"
"\t\t\t\"port\": 105\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"UDPv4\",\n"
"\t\t\t\"address\": \"206\",\n"
"\t\t\t\"port\": 106\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"UDPv6\",\n"
"\t\t\t\"address\": \"207\",\n"
"\t\t\t\"port\": 107\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\"address\": \"208\",\n"
"\t\t\t\"port\": 108\n"
"\t\t},\n"
"\t\t{ \n"
"\t\t\t\"type\": \"TCPv6\",\n"
"\t\t\t\"address\": \"209\",\n"
"\t\t\t\"port\": 109\n"
"\t\t}\n"
"\t]\n"
"}\n"
,
NULL};
char* success_list_fwds[]={
"{ \"forwarders\": [] }\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"20\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"ab\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}, \n"
"\t\t{\n"
"\t\t\t\"id\": \"2\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"UDPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}, \n"
"\t\t{\n"
"\t\t\t\"id\": \"3\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv6\",\n"
"\t\t\t\t\t\"address\": \"fe80::250:bfff:feb7:c61d\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}, \n"
"\t\t{\n"
"\t\t\t\"id\": \"4\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"UDPv6\",\n"
"\t\t\t\t\t\"address\": \"fe80::250:bfff:feb7:c61d\",\n"
"\t\t\t\t\t\"port\": 8080\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"1\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}, \n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"UDPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.152\",\n"
"\t\t\t\t\t\"port\": 111\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}, \n"
"\t\t{\n"
"\t\t\t\"id\": \"2\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"UDPv6\",\n"
"\t\t\t\t\t\"address\": \"fe80::250:bfff:feb7:c61e\",\n"
"\t\t\t\t\t\"port\": 8080\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}, \n"
"\t\t{\n"
"\t\t\t\"id\": \"3\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv6\",\n"
"\t\t\t\t\t\"address\": \"fe80::250:bfff:feb7:c61d\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}, \n"
"\t\t{\n"
"\t\t\t\"id\": \"4\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"UDPv6\",\n"
"\t\t\t\t\t\"address\": \"fe80::250:bfff:feb7:c61d\",\n"
"\t\t\t\t\t\"port\": 8080\n"
"\t\t\t\t},\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 100\n"
"\t\t\t\t} \n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
NULL};
char* error_list_fwds[]={
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"20\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"20\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": -2\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"20\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 100000\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"20\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\"\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"20\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"bla\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"saf\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"20\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": \"7400\"\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"20\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": 10,\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": \"20\",\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": 10,\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": [ \n"
"\t\t{\n"
"\t\t\t\"id\": 20,\n"
"\t\t\t\"locators\": [\n"
"\t\t\t\t{\n"
"\t\t\t\t\t\"type\": \"TCPv4\",\n"
"\t\t\t\t\t\"address\": \"212.118.224.153\",\n"
"\t\t\t\t\t\"port\": 7400\n"
"\t\t\t\t}\n"
"\t\t\t] \n"
"\t\t}\n"
"\t]\n"
"}\n",
"{\n"
"\t\"forwarders\": \"abcd\"\n"
"}\n",
NULL};

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/
char* get_success_list_fwds(int id){
    return success_list_fwds[id];
}

char* get_error_list_fwds(int id){
    return error_list_fwds[id];
}

char* get_success_register_fwd(int id){
    return success_register_fwd[id];
}
