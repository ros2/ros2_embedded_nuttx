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


/** \file
 * Qeo JSON API.
 */

/*#######################################################################
#                   HEADER (INCLUDE) SECTION                            #
########################################################################*/
#ifndef QEO_JSON_ASYNC_API_H_
#define QEO_JSON_ASYNC_API_H_
#include <stdint.h>
#include <qeo/error.h>

/*#######################################################################
#                       TYPE SECTION                                    #
########################################################################*/

/**
 * Opaque structure representing an Asynchronous Qeo JSON context.
 */
typedef struct qeo_json_async_ctx_s qeo_json_async_ctx_t;

/**
 * Callback called whenever a requested call was executed. 
 *
 * \param[in] ctx Asynchronous Qeo JSON context.
 * \param[in] userdata Opaque user data as provided during async json call.
 * \param[in] id Identifier of the object that initiated the call originally.
 * \param[in] event Original event that was provided OR the error event.
 * \param[in] json_data json_data \htmlonly  Additional data in JSON format containing the result of the call. <br><br>
 * <table border="1">
 * <tr>
 * <th>event</th><th align="center">applicable entity</th><th align="center">JSON structure</th>
 * </tr><tr>
 * <td>error</td><td align="center">all</td><td>{"error":"message"}</td>
 * </tr><tr>
 * <td>data</td><td align="center">eventReader, stateChangeReader</td><td>see "properties" of type description</td>
 * </tr><tr>
 * <td>noMoreData</td><td align="center">eventReader, stateChangeReader</td><td></td>
 * </tr><tr>
 * <td>policyupdate</td><td align="center">all</td><td>{"users":[{"id":123, "allow":true},{"id":456, "allow":false},..]}</td>
 * </tr><tr>
 * <td>remove</td><td align="center">stateChangeReader</td><td>see "properties" of type description (only key fields have valid values)</td>
 * </tr><tr>
 * <td>update</td><td align="center">stateReader</td><td></td>
 * </tr>
 * </table> \endhtmlonly 
 */
typedef void (*qeo_json_async_event_callback)(const qeo_json_async_ctx_t *ctx, uintptr_t userdata, const char *id, const char *event, const char *json_data);

/**
 * The Asynchronous Qeo JSON listener callback structure. Upon reception of a 
 * new Asynchronous Qeo JSON context the appropriate callbacks will be called.
 */
typedef struct {
    /** 
     * Callback handler 
     * \see ::qeo_json_async_event_callback
     */
    qeo_json_async_event_callback on_qeo_json_event;
} qeo_json_async_listener_t;


/*########################################################################
#                       API FUNCTION SECTION                             #
########################################################################*/

/**
 * Creates an Asynchronous Qeo JSON context that can be used for interactiong with the Qeo system.
 * All interactions with this context is done asynchronously. 
 * The Async context instance must be properly closed if it is not needed anymore.  
 * This will free any allocated resources associated with that context.
 * 
 * \param[in] listener The listener to use for reception of notifications
 *                     (non-\c NULL).  The qeo_json_async_listener_t::on_qeo_json_event 
 *                     callback should also be non-\c NULL.
 * \param[in] userdata Opaque user data as provided during Async context creation.
 *
 * \retval A pointer to a qeo_json_async_ctx_t object will be returned when the function completes successfully.
 *         Otherwise NULL is returned.
 *
 *
 * \see ::qeo_json_async_close
 */
qeo_json_async_ctx_t *qeo_json_async_create(const qeo_json_async_listener_t *listener, uintptr_t userdata);

/**
 * Close the Asynchronous Qeo JSON context and release any resources associated with it.
 *
 * \warning This function can take some time to complete if a callback is
 *          currently being called. Resource clean up can only happen after
 *          the callback has finished.
 *
 * \param[in] ctx  The Async context to be closed.
 */
void qeo_json_async_close(qeo_json_async_ctx_t *ctx);

/**
 * Perform an asynchronous call on the Qeo system.
 *
 * \param[in] ctx Asynchronous Qeo JSON context.
 * \param[in] cmd The command to be performed (create, close, write, remove, iterate, policyupdate, policyUpdate, get).
 * \param[in] options options \htmlonly JSON string containing the parameters associated with the given command. <br/>
 *                    JSON tags according to the available commands: e.g. {"id":"req_3566","objType":"factory"} <br><br><table border="1"><th>command</th><th>objType</th><th>&nbsp;id&nbsp;</th><th>factoryId</th><th>typeDesc</th><th>data</th><th>readerId</th><th>  identity</th>
 *   <tr>
 *   <td>create</td><td>factory</td><td align="center">X</td><td></td><td></td><td></td><td></td><td align="center">(opt.) "open"</td>
 *   </tr><tr>
 *   <td>create</td><td>eventReader</td><td align="center">X</td><td align="center">X</td><td align="center">{...}</td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>create</td><td>stateReader</td><td align="center">X</td><td align="center">X</td><td align="center">{...}</td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>create</td><td>stateChangeReader</td><td align="center">X</td><td align="center">X</td><td align="center">{...}</td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>create</td><td>eventWriter</td><td align="center">X</td><td align="center">X</td><td align="center">{...}</td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>create</td><td>stateWriter</td><td align="center">X</td><td align="center">X</td><td align="center">{...}</td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>close</td><td>factory</td><td align="center">X</td><td></td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>close</td><td>eventReader</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>close</td><td>stateReader</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>close</td><td>stateChangeReader</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>close</td><td>eventWriter</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>close</td><td>stateWriter</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>write</td><td>eventWriter</td><td align="center">X</td><td align="center">X</td><td></td><td align="center">{...}</td><td></td><td></td>
 *   </tr><tr>
 *   <td>write</td><td>stateWriter</td><td align="center">X</td><td align="center">X</td><td></td><td align="center">{...}</td><td></td><td></td>
 *   </tr><tr>
 *   <td>remove</td><td>stateWriter</td><td align="center">X</td><td align="center">X</td><td></td><td align="center">{...}</td><td></td><td></td>
 *   </tr><tr>
 *   <td>iterate</td><td>iterator</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td align="center">X</td><td></td>
 *   </tr><tr>
 *   <td>policyUpdate</td><td>eventReader</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>policyUpdate</td><td>stateReader</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>policyUpdate</td><td>stateChangeReader</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>policyUpdate</td><td>eventWriter</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>policyUpdate</td><td>stateWriter</td><td align="center">X</td><td align="center">X</td><td></td><td></td><td></td><td></td>
 *   </tr><tr>
 *   <td>policyupdate </td><td>eventReader</td><td align="center">X</td><td align="center">X</td><td></td><td align="center">{...}</td><td></td><td></td>
 *   </tr><tr>
 *   <td>policyupdate </td><td>stateReader</td><td align="center">X</td><td align="center">X</td><td></td><td align="center">{...}</td><td></td><td></td>
 *   </tr><tr>
 *   <td>policyupdate </td><td>stateChangeReader</td><td align="center">X</td><td align="center">X</td><td></td><td align="center">{...}</td><td></td><td></td>
 *   </tr><tr>
 *   <td>policyupdate </td><td>eventWriter</td><td align="center">X</td><td align="center">X</td><td></td><td align="center">{...}</td><td></td><td></td>
 *   </tr><tr>
 *   <td>policyupdate </td><td>stateWriter</td><td align="center">X</td><td align="center">X</td><td></td><td align="center">{...}</td><td></td><td></td>
 *   </tr><tr>
 *   <td>get </td><td>deviceid</td><td align="center">X</td><td align="center">X</td><td></td><td align="center">{...}</td><td></td><td></td>
 *   </tr>
 *</table><br>
 *   X&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;: value to be filled <br>
 *   (opt.): optional parameter <br>
 *   {...}&nbsp;&nbsp;: JSON string<br><br>
 *   <b>Note</b> : JSON string for policy updates is of the form {"users":[{"id":123,"allow":true},{"id":432,"allow":false},...]}<br>
 *   &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;JSON string for writing is the same as the "properties" field of the type description.<br>
 *   &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;In case of writing states, all fields are mandatory.<br>
 *   &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;For removing states the JSON string must contain all fields to identify the instance to be removed 
 * \endhtmlonly 
 *
 * \retval ::QEO_OK on successful delivery of the call
 * \retval ::QEO_EINVAL in case of invalid arguments
 * \retval ::QEO_EFAIL in case internal failures
 * \retval ::QEO_ENOMEM not enough memory available 
 */
qeo_retcode_t qeo_json_async_call(qeo_json_async_ctx_t *ctx, const char *cmd, const char *options);

#endif
