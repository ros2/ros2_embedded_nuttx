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
 * Qeo API.
 */

#ifndef QEO_API_H_
#define QEO_API_H_

#include <stdint.h>

#include <qeo/error.h>
#include <qeo/factory.h>
#include <qeo/types.h>

#ifdef __cplusplus
extern "C"
{
#endif


/* ===[ Event reader ]======================================================= */

/// \name Qeo Event Reader
/// \{

/**
 * Opaque structure representing a Qeo event reader.
 */
typedef struct qeo_event_reader_s qeo_event_reader_t;

/**
 * Called when a new data sample has arrived.
 *
 * \param[in] reader The reader on which the new data sample has arrived.
 * \param[in] data Incoming sample data.
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_event_on_data_callback)(const qeo_event_reader_t *reader,
                                           const void *data,
                                           uintptr_t userdata);

/**
 * Called when there are no more samples in the current burst.
 *
 * There are situations where ::qeo_event_on_data_callback will be called several
 * times in a short period of time because a burst of samples arrived.  If
 * provided then ::qeo_event_on_no_more_data_callback will be called at the end of
 * such a burst.
 *
 * \param[in] reader The reader for which the burst ended.
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_event_on_no_more_data_callback)(const qeo_event_reader_t *reader,
                                                   uintptr_t userdata);

/**
 * Callback called whenever the policy for a reader is being updated.  This
 * callback enables an application to restrict the list of identities that are
 * allowed for reading.  During a policy update it will be called for each
 * identity that is relevant for this reader.  At the end of the update it will
 * be called with a \c NULL identity to signal the end of the update.
 *
 * \param[in] reader The reader for which the policy is being updated.
 * \param[in] identity The identity that is about to be allowed.
 * \param[in] userdata Opaque user data as provided during reader creation.
 *
 * \retval ::QEO_POLICY_ALLOW Allow this identity and add it to the reader's
 *                            policy.
 * \retval ::QEO_POLICY_DENY Disallow this identity and do not add it to the
 *                           reader's policy.
 */
typedef qeo_policy_perm_t (*qeo_event_reader_on_policy_update_callback)(const qeo_event_reader_t *reader,
                                                                        const qeo_policy_identity_t *identity,
                                                                        uintptr_t userdata);

/**
 * Event reader listener callback structure.  Upon reception of new data or
 * policy updates the appropriate callbacks (if provided) will be called.
 */
typedef struct {
    qeo_event_on_data_callback on_data; /**< see ::qeo_event_on_data_callback */
    qeo_event_on_no_more_data_callback on_no_more_data; /**< see ::qeo_event_on_no_more_data_callback */
    qeo_event_reader_on_policy_update_callback on_policy_update; /**< see ::qeo_event_reader_on_policy_update_callback */
} qeo_event_reader_listener_t;

/**
 * Create a new Qeo event reader for the given type.
 *
 * \param[in] factory  The factory to be used for creating the reader.
 * \param[in] type     The type for which to create the reader (non-\c NULL).
 * \param[in] listener The listener to use for data reception notifications
 *                     (non-\c NULL).  The qeo_event_reader_listener_t::on_data callback
 *                     should also be non-\c NULL.
 * \param[in] userdata Opaque user data that will be provided to the callbacks
 *                     (an integer or \c void pointer).
 *
 * \return The reader or \c NULL on failure.
 *
 * \see ::qeo_event_reader_close
 */
qeo_event_reader_t *qeo_factory_create_event_reader(const qeo_factory_t *factory,
                                                    const DDS_TypeSupport_meta *type,
                                                    const qeo_event_reader_listener_t *listener,
                                                    uintptr_t userdata);

/**
 * Closes the reader, relinquishing any underlying resources.
 *
 * \warning This function can take some time to return if a callback is
 *          currently being called.  Resource clean up can only happen after
 *          the callback has finished.
 *
 * \param[in] reader The reader to close.
 */
void qeo_event_reader_close(qeo_event_reader_t *reader);

/**
 * Call this function to trigger a re-evaluation of the reader's policy.  The
 * registered \c on_update callback (if any) will be called again.
 *
 * \param[in] reader The reader.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_event_reader_policy_update(const qeo_event_reader_t *reader);

/// \}

/* ===[ Event writer ]======================================================= */

/// \name Qeo Event Writer
/// \{

/**
 * Opaque structure representing a Qeo event writer.
 */
typedef struct qeo_event_writer_s qeo_event_writer_t;

/**
 * Callback called whenever the policy for a writer is being updated.  This
 * callback enables an application to restrict the list of identities that are
 * allowed for writing.  During a policy update it will be called for each
 * identity that is relevant for this writer.  At the end of the update it will
 * be called with a \c NULL identity to signal the end of the update.
 *
 * \param[in] writer The writer for which the policy is being updated.
 * \param[in] identity The identity that is about to be allowed.
 * \param[in] userdata Opaque user data as provided during writer creation.
 *
 * \retval ::QEO_POLICY_ALLOW Allow this identity and add it to the writer's
 *                            policy.
 * \retval ::QEO_POLICY_DENY Disallow this identity and do not add it to the
 *                           writer's policy.
 */
typedef qeo_policy_perm_t (*qeo_event_writer_on_policy_update_callback)(const qeo_event_writer_t *writer,
                                                                        const qeo_policy_identity_t *identity,
                                                                        uintptr_t userdata);

/**
 * Event writer listener callback structure.  Upon reception of policy updates
 * the appropriate callbacks (if provided) will be called.
 */
typedef struct {
    qeo_event_writer_on_policy_update_callback on_policy_update; /**< see ::qeo_event_writer_on_policy_update_callback */
} qeo_event_writer_listener_t;

/**
 * Create a new Qeo event writer for the given type.
 *
 * \param[in] factory The factory to be used for creating the writer.
 * \param[in] type    The type for which to create the writer (non-\c NULL).
 * \param[in] listener The listener to use for notifications (may be \c NULL).
 * \param[in] userdata Opaque user data that will be provided to the callbacks
 *                     (an integer or \c void pointer).
 *
 * \return The writer or \c NULL on failure.
 *
 * \see ::qeo_event_writer_close
 */
qeo_event_writer_t *qeo_factory_create_event_writer(const qeo_factory_t *factory,
                                                    const DDS_TypeSupport_meta *type,
                                                    const qeo_event_writer_listener_t *listener,
                                                    uintptr_t userdata);

/**
 * Closes the writer, relinquishing any underlying resources.
 *
 * \param[in] writer The writer to close.
 */
void qeo_event_writer_close(qeo_event_writer_t *writer);

/**
 * Send a new data sample to all subscribed readers.
 *
 * \param[in] writer The writer.
 * \param[in] data   The sample data to write.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL in case of invalid arguments
 * \retval ::QEO_EFAIL in case writing failed
 */
qeo_retcode_t qeo_event_writer_write(const qeo_event_writer_t *writer,
                                     const void *data);

/**
 * Call this function to trigger a re-evaluation of the writer's policy.  The
 * registered \c on_update callback (if any) will be called again.
 *
 * \param[in] writer The writer.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_event_writer_policy_update(const qeo_event_writer_t *writer);

/// \}

/* ===[ State reader ]======================================================= */

/// \name Qeo State Reader
/// \{

/**
 * Opaque structure representing a Qeo state reader.
 */
typedef struct qeo_state_reader_s qeo_state_reader_t;

/**
 * Called when a change in the data is detected.
 *
 * \param[in] reader The reader on which the change was detected.
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_state_on_update_callback)(const qeo_state_reader_t *reader,
                                             uintptr_t userdata);

/**
 * \copydoc ::qeo_event_reader_on_policy_update_callback
 */
typedef qeo_policy_perm_t (*qeo_state_reader_on_policy_update_callback)(const qeo_state_reader_t *reader,
                                                                        const qeo_policy_identity_t *identity,
                                                                        uintptr_t userdata);

/**
 * State reader listener callback structure.  Upon reception of new data the
 * appropriate callbacks (if provided) will be called.
 */
typedef struct {
    qeo_state_on_update_callback on_update; /**< see ::qeo_state_on_update_callback */
    qeo_state_reader_on_policy_update_callback on_policy_update; /**< see ::qeo_state_reader_on_policy_update_callback */
} qeo_state_reader_listener_t;

/**
 * Create a new Qeo state reader for the given type.
 * 
 * \param[in] factory  The factory to be used for creating the reader.
 * \param[in] type     The type for which to create the reader (non-\c NULL).
 * \param[in] listener The listener to use for data reception notifications.
 *                     If this is non-\c NULL then the qeo_state_reader_listener_t::on_update callback
 *                     should also be non-\c NULL.
 * \param[in] userdata Opaque user data that will be provided to the callbacks
 *                     (an integer or \c void pointer).
 *
 * \return The reader or \c NULL on failure.
 *
 * \see ::qeo_state_reader_close
 */
qeo_state_reader_t *qeo_factory_create_state_reader(const qeo_factory_t *factory,
                                                    const DDS_TypeSupport_meta *type,
                                                    const qeo_state_reader_listener_t *listener,
                                                    uintptr_t userdata);

/**
 * Closes the reader, relinquishing any underlying resources.
 *
 * \warning This function can take some time to return if a callback is
 *          currently being called.  Resource clean up can only happen after
 *          the callback has finished.
 *
 * \param[in] reader The reader to close.
 */
void qeo_state_reader_close(qeo_state_reader_t *reader);

/**
 * Called for each instance during iteration.
 *
 * \see ::qeo_state_reader_foreach
 *
 * \param[in] data     Instance data.
 * \param[in] userdata Opaque user data as provided to ::qeo_state_reader_foreach.
 *
 * \return One of ::qeo_iterate_action_t depending on what action to perform
 *         after returning from the callback.
 */
typedef qeo_iterate_action_t (*qeo_iterate_callback)(const void *data, uintptr_t userdata);

/**
 * Iterate over the available instances.
 *
 * \param[in] reader   The reader to be used for iterating.
 * \param[in] cb       Callback to be called for each instance.
 * \param[in] userdata Opaque user data that will be passed to the callback
 *                     (an integer or \c void pointer).
 *                         
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL in case of invalid arguments
 * \retval ::QEO_EFAIL in case reading failed
 */
qeo_retcode_t qeo_state_reader_foreach(const qeo_state_reader_t *reader,
                                       qeo_iterate_callback cb,
                                       uintptr_t userdata);

/**
 * \copydoc ::qeo_event_reader_policy_update
 */
qeo_retcode_t qeo_state_reader_policy_update(const qeo_state_reader_t *reader);

/// \}

/* ===[ State change reader ]================================================ */

/// \name Qeo State Change Reader
/// \{

/**
 * Opaque structure representing a Qeo state change reader.
 */
typedef struct qeo_state_change_reader_s qeo_state_change_reader_t;

/**
 * Called when a new data sample has arrived.
 *
 * \param[in] reader The reader on which the new data sample has arrived.
 * \param[in] data Incoming sample data.
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_state_on_data_callback)(const qeo_state_change_reader_t *reader,
                                           const void *data,
                                           uintptr_t userdata);

/**
 * Called when there are no more samples in the current burst.
 *
 * There are situations where ::qeo_state_on_data_callback will be called several
 * times in a short period of time because a burst of samples arrived.  If
 * provided then ::qeo_state_on_no_more_data_callback will be called at the end of
 * such a burst.
 *
 * \param[in] reader The reader for which the burst ended.
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_state_on_no_more_data_callback)(const qeo_state_change_reader_t *reader,
                                                   uintptr_t userdata);

/**
 * Called when a data instance has been removed. Note that this call will
 * only be made for keyed data.
 *
 * \param[in] reader The reader from which a data instance has been removed.
 * \param[in] data Removed instance data (note that only the fields marked
 *                 as key will have valid data).
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_state_on_remove_callback)(const qeo_state_change_reader_t *reader,
                                             const void *data,
                                             uintptr_t userdata);

/**
 * \copydoc ::qeo_event_reader_on_policy_update_callback
 */
typedef qeo_policy_perm_t (*qeo_state_change_reader_on_policy_update_callback)(const qeo_state_change_reader_t *reader,
                                                                               const qeo_policy_identity_t *identity,
                                                                               uintptr_t userdata);

/**
 * State change reader listener callback structure.  Upon reception of new data
 * the appropriate callbacks (if provided) will be called.
 */
typedef struct {
    qeo_state_on_data_callback on_data; /**< see ::qeo_state_on_data_callback */
    qeo_state_on_no_more_data_callback on_no_more_data; /**< see ::qeo_state_on_no_more_data_callback */
    qeo_state_on_remove_callback on_remove; /**< see ::qeo_state_on_remove_callback */
    qeo_state_change_reader_on_policy_update_callback on_policy_update; /**< see ::qeo_state_change_reader_on_policy_update_callback */
} qeo_state_change_reader_listener_t;

/**
 * Create a new Qeo state change reader for the given type.
 * 
 * \param[in] factory  The factory to be used for creating the reader.
 * \param[in] type     The type for which to create the reader (non-\c NULL).
 * \param[in] listener The listener to use for data reception notifications
 *                     (non-\c NULL).  Either the qeo_state_change_reader_listener_t::on_data
 *                     callback or the qeo_state_change_reader_listener_t::on_remove
 *                     callback should be non-\c NULL.
 * \param[in] userdata Opaque user data that will be provided to the callbacks
 *                     (an integer or \c void pointer).
 *
 * \return The reader or \c NULL on failure.
 *
 * \see ::qeo_state_change_reader_close
 */
qeo_state_change_reader_t *qeo_factory_create_state_change_reader(const qeo_factory_t *factory,
                                                                  const DDS_TypeSupport_meta *type,
                                                                  const qeo_state_change_reader_listener_t *listener,
                                                                  uintptr_t userdata);

/**
 * Closes the reader, relinquishing any underlying resources.
 *
 * \warning This function can take some time to return if a callback is
 *          currently being called.  Resource clean up can only happen after
 *          the callback has finished.
 *
 * \param[in] reader The reader to close.
 */
void qeo_state_change_reader_close(qeo_state_change_reader_t *reader);

/**
 * \copydoc ::qeo_event_reader_policy_update
 */
qeo_retcode_t qeo_state_change_reader_policy_update(const qeo_state_change_reader_t *reader);

/// \}

/* ===[ State writer ]======================================================= */

/// \name Qeo State Writer
/// \{

/**
 * Opaque structure representing a Qeo state writer.
 */
typedef struct qeo_state_writer_s qeo_state_writer_t;

/**
 * \copydoc ::qeo_event_writer_on_policy_update_callback
 */
typedef qeo_policy_perm_t (*qeo_state_writer_on_policy_update_callback)(const qeo_state_writer_t *writer,
                                                                        const qeo_policy_identity_t *identity,
                                                                        uintptr_t userdata);

/**
 * State writer listener callback structure.  Upon reception of policy updates
 * the appropriate callbacks (if provided) will be called.
 */
typedef struct {
    qeo_state_writer_on_policy_update_callback on_policy_update; /**< see ::qeo_state_writer_on_policy_update_callback */
} qeo_state_writer_listener_t;


/**
 * Create a new Qeo state writer for the given type.
 *
 * \param[in] factory The factory to be used for creating the writer.
 * \param[in] type    The type for which to create the writer (non-\c NULL).
 * \param[in] listener The listener to use for notifications (may be \c NULL).
 * \param[in] userdata Opaque user data that will be provided to the callbacks
 *                     (an integer or \c void pointer).
 *
 * \return The writer or \c NULL on failure.
 *
 * \see ::qeo_state_writer_close
 */
qeo_state_writer_t *qeo_factory_create_state_writer(const qeo_factory_t *factory,
                                                    const DDS_TypeSupport_meta *type,
                                                    const qeo_state_writer_listener_t *listener,
                                                    uintptr_t userdata);

/**
 * Closes the writer, relinquishing any underlying resources.
 *
 * \param[in] writer The writer to close.
 */
void qeo_state_writer_close(qeo_state_writer_t *writer);

/**
 * Send a new data sample to all subscribed readers.
 *
 * \param[in] writer The writer.
 * \param[in] data   The sample data to write.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL in case of invalid arguments
 * \retval ::QEO_EFAIL in case writing failed
 */
qeo_retcode_t qeo_state_writer_write(const qeo_state_writer_t *writer,
                                     const void *data);

/**
 * Signal the removal of an instance to all subscribed readers.
 *
 * \param[in] writer The writer.
 * \param[in] data   The instance to be removed.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL in case of invalid arguments
 * \retval ::QEO_EFAIL in case writing failed
 */
qeo_retcode_t qeo_state_writer_remove(const qeo_state_writer_t *writer,
                                      const void *data);

/**
 * \copydoc ::qeo_event_writer_policy_update
 */
qeo_retcode_t qeo_state_writer_policy_update(const qeo_state_writer_t *writer);

/// \}

/* ===[ Utility functions ]================================================== */

/// \name Utility functions
/// \{

/**
 * Converts an enumeration constant's numeric value to its string
 * representation.  The returned string should not be freed.
 *
 * \param[in] enum_type The enumeration type to be used for conversion.
 * \param[in] value     The enumeration constant value to be converted.
 * \param[out] name     A preallocated buffer in which the name will be copied.
 * \param[in] len       The size of the buffer.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid or the value is
 *                      not valid for the enumeration
 * \retval ::QEO_ENOMEM when the buffer is too small to contain the name
 */
qeo_retcode_t qeo_enum_value_to_string(const DDS_TypeSupport_meta *enum_type,
                                       qeo_enum_value_t value,
                                       char *name,
                                       size_t sz);

/**
 * Converts an enumeration constant's string representation to its numeric
 * value.
 *
 * \param[in] enum_type The enumeration type to be used for conversion.
 * \param[in] name      The enumeration constant's string representation to be
 *                      converted.
 * \param[out] value    A pointer to a variable in which to store the value.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid or the name is not
 *                      valid for the enumeration
 */
qeo_retcode_t qeo_enum_string_to_value(const DDS_TypeSupport_meta *enum_type,
                                       const char *name,
                                       qeo_enum_value_t *value);

/// \}

#ifdef __cplusplus
}
#endif

#endif /* QEO_API_H_ */
