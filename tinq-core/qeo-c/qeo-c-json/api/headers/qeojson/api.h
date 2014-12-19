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
#include <stdbool.h>
#include <qeo/error.h>
#include <qeo/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ===[ Qeo factory ]======================================================== */

/// \name Qeo Factory
/// \{

/**
 * Called when a new factory is created.
 *
 * \param[in] factory The created factory or \c NULL on failure.
 * \param[in] userdata Opaque user data as provided during factory creation.
 */
typedef void (*qeo_json_factory_init_callback)(qeo_factory_t *factory,
                                               uintptr_t userdata);

/**
 * The factory listener callback structure. Upon reception of a new factory the
 * appropriate callbacks will be called.
 */
typedef struct {
    qeo_json_factory_init_callback  on_factory_init_done;
} qeo_json_factory_listener_t;

/**
 * Creates a Qeo factory instance for the QEO_IDENTITY_DEFAULT identity that can be 
 * used for creating Qeo readers and writers.  The factory instance should be properly 
 * closed if none of the readers and/or writers that have been created, are needed anymore.
 * This will free any allocated resources associated with that factory.
 * 
 * \param[in] listener The listener to use for reception notifications
 *                     (non-\c NULL).  The qeo_json_factory_listener_t::on_factory_init_done 
 *                     callback should also be non-\c NULL.
 * \param[in] userdata Opaque user data as provided during reader creation.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL in case of invalid arguments
 * \retval ::QEO_EFAIL in case internal failures
 * \retval ::QEO_ENOMEM not enough memory available
 *
 * \see ::qeo_json_factory_close
 */
qeo_retcode_t qeo_json_factory_create(const qeo_json_factory_listener_t *listener, 
                                      uintptr_t userdata);

/**
 * Creates a Qeo factory instance that can be used for creating Qeo readers and
 * writers.  The factory instance should be properly closed if none of the
 * readers and/or writers that have been created, are needed anymore.  This will
 * free any allocated resources associated with that factory.
 *
 * \param[in] identity The qeo identity string for which you want to create the factory.
 *                     FOR NOW : Use "open" for an open identity: QEO_IDENTITY_OPEN
 *                               Any other value will create a default identity: QEO_IDENTITY_DEFAULT
 * \param[in] listener The listener to use for reception notifications
 *                     (non-\c NULL).  The qeo_json_factory_listener_t::on_factory_init_done 
 *                     callback should also be non-\c NULL.
 * \param[in] userdata Opaque user data as provided during reader creation.
 * 
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL in case of invalid arguments
 * \retval ::QEO_EFAIL in case internal failures
 * \retval ::QEO_ENOMEM not enough memory available
 *
 * \see ::qeo_factory_close
 */
qeo_retcode_t qeo_json_factory_create_by_id(const char *identity,
                                            const qeo_json_factory_listener_t *listener, 
                                            uintptr_t userdata);

/**
 * Close the factory and release any resources associated with it.
 *
 * \warning Make sure that any readers and/or writers created with this factory
 *          have been closed before calling this function.
 *
 * \param[in] factory  The factory to be closed.
 */
void qeo_json_factory_close(qeo_factory_t *factory);

/// \}

/* ===[ Event reader ]======================================================= */

/// \name Qeo Event Reader
/// \{

/**
 * Opaque structure representing a Qeo event reader.
 */
typedef struct qeo_event_reader_s qeo_json_event_reader_t;

/**
 * Called when a new data sample has arrived.
 *
 * \param[in] reader The reader on which the new data sample has arrived.
 * \param[in] json_data Incoming sample data.
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_json_event_on_data_callback)(const qeo_json_event_reader_t *reader,
                                                const char *json_data,
                                                uintptr_t userdata);

/**
 * Called when there are no more samples in the current burst.
 *
 * There are situations where ::qeo_json_event_on_data_callback will be called several
 * times in a short period of time because a burst of samples arrived.  If
 * provided then ::qeo_json_event_on_no_more_data_callback will be called at the end of
 * such a burst.
 *
 * \param[in] reader The reader for which the burst ended.
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_json_event_on_no_more_data_callback)(const qeo_json_event_reader_t *reader,
                                                        uintptr_t userdata);

/**
 * Callback called whenever the policy for a reader is being updated.  This
 * callback enables an application to restrict the list of identities that are
 * allowed for reading.  During a policy update it will be called for each
 * identity that is relevant for this reader.  At the end of the update it will
 * be called with a \c NULL identity to signal the end of the update.
 *
 * \param[in] reader The reader for which the policy is being updated.
 * \param[in] json_policy The currently enforced policy.
 * \param[in] userdata Opaque user data as provided during reader creation.
 *
 */
typedef void (*qeo_json_event_reader_on_policy_update_callback)(const qeo_json_event_reader_t *reader,
                                                                             const char *json_policy,
                                                                             uintptr_t userdata);

/**
 * Event reader listener callback structure.  Upon reception of new data the
 * appropriate callbacks (if provided) will be called.
 */
typedef struct {
    qeo_json_event_on_data_callback on_data; /**< see ::qeo_json_event_on_data_callback */
    qeo_json_event_on_no_more_data_callback on_no_more_data; /**< see ::qeo_json_event_on_no_more_data_callback */
    qeo_json_event_reader_on_policy_update_callback on_policy_update; /**< see ::qeo_json_event_reader_on_policy_update_callback */
} qeo_json_event_reader_listener_t;

/**
 * Create a new disabled Qeo event reader for the given type.
 *
 * \param[in] factory  The factory to be used for creating the reader.
 * \param[in] json_type The type for which to create the reader (non-\c NULL).
 * \param[in] listener The listener to use for data reception notifications
 *                     (non-\c NULL).  The qeo_json_event_reader_listener_t::on_data callback
 *                     should also be non-\c NULL.
 * \param[in] userdata Opaque user data that will be provided to the callbacks
 *                     (an integer or \c void pointer).
 *
 * \return The reader or \c NULL on failure.
 *
 * \see ::qeo_json_event_reader_close
 */
qeo_json_event_reader_t *qeo_json_factory_create_event_reader(const qeo_factory_t *factory,
                                                              const char *json_type,
                                                              const qeo_json_event_reader_listener_t *listener,
                                                              uintptr_t userdata);

/**
 * Enable a disabled Qeo event reader.
 *
 * \param[in] reader The reader to enable.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_json_event_reader_enable(qeo_json_event_reader_t *reader);


/**
 * Closes the reader, relinquishing any underlying resources.
 *
 * \warning This function can take some time to return if a callback is
 *          currently being called.  Resource clean up can only happen after
 *          the callback has finished.
 *
 * \param[in] reader The reader to close.
 */
void qeo_json_event_reader_close(qeo_json_event_reader_t *reader);

/**
 * Call this function to trigger a re-evaluation of the reader's policy.  The
 * register \c on_update callback (if any) will be called again.
 *
 * \param[in] reader The reader.
 * \param[in] json_policy The policy to be enforced.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_json_event_reader_policy_update(const qeo_json_event_reader_t *reader, const char* json_policy);

/// \}

/* ===[ Event writer ]======================================================= */

/// \name Qeo Event Writer
/// \{

/**
 * Opaque structure representing a Qeo event writer.
 */
typedef struct qeo_event_writer_s qeo_json_event_writer_t;


/**
 * Callback called whenever the policy for a writer is being updated.  This
 * callback enables an application to restrict the list of identities that are
 * allowed for writing.  During a policy update it will be called for each
 * identity that is relevant for this writer.  At the end of the update it will
 * be called with a \c NULL identity to signal the end of the update.
 *
 * \param[in] writer The writer for which the policy is being updated.
 * \param[in] The currently enforced policy.
 * \param[in] userdata Opaque user data as provided during writer creation.
 *
 */
typedef void (*qeo_json_event_writer_on_policy_update_callback)(const qeo_json_event_writer_t *writer,
                                                                             const char * json_policy,
                                                                             uintptr_t userdata);

/**
 * Event writer listener callback structure.  Upon reception of policy updates
 * the appropriate callbacks (if provided) will be called.
 */
typedef struct {
    qeo_json_event_writer_on_policy_update_callback on_policy_update; /**< see ::qeo_json_event_writer_on_policy_update_callback */
} qeo_json_event_writer_listener_t;

/**
 * Create a new disabled Qeo event writer for the given type.
 *
 * \param[in] factory The factory to be used for creating the writer.
 * \param[in] json_type The type for which to create the writer (non-\c NULL).
 * \param[in] listener The listener to use for policy reception notifications
 *                     (non-\c NULL).  The qeo_json_event_writer_listener_t::on_policy_update callback
 *                     should also be non-\c NULL.
 * \param[in] userdata Opaque user data that will be provided to the callbacks
 *                     (an integer or \c void pointer).
 *
 * \return The writer or \c NULL on failure.
 *
 * \see ::qeo_json_event_writer_close
 */
qeo_json_event_writer_t *qeo_json_factory_create_event_writer(const qeo_factory_t *factory,
                                                              const char *json_type,
                                                              const qeo_json_event_writer_listener_t *listener,
                                                              uintptr_t userdata);
/**
 * Enable a disabled Qeo event writer.
 *
 * \param[in] reader The reader to enable.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_json_event_writer_enable(qeo_json_event_writer_t *writer);

/**
 * Closes the writer, relinquishing any underlying resources.
 *
 * \param[in] writer The writer to close.
 */
void qeo_json_event_writer_close(qeo_json_event_writer_t *writer);

/**
 * Send a new data sample to all subscribed readers.
 *
 * \param[in] writer The writer.
 * \param[in] json_data   The sample data to write.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL in case of invalid arguments
 * \retval ::QEO_EFAIL in case writing failed
 */
qeo_retcode_t qeo_json_event_writer_write(const qeo_json_event_writer_t *writer,
                                          const char* json_data);

/**
 * Call this function to trigger a re-evaluation of the writer's policy.  The
 * register \c on_update callback (if any) will be called again.
 *
 * \param[in] writer The writer.
 * \param[in] The policy to be enforced.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_json_event_writer_policy_update(const qeo_json_event_writer_t *writer,  const char *json_policy);

/// \}

/* ===[ State reader ]======================================================= */

/// \name Qeo State Reader
/// \{

/**
 * Opaque structure representing a Qeo state reader.
 */
typedef struct qeo_state_reader_s qeo_json_state_reader_t;

/**
 * Called when a change in the data is detected.
 *
 * \param[in] reader The reader on which the change was detected.
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_json_state_on_update_callback)(const qeo_json_state_reader_t *reader,
                                                  uintptr_t userdata);

/**
 * Callback called whenever the policy for a reader is being updated.  This
 * callback enables an application to restrict the list of identities that are
 * allowed for reading.  During a policy update it will be called for each
 * identity that is relevant for this reader.  At the end of the update it will
 * be called with a \c NULL identity to signal the end of the update.
 *
 * \param[in] reader The reader for which the policy is being updated.
 * \param[in] policy The currently enforced policy.
 * \param[in] userdata Opaque user data as provided during reader creation.
 *
 */
typedef void (*qeo_json_state_reader_on_policy_update_callback)(const qeo_json_state_reader_t *reader,
                                                                             const char *json_policy,
                                                                             uintptr_t userdata);

/**
 * State reader listener callback structure.  Upon reception of new data the
 * appropriate callbacks (if provided) will be called.
 */
typedef struct {
    qeo_json_state_on_update_callback on_update; /**< see ::qeo_json_state_on_update_callback */
    qeo_json_state_reader_on_policy_update_callback on_policy_update; /**< see ::qeo_json_state_reader_on_policy_update_callback */
} qeo_json_state_reader_listener_t;

/**
 * Create a new disabled Qeo state reader for the given type.
 * 
 * \param[in] factory  The factory to be used for creating the reader.
 * \param[in] json_type The type for which to create the reader (non-\c NULL).
 * \param[in] listener The listener to use for data reception notifications.
 *                     If this is non-\c NULL then the qeo_json_state_reader_listener_t::on_update callback
 *                     should also be non-\c NULL.
 * \param[in] userdata Opaque user data that will be provided to the callbacks
 *                     (an integer or \c void pointer).
 *
 * \return The reader or \c NULL on failure.
 *
 * \see ::qeo_json_state_reader_close
 */
qeo_json_state_reader_t *qeo_json_factory_create_state_reader(const qeo_factory_t *factory,
                                                              const char * json_type,
                                                              const qeo_json_state_reader_listener_t *listener,
                                                              uintptr_t userdata);
/**
 * Enable a disabled Qeo state reader.
 *
 * \param[in] reader The reader to enable.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_json_state_reader_enable(qeo_json_state_reader_t *reader);

/**
 * Closes the reader, relinquishing any underlying resources.
 *
 * \warning This function can take some time to return if a callback is
 *          currently being called.  Resource clean up can only happen after
 *          the callback has finished.
 *
 * \param[in] reader The reader to close.
 */
void qeo_json_state_reader_close(qeo_json_state_reader_t *reader);

/**
 * Called for each instance during iteration.
 *
 * \see ::qeo_json_state_reader_foreach
 *
 * \param[in] json_data  Instance data.
 * \param[in] userdata Opaque user data as provided to ::qeo_state_reader_foreach.
 *
 * \return On of ::qeo_iterate_action_t depending on what action to perform
 *         after returning from the callback.
 */
typedef qeo_iterate_action_t (*qeo_json_iterate_callback)(const char *json_data, uintptr_t userdata);

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
 * \retval ::QEO_ENODATA when end of iteration reached
 */
qeo_retcode_t qeo_json_state_reader_foreach(const qeo_json_state_reader_t *reader,
                                            qeo_json_iterate_callback cb,
                                            uintptr_t userdata);
/**
 * \copydoc ::qeo_event_reader_policy_update
 */
qeo_retcode_t qeo_json_state_reader_policy_update(const qeo_json_state_reader_t *reader, const char* json_policy);

/// \}

/* ===[ State change reader ]================================================ */

/// \name Qeo State Change Reader
/// \{

/**
 * Opaque structure representing a Qeo state change reader.
 */
typedef struct qeo_state_change_reader_s qeo_json_state_change_reader_t;

/**
 * Called when a new data sample has arrived.
 *
 * \param[in] reader The reader on which the new data sample has arrived.
 * \param[in] json_data Incoming sample data.
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_json_state_on_data_callback)(const qeo_json_state_change_reader_t *reader,
                                                const char *json_data,
                                                uintptr_t userdata);

/**
 * Called when there are no more samples in the current burst.
 *
 * There are situations where ::qeo_json_state_on_data_callback will be called several
 * times in a short period of time because a burst of samples arrived.  If
 * provided then ::qeo_json_state_on_no_more_data_callback will be called at the end of
 * such a burst.
 *
 * \param[in] reader The reader for which the burst ended.
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_json_state_on_no_more_data_callback)(const qeo_json_state_change_reader_t *reader,
                                                        uintptr_t userdata);

/**
 * Called when a data instance has been removed. Note that this call will
 * only be made for keyed data.
 *
 * \param[in] reader The reader from which a data instance has been removed.
 * \param[in] json_data Removed instance data (note that only the fields marked
 *                 as key will have valid data).
 * \param[in] userdata Opaque user data as provided during reader creation.
 */
typedef void (*qeo_json_state_on_remove_callback)(const qeo_json_state_change_reader_t *reader,
                                                  const char *json_data,
                                                  uintptr_t userdata);

/**
 * Callback called whenever the policy for a reader is being updated.  This
 * callback enables an application to restrict the list of identities that are
 * allowed for reading.  During a policy update it will be called for each
 * identity that is relevant for this reader.  At the end of the update it will
 * be called with a \c NULL identity to signal the end of the update.
 *
 * \param[in] reader The reader for which the policy is being updated.
 * \param[in] json_policy The currently enforced policy.
 * \param[in] userdata Opaque user data as provided during reader creation.
 *
 */
typedef void (*qeo_json_state_change_reader_on_policy_update_callback)(const qeo_json_state_change_reader_t *reader,
                                                                                    const char *json_policy,
                                                                                    uintptr_t userdata);

/**
 * State change reader listener callback structure.  Upon reception of new data
 * the appropriate callbacks (if provided) will be called.
 */
typedef struct {
    qeo_json_state_on_data_callback on_data; /**< see ::qeo_json_state_on_data_callback */
    qeo_json_state_on_no_more_data_callback on_no_more_data; /**< see ::qeo_json_state_on_no_more_data_callback */
    qeo_json_state_on_remove_callback on_remove; /**< see ::qeo_json_state_on_remove_callback */
    qeo_json_state_change_reader_on_policy_update_callback on_policy_update; /**< see ::qeo_json_state_change_reader_on_policy_update_callback */
} qeo_json_state_change_reader_listener_t;

/**
 * Create a new disabled Qeo state change reader for the given type.
 * 
 * \param[in] factory  The factory to be used for creating the reader.
 * \param[in] json_type The type for which to create the reader (non-\c NULL).
 * \param[in] listener The listener to use for data reception notifications
 *                     (non-\c NULL).  Either the qeo_json_state_change_reader_listener_t::on_data
 *                     callback or the qeo_json_state_change_reader_listener_t::on_remove
 *                     callback should be non-\c NULL.
 * \param[in] userdata Opaque user data that will be provided to the callbacks
 *                     (an integer or \c void pointer).
 *
 * \return The reader or \c NULL on failure.
 *
 * \see ::qeo_json_state_change_reader_close
 */
qeo_json_state_change_reader_t *qeo_json_factory_create_state_change_reader(const qeo_factory_t *factory,
                                                                            const char * json_type,
                                                                            const qeo_json_state_change_reader_listener_t *listener,
                                                                            uintptr_t userdata);
/**
 * Enable a disabled Qeo state change reader.
 *
 * \param[in] reader The reader to enable.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_json_state_change_reader_enable(qeo_json_state_change_reader_t *reader);

/**
 * Closes the reader, relinquishing any underlying resources.
 *
 * \warning This function can take some time to return if a callback is
 *          currently being called.  Resource clean up can only happen after
 *          the callback has finished.
 *
 * \param[in] reader The reader to close.
 */
void qeo_json_state_change_reader_close(qeo_json_state_change_reader_t *reader);

/**
 * Call this function to trigger a re-evaluation of the reader's policy.  The
 * register \c on_update callback (if any) will be called again.
 *
 * \param[in] reader The reader.
 * \param[in] json_policy The policy to be enforced.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_json_state_change_reader_policy_update(const qeo_json_state_change_reader_t *reader, const char* json_policy);

/// \}

/* ===[ State writer ]======================================================= */

/// \name Qeo State Writer
/// \{

/**
 * Opaque structure representing a Qeo state writer.
 */
typedef struct qeo_state_writer_s qeo_json_state_writer_t;

/**
 * Callback called whenever the policy for a writer is being updated.  This
 * callback enables an application to restrict the list of identities that are
 * allowed for writing.  During a policy update it will be called for each
 * identity that is relevant for this writer.  At the end of the update it will
 * be called with a \c NULL identity to signal the end of the update.
 *
 * \param[in] writer The writer for which the policy is being updated.
 * \param[in] The currently enforced policy.
 * \param[in] userdata Opaque user data as provided during writer creation.
 *
 */
typedef void (*qeo_json_state_writer_on_policy_update_callback)(const qeo_json_state_writer_t *writer,
                                                                             const char *json_policy,
                                                                             uintptr_t userdata);

/**
 * State writer listener callback structure.  Upon reception of policy updates
 * the appropriate callbacks (if provided) will be called.
 */
typedef struct {
    qeo_json_state_writer_on_policy_update_callback on_policy_update; /**< see ::qeo_json_state_writer_on_policy_update_callback */
} qeo_json_state_writer_listener_t;


/**
 * Create a new disabled Qeo state writer for the given type.
 *
 * \param[in] factory The factory to be used for creating the writer.
 * \param[in] json_type The type for which to create the writer (non-\c NULL).
 * \param[in] listener The listener to use for policy reception notifications
 *                     (non-\c NULL).  Either the qeo_json_state_writer_listener_t::on_policy_update
 *                     callback should be non-\c NULL.
 * \param[in] userdata Opaque user data that will be provided to the callbacks
 *                     (an integer or \c void pointer).
 *
 * \return The writer or \c NULL on failure.
 *
 * \see ::qeo_json_state_writer_close
 */
qeo_json_state_writer_t *qeo_json_factory_create_state_writer(const qeo_factory_t *factory,
                                                              const char *json_type,
                                                              const qeo_json_state_writer_listener_t *listener,
                                                              uintptr_t userdata);
/**
 * Enable a disabled Qeo state writer.
 *
 * \param[in] reader The reader to enable.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_json_state_writer_enable(qeo_json_state_writer_t *writer);

/**
 * Closes the writer, relinquishing any underlying resources.
 *
 * \param[in] writer The writer to close.
 */
void qeo_json_state_writer_close(qeo_json_state_writer_t *writer);

/**
 * Send a new data sample to all subscribed readers.
 *
 * \param[in] writer The writer.
 * \param[in] json_data The sample data to write.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL in case of invalid arguments
 * \retval ::QEO_EFAIL in case writing failed
 */
qeo_retcode_t qeo_json_state_writer_write(const qeo_json_state_writer_t *writer,
                                          const char *json_data);

/**
 * Signal the removal of an instance to all subscribed readers.
 *
 * \param[in] writer The writer.
 * \param[in] json_data The instance to be removed.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL in case of invalid arguments
 * \retval ::QEO_EFAIL in case writing failed
 */
qeo_retcode_t qeo_json_state_writer_remove(const qeo_json_state_writer_t *writer,
                                           const char *json_data);


/**
 * Call this function to trigger a re-evaluation of the writer's policy.  The
 * register \c on_update callback (if any) will be called again.
 *
 * \param[in] writer The writer.
 * \param[in] json_policy The policy to be enforced.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_json_state_writer_policy_update(const qeo_json_state_writer_t *writer, const char* json_policy);


/// \}
/* ===[ DeviceInfo ]======================================================= */

/// \name Qeo Device Info
/// \{

/**
 * Call this function to fetch the Qeo Device ID.
 * This function will allocate the required memory for json_device_id 
 * and the user of this function needs to free it.
 *
 * \param[out] json_device_id JSON representation of the Qeo Device ID.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeo_json_get_device_id(char** json_device_id);
/// \}

#ifdef __cplusplus
}
#endif

#endif /* QEO_API_H_ */
