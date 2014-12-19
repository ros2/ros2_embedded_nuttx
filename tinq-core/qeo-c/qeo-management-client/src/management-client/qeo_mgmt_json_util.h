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

#ifndef QEO_MGMT_JSON_UTIL_H_
#define QEO_MGMT_JSON_UTIL_H_

#include <qeo/mgmt_client_forwarder.h>
#include <qeo/mgmt_client.h>

typedef struct qeo_mgmt_json_hdl_s *qeo_mgmt_json_hdl_t;

typedef struct
{
    char* value;
    bool  deprecated;
} qeo_mgmt_client_json_value_t;

qeo_mgmt_client_retcode_t qeo_mgmt_json_util_parseGetFWDMessage(const char* data, ssize_t length, qeo_mgmt_client_forwarder_cb callback, void* cookie);

qeo_mgmt_client_retcode_t qeo_mgmt_json_util_formatLocatorData(qeo_mgmt_client_locator_t *locators,
                                                     u_int32_t nrOfLocators,
                                                     char **message,
                                                     size_t *length);


qeo_mgmt_json_hdl_t qeo_mgmt_json_util_parse_message(const char* message, size_t length);

qeo_mgmt_client_retcode_t qeo_mgmt_json_util_get_string(qeo_mgmt_json_hdl_t hdl, const char **query, size_t nrquery, qeo_mgmt_client_json_value_t *value);

void qeo_mgmt_json_util_release_handle(qeo_mgmt_json_hdl_t hdl);
#endif /* QEO_MGMT_JSON_UTIL_H_ */
