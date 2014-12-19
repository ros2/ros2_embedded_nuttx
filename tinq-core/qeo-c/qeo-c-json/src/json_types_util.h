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

#ifndef JSON_TYPES_UTIL_H_
#define JSON_TYPES_UTIL_H_

#include <qeocore/api.h>
#include <jansson.h>
#include "qeo_json_p.h"


/**
 * Build qeocore_types from a JSON string type description.
 *
 * \param[in] factory The factory 
 * \param[inout] typedesc in : The original json type description decoded from
 *                             json string
 *                        out: Modified json type, member id's are added, which
 *                             corresponds to the qeocore_type members. These
 *                             values are added for later reference when converting
 *                             json data to and from qeocore_data in the
 *                             data_from_json and json_from_data calls.
 *
 * \retval top qeocore_type of qdm 
 */
qeocore_type_t * types_from_json(const qeo_factory_t  *factory,
                                json_t               *typedesc);

/**
 * Build a qeocore_data_t from a json string with object values
 *
 * \param[in] typedesc  The json type description containing corresponding
 *                      qeocore_type member id's, which were obtained from the
 *                      type_from_json call.
 * \param[in] json_data The json data
 * \param[inout] data   The qeocore_data_t structure to be filled
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EFAIL when setting the value failed
 */
qeo_retcode_t data_from_json(const json_t   *typedesc,
                             json_t         *json_data,
                             qeocore_data_t *data);

/**
 * Build a json string from a qeocore_data_t
 *
 * \param[in] typedesc  The json type description containing corresponding
 *                      qeocore_type member id's, which were  obtained from the
 *                      type_from_json call.
 * \param[in] data      The qeocore_data_t structure to be parsed
 *
 * \return A JSON-encoded string with the value from data, NULL on failure.
 *         It is up to the caller to free the string once he is done with it.
 */
char *json_from_data(const json_t         *typedesc,
                     const qeocore_data_t *data);

/**
 * Fill in a qeo identity struct based on a json string
 *
 * \param[in] id            The qeo identity struct to fill in
 * \param[in] idstring      The qeocore_data_t structure to be parsed
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EFAIL when setting the value failed
 * \retval ::QEO_ENOMEM not enough memory
 */
qeo_retcode_t qeo_identity_from_json(qeo_identity_t **id,
                                     const char     *idstring);

#endif /* JSON_TYPES_UTIL_H_ */
