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
 * Qeo language binding API.
 */

#ifndef QEOCORE_API_H_
#define QEOCORE_API_H_

#include <limits.h>

#include <qeo/error.h>
#include <qeo/types.h>
#include <qeocore/factory.h>
#include <qeocore/dyntype.h>

/* ---[ data API ]---------------------------------------------------------- */

/// \name Data
/// \{

/**
 * Definition to be used as the size of a sequence when getting all elements of
 * the sequence using ::qeocore_data_sequence_get.
 */
#define QEOCORE_SIZE_UNLIMITED ((int)INT_MAX)

/**
 * The status associated with data.
 */
typedef enum {
    QEOCORE_NOTIFY,                    /**< something happened */
    QEOCORE_DATA,                      /**< data was read */
    QEOCORE_NO_MORE_DATA,              /**< no data was read */
    QEOCORE_REMOVE,                    /**< instance removal, only key data */
    QEOCORE_ERROR,                     /**< error occurred while reading */
} qeocore_data_status_t;

/**
 * Opaque structure representing a data structure with members.
 *
 * \see ::qeocore_reader_read
 * \see ::qeocore_reader_take
 * \see ::qeocore_on_data_available_f
 * \see ::qeocore_writer_write
 * \see ::qeocore_writer_remove
 */
typedef struct qeocore_data_s qeocore_data_t;

// TODO make opaque ?
/**
 * Structure that is used to filter what kind of samples are read.
 *
 * \see ::qeocore_reader_read
 * \see ::qeocore_reader_take
 */
typedef struct {
    unsigned int instance_handle;      /**< used for instance iteration:
                                            if zero return the first instance,
                                            if non-zero return the next one */
} qeocore_filter_t;

/**
 * Clean the structure and initialize it again for reuse.
 *
 * \param[in,out] data the data structure to reinitialize
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeocore_data_reset(qeocore_data_t *data);

/**
 * Free the data structure.  This will free any resources contained in
 * the structure and also the structure itself.
 *
 * \param[in] data the data structure to be freed
 */
void qeocore_data_free(qeocore_data_t *data);

/**
 * Set the value of a member in the data structure.
 *
 * The following table provides a mapping between the member's type code and
 * the value's pointer type.
 *
 * qeocore_typecode_t          | pointer type for \a value |
 * ----------------------------|---------------------------|
 * ::QEOCORE_TYPECODE_BOOLEAN  | ::qeo_boolean_t *         |
 * ::QEOCORE_TYPECODE_INT8     | \c unsigned \c char *     |
 * ::QEOCORE_TYPECODE_INT16    | \c int16_t *              |
 * ::QEOCORE_TYPECODE_INT32    | \c int32_t *              |
 * ::QEOCORE_TYPECODE_INT64    | \c int64_t *              |
 * ::QEOCORE_TYPECODE_FLOAT32  | \c float *                |
 * ::QEOCORE_TYPECODE_STRING   | \c char **                |
 * ::QEOCORE_TYPECODE_SEQUENCE | ::qeocore_data_t **       |
 * ::QEOCORE_TYPECODE_STRUCT   | ::qeocore_data_t **       |
 * ::QEOCORE_TYPECODE_ENUM     | ::qeo_enum_value_t *      |
 *
 * \param[in] data the data structure
 * \param[in] id the member identifier (see ::qeocore_type_struct_add)
 * \param[in] value pointer to the value of the member
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EFAIL when setting the value failed
 */
qeo_retcode_t qeocore_data_set_member(qeocore_data_t *data,
                                      qeocore_member_id_t id,
                                      const void *value);

/**
 * Get the value of a member in the data structure.  Note that if the returned
 * value contains dynamic data (e.g. string, sequence, ...) it is up to the
 * caller to free the memory associated with this dynamic data.
 *
 * The following table provides a mapping between the member's type code and
 * the value's pointer type.
 *
 * qeocore_typecode_t          | pointer type for \a value | free data with        |
 * ----------------------------|---------------------------|-----------------------|
 * ::QEOCORE_TYPECODE_BOOLEAN  | ::qeo_boolean_t *         |                       |
 * ::QEOCORE_TYPECODE_INT8     | \c unsigned \c char *     |                       |
 * ::QEOCORE_TYPECODE_INT16    | \c int16_t *              |                       |
 * ::QEOCORE_TYPECODE_INT32    | \c int32_t *              |                       |
 * ::QEOCORE_TYPECODE_INT64    | \c int64_t *              |                       |
 * ::QEOCORE_TYPECODE_FLOAT32  | \c float *                |                       |
 * ::QEOCORE_TYPECODE_STRING   | \c char **                | \c free()             |
 * ::QEOCORE_TYPECODE_SEQUENCE | ::qeocore_data_t **       | ::qeocore_data_free() |
 * ::QEOCORE_TYPECODE_STRUCT   | ::qeocore_data_t **       | ::qeocore_data_free() |
 * ::QEOCORE_TYPECODE_ENUM     | ::qeo_enum_value_t *      |                       |
 *
 * \param[in] data the data structure
 * \param[in] id the member identifier (see ::qeocore_type_struct_add)
 * \param[in] value pointer to the location where to store the value of the
 *                  member
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EFAIL when setting the value failed
 */
qeo_retcode_t qeocore_data_get_member(const qeocore_data_t *data,
                                      qeocore_member_id_t id,
                                      void *value);

/**
 * Allocate a \a sequence->data buffer large enough to contain \a num
 * elements from the sequence represented by \a data.
 *
 * \param[in]     data     The data item for which to create the sequence.
 * \param[in,out] sequence The sequence to be initialized.
 * \param[in]     num      The number of elements to allocate buffer space for.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_ENOMEM when out of resources
 */
qeo_retcode_t qeocore_data_sequence_new(const qeocore_data_t *data,
                                        qeo_sequence_t *sequence,
                                        int num);

/**
 * Free the elements in the \a sequence->data buffer and the buffer itself.
 *
 * \param[in]     data     The data item for which to free the sequence.
 * \param[in,out] sequence The sequence to be freed.
*
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
  */
qeo_retcode_t qeocore_data_sequence_free(const qeocore_data_t *data,
                                         qeo_sequence_t *sequence);

/**
 * Copy \a sequence->_length elements from the given \a sequence to the
 * \a offset in the sequence represented by \a data.
 *
 * \param[in,out] data     The data representation of the sequence.
 * \param[in]     sequence The sequence from which to copy the elements.
 * \param[in]     offset   The offset at which to copy the sequence elements.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_ENOMEM when out of resources
 */
qeo_retcode_t qeocore_data_sequence_set(qeocore_data_t *data,
                                        const qeo_sequence_t *sequence,
                                        int offset);

/**
 * Copy \a num elements starting from \a offset from the sequence represented
 * by \a data to the given \a sequence.  To get all elements use
 * ::QEOCORE_SIZE_UNLIMITED.
 *
 * \param[in,out] data     The data representation of the sequence.
 * \param[in]     sequence The sequence into which to copy the elements.
 * \param[in]     offset   The offset from which to copy the sequence elements.
 * \param[in]     num      The number of elements to copy.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_ENOMEM when out of resources
 */
qeo_retcode_t qeocore_data_sequence_get(const qeocore_data_t *data,
                                        qeo_sequence_t *sequence,
                                        int offset,
                                        int num);

/**
 * Get the status of the last read action in which this data structure was
 * used.
 *
 * \param[in] data the data structure
 *
 * \return ::QEOCORE_ERROR when the input arguments are invalid, otherwise one of
 *         ::qeocore_data_status_t depending on the last read
 */
qeocore_data_status_t qeocore_data_get_status(const qeocore_data_t *data);

/**
 * Get the instance handle of the instance that was read
 *
 * \param[in] data the data structure
 *
 * \return 0 when the input arguments are invalid, otherwise the instance
 *         handle
 */
unsigned int qeocore_data_get_instance_handle(const qeocore_data_t *data);

/**
 * For TSM based entities this will return a pointer to the actual C data
 * structure.
 *
 * \param[in] data the data structure
 *
 * \return Pointer to the actual data or \c NULL on error.
 */
const void *qeocore_data_get_data(const qeocore_data_t *data);

/// \}

/* ---[ entity creation flags ]--------------------------------------------- */

/// \name Entity creation flags
/// \{

/** Nothing special. */
#define QEOCORE_EFLAG_NONE     0

/** Eventing entity. */
#define QEOCORE_EFLAG_EVENT    (1 << 0)

/** State entity. */
#define QEOCORE_EFLAG_STATE    (1 << 1)

/** Enable entity at creation time. */
#define QEOCORE_EFLAG_ENABLE   (1 << 2)

/** Use this combination of flags for an event reader or writer. */
#define QEOCORE_EFLAG_EVENT_DATA   QEOCORE_EFLAG_EVENT

/** Use this combination of flags for an state change reader or writer. */
#define QEOCORE_EFLAG_STATE_DATA   (QEOCORE_EFLAG_EVENT | QEOCORE_EFLAG_STATE)

/** Use this combination of flags for an state reader or writer. */
#define QEOCORE_EFLAG_STATE_UPDATE QEOCORE_EFLAG_STATE

/// \}

/* ---[ reader API ]-------------------------------------------------------- */

/// \name Reader
/// \{

/**
 * Opaque structure representing a Qeo reader.
 */
typedef struct qeocore_reader_s qeocore_reader_t;

/**
 * Callback used when new sample data is available.
 *
 * \param[in] reader the reader on which the data became available
 * \param[in] data the actual data
 * \param[in] userdata opaque user data as provided during reader creation in
 *                     ::qeocore_reader_listener_t::userdata
 */
typedef void (*qeocore_on_data_available_f)(const qeocore_reader_t *reader,
                                            const qeocore_data_t *data,
                                            uintptr_t userdata);

/**
 * Callback called whenever the fine-grained policy for a reader is being updated.  This
 * callback enables an application to restrict the list of identities that are
 * allowed for reading.  During a policy update it will be called for each
 * identity that is relevant for this reader.  At the end of the update it will
 * be called with a \c NULL identity to signal the end of the update.
 *
 * \note This callback is only useful for fine-grained policies.  If a coarse-grained
 *       policy is being used then it will never be called.
 *
 * \param[in] reader the reader for which the policy is being updated
 * \param[in] identity the identity that is about to be allowed
 * \param[in] userdata opaque user data as provided during reader creation in
 *                     ::qeocore_reader_listener_t::userdata
 *
 * \retval ::QEO_POLICY_ALLOW allow this identity and add it to the reader's
 *                            policy
 * \retval ::QEO_POLICY_DENY disallow this identity and do not add it to the
 *                           reader's policy
 */
typedef qeo_policy_perm_t (*qeocore_on_reader_policy_update_f)(const qeocore_reader_t *reader,
                                                               const qeo_policy_identity_t *identity,
                                                               uintptr_t userdata);

/**
 * Reader listener structure containing one or more callbacks and optional
 * user data.
 */
typedef struct {
    qeocore_on_data_available_f on_data; /**< see ::qeocore_on_data_available_f */
    qeocore_on_reader_policy_update_f on_policy_update; /**< see ::qeocore_on_reader_policy_update_f */
    uintptr_t userdata;                  /**< opaque user data that will be
                                          *   passed to the callbacks (an
                                          *   integer or \c void pointer, may
                                          *   be 0) */
} qeocore_reader_listener_t;

/**
 * Open a new Qeo reader.  By default the reader will be created in a disabled
 * state.  This means that no data will arrive on it or can be read from it.  To
 * enable the reader either pass ::QEOCORE_EFLAG_ENABLE at creation time or
 * call ::qeocore_reader_enable afterwards.  When enabling it the policy update
 * callback (if any) will be called if needed.
 *
 * \param[in] factory The factory to be used for creating the reader.
 * \param[in] type the dynamic type for which to create a reader
 * \param[in] topic_name the name of the topic, may be \c NULL (default is to
 *                       use the name of the type)
 * \param[in] flags determines what kind of reader to create (OR-ed values of
 *                  \c QEOCORE_EFLAG_*)
 * \param[in] listener structure with callbacks to be used, may be \c NULL in
 *                     which case no listeners get installed
 * \param[out] rc if non-\c NULL a return code will be set to be able to
 *                distinguish between the different error cases
 *
 * \return the newly created reader on success, or \c NULL on failure
 */
qeocore_reader_t *qeocore_reader_open(const qeo_factory_t *factory,
                                      qeocore_type_t *type,
                                      const char *topic_name,
                                      int flags,
                                      const qeocore_reader_listener_t *listener,
                                      qeo_retcode_t *rc);

/**
 * Enable the reader.  The policy update callback (if any) will be called if
 * needed.
 *
 * \param[in] reader the reader
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the reader was already enabled
 */
qeo_retcode_t qeocore_reader_enable(qeocore_reader_t *reader);

/**
 * Close a Qeo reader.  This will release any resources associated with it.
 *
 * \warning This function can take some time to return if a callback is
 *          currently being called.  Resource clean up can only happen after
 *          the callback has finished.
 *
 * \param[in] reader the reader
 */
void qeocore_reader_close(qeocore_reader_t *reader);

/**
 * Get the cookie associated with the reader.  This is the same cookie as the
 * one that was provided when opening the reader.
 *
 * \param[in] reader the reader
 *
 * \return the opaque user data, or 0 on invalid arguments
 */
uintptr_t qeocore_reader_get_userdata(const qeocore_reader_t *reader);

/**
 * Create a new data structure for use with this reader.
 *
 * \see ::qeocore_reader_read, ::qeocore_reader_take
 *
 * \param[in] reader the reader
 *
 * \return the newly created data structure, or \c NULL on failure
 */
qeocore_data_t *qeocore_reader_data_new(const qeocore_reader_t *reader);

/**
 * Read a sample instance (and leave it in the DDS cache).  When the instance
 * handle in the filter is zero, the first available sample is returned.  If
 * it is non-zero the next sample for the next instance is returned.
 *
 * \param[in] reader the reader
 * \param[in] filter the filter to use when reading, may be \c NULL
 * \param[out] data the data structure to fill with the results of the read
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the reader was not yet enabled
 * \retval ::QEO_ENODATA when no data was read
 * \retval ::QEO_EFAIL when the read failed
 */
qeo_retcode_t qeocore_reader_read(const qeocore_reader_t *reader,
                                  const qeocore_filter_t *filter,
                                  qeocore_data_t *data);

/**
 * Take a sample instance (and remove it from the DDS cache).  When the
 * instance handle in the filter is zero, the first available sample is
 * returned.  If it is non-zero the next sample for the next instance is
 * returned.
 *
 * \param[in] reader the reader
 * \param[in] filter the filter to use when reading, may be \c NULL
 * \param[out] data the data structure to fill with the results of the take
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the reader was not yet enabled
 * \retval ::QEO_ENODATA when no data was taken
 * \retval ::QEO_EFAIL when the take failed
 */
qeo_retcode_t qeocore_reader_take(const qeocore_reader_t *reader,
                                  const qeocore_filter_t *filter,
                                  qeocore_data_t *data);

/**
 * Call this function to trigger a re-evaluation of the reader's policy.  The
 * register \c on_update callback (if any) will be called again.
 *
 * \see ::qeocore_on_reader_policy_update_f
 *
 * \param[in] reader the reader
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeocore_reader_policy_update(const qeocore_reader_t *reader);

/// \}

/* ---[ writer API ]-------------------------------------------------------- */

/// \name Writer
/// \{

/**
 * Opaque structure representing a Qeo writer.
 */
typedef struct qeocore_writer_s qeocore_writer_t;

/**
 * Callback called whenever the fine-grained policy for a writer is being updated.  This
 * callback enables an application to restrict the list of identities that are
 * allowed for writing.  During a policy update it will be called for each
 * identity that is relevant for this writer.  At the end of the update it will
 * be called with a \c NULL identity to signal the end of the update.
 *
 * \note This callback is only useful for fine-grained policies.  If a coarse-grained
 *       policy is being used then it will never be called.
 *
 * \param[in] writer the writer for which the policy is being updated
 * \param[in] identity the identity that is about to be allowed
 * \param[in] userdata opaque user data as provided during writer creation in
 *                     ::qeocore_writer_listener_t::userdata
 *
 * \retval ::QEO_POLICY_ALLOW allow this identity and add it to the writer's
 *                            policy
 * \retval ::QEO_POLICY_DENY disallow this identity and do not add it to the
 *                           writer's policy
 */
typedef qeo_policy_perm_t (*qeocore_on_writer_policy_update_f)(const qeocore_writer_t *writer,
                                                               const qeo_policy_identity_t *identity,
                                                               uintptr_t userdata);

/**
 * Writer listener structure containing one or more callbacks and optional
 * user data.
 */
typedef struct {
    qeocore_on_writer_policy_update_f on_policy_update; /**< see ::qeocore_on_writer_policy_update_f */
    uintptr_t userdata;                  /**< opaque user data that will be
                                          *   passed to the callbacks (an
                                          *   integer or \c void pointer, may
                                          *   be 0) */
} qeocore_writer_listener_t;

/**
 * Open a new Qeo writer.  By default the writer will be created in a disabled
 * state.  This means that it will not be possible to send data.  To enable the
 * writer either pass ::QEOCORE_EFLAG_ENABLE at creation time or call
 * call ::qeocore_writer_enable afterwards.  When enabling it the policy update
 * callback (if any) will be called if needed.
 *
 * \param[in] factory The factory to be used for creating the writer.
 * \param[in] type the dynamic type for which to create a reader
 * \param[in] topic_name the name of the topic, may be \c NULL (default is to
 *                       use the name of the type)
 * \param[in] flags determines what kind of writer to create (OR-ed values of
 *                  \c QEOCORE_EFLAG_*)
 * \param[in] listener structure with callbacks to be used, may be \c NULL in
 *                     which case no listeners get installed
 * \param[out] rc if non-\c NULL a return code will be set to be able to
 *                distinguish between the different error cases
 *
 * \return the newly created writer on success, or \c NULL on failure
 */
qeocore_writer_t *qeocore_writer_open(const qeo_factory_t *factory,
                                      qeocore_type_t *type,
                                      const char *topic_name,
                                      int flags,
                                      const qeocore_writer_listener_t *listener,
                                      qeo_retcode_t *rc);

/**
 * Enable the writer.  The policy update callback (if any) will be called if
 * needed.
 *
 * \param[in] writer the writer
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the writer was already enabled
 */
qeo_retcode_t qeocore_writer_enable(qeocore_writer_t *writer);

/**
 * Close a Qeo writer.  This will release any resources associated with it.
 *
 * \param[in] writer the writer
 */
void qeocore_writer_close(qeocore_writer_t *writer);

/**
 * Get the cookie associated with the writer.  This is the same cookie as the
 * one that was provided when opening the writer.
 *
 * \param[in] writer the writer
 *
 * \return the opaque user data, or 0 on invalid arguments
 */
uintptr_t qeocore_writer_get_userdata(const qeocore_writer_t *writer);

/**
 * Create a new data structure for use with this writer.
 *
 * \see ::qeocore_writer_write, ::qeocore_writer_remove
 *
 * \param[in] writer the writer
 *
 * \return the newly created data structure, or \c NULL on failure
 */
qeocore_data_t *qeocore_writer_data_new(const qeocore_writer_t *writer);

/**
 * Writer a new instance sample.
 *
 * \param[in] writer the writer
 * \param[in] data the data structure for the sample to write
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the writer was not yet enabled
 * \retval ::QEO_EFAIL when the write failed
 */
qeo_retcode_t qeocore_writer_write(const qeocore_writer_t *writer,
                                   const void *data);

/**
 * Remove an instance.
 *
 * \param[in] writer the writer
 * \param[in] data the data structure for the instance to remove
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the writer was not yet enabled
 * \retval ::QEO_EFAIL when the write failed
 */
qeo_retcode_t qeocore_writer_remove(const qeocore_writer_t *writer,
                                    const void *data);

/**
 * Call this function to trigger a re-evaluation of the writer's policy.  The
 * register \c on_update callback (if any) will be called again.
 *
 * \see ::qeocore_on_writer_policy_update_f
 *
 * \param[in] writer the writer
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeocore_writer_policy_update(const qeocore_writer_t *writer);

/// \}

#endif /* QEOCORE_API_H_ */
