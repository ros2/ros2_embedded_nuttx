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
 * Qeo dynamic type API.
 */

#ifndef QEOCORE_DYNTYPE_H_
#define QEOCORE_DYNTYPE_H_

#include <stddef.h> // for offsetof

#ifndef XTYPES_USED
/**
 * Define to indicate support for dynamic types
 */
#define XTYPES_USED
#endif
#include <dds/dds_tsm.h>

#include <qeocore/factory.h>
#include <qeo/error.h>

/**
 * Enumeration of the different types that are supported.
 */
typedef enum {
    QEOCORE_TYPECODE_BOOLEAN,          /**< boolean (primitive) */
    QEOCORE_TYPECODE_INT8,             /**< signed 8-bit integer number (primitive) */
    QEOCORE_TYPECODE_INT16,            /**< signed 16-bit integer number (primitive) */
    QEOCORE_TYPECODE_INT32,            /**< signed 32-bit integer number (primitive) */
    QEOCORE_TYPECODE_INT64,            /**< signed 64-bit integer number (primitive) */
    QEOCORE_TYPECODE_FLOAT32,          /**< 32-bit floating point number (primitive) */
    /** All primitives types should be before this line **/
    QEOCORE_TYPECODE_STRING,           /**< 0-terminated string */
    QEOCORE_TYPECODE_STRUCT,           /**< structure with members */
    QEOCORE_TYPECODE_SEQUENCE,         /**< sequence of elements of a type */
    QEOCORE_TYPECODE_ENUM,             /**< enumeration */
} qeocore_typecode_t;

/**
 * Qeo sequence (unbounded array) type.  Use the macros and functions defined
 * in \c dds_seq.h for handling these sequences.
 */
typedef DDS_VoidSeq qeo_sequence_t;

/**
 * The type for a structure member identifier.
 */
typedef unsigned qeocore_member_id_t;

/**
 * The value to be used when you want the default identifier value for a
 * certain member when adding it to a structure.
 *
 * \see ::qeocore_type_struct_add
 */
#define QEOCORE_MEMBER_ID_DEFAULT  ((qeocore_member_id_t)0)

/**
 * Type definition flag when no special behaviour is expected.
 *
 * \see the flags field in ::qeocore_type_struct_add
 */
#define QEOCORE_FLAG_NONE          (0)

/**
 * Type definition flag for members that are key.
 *
 * \see the flags field in ::qeocore_type_struct_add
 */
#define QEOCORE_FLAG_KEY           (1 << 0)

/**
 * Opaque structure representing a Qeo type.
 */
typedef struct qeocore_type_s qeocore_type_t;

/**
 * Qeo enumerator constant.
 */
typedef struct {
    char *name;                /**< name of the enumerator value */
} qeocore_enum_constant_t;

/**
 * Qeo enumerator constant sequence.
 */
DDS_SEQUENCE(qeocore_enum_constant_t, qeocore_enum_constants_t);

/**
 * Converts an enumeration constant's numeric value to its string
 * representation.  The returned string should not be freed.
 *
 * \pre Either \a enum_tsm or \a enum_type should be non-\c NULL, but not both.
 *
 * \param[in] enum_tsm  The enumeration type definition to be used for conversion.
 * \param[in] enum_type The dynamic enumeration type to be used for conversion.
 * \param[in] value     The enumeration constant value to be converted.
 * \param[out] name     A preallocated buffer in which the name will be copied.
 * \param[in] len       The size of the buffer.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid or the value is
 *                      not valid for the enumeration
 * \retval ::QEO_ENOMEM when the buffer is too small to contain the name
 */
qeo_retcode_t qeocore_enum_value_to_string(const DDS_TypeSupport_meta *enum_tsm,
                                           const qeocore_type_t *enum_type,
                                           qeo_enum_value_t value,
                                           char *name,
                                           size_t sz);

/**
 * Converts an enumeration constant's string representation to its numeric
 * value.
 *
 * \pre Either \a enum_tsm or \a enum_type should be non-\c NULL, but not both.
 *
 * \param[in] enum_tsm  The enumeration type definition to be used for conversion.
 * \param[in] enum_type The dynamic enumeration type to be used for conversion.
 * \param[in] name      The enumeration constant's string representation to be
 *                      converted.
 * \param[out] value    A pointer to a variable in which to store the value.
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid or the name is not
 *                      valid for the enumeration
 */
qeo_retcode_t qeocore_enum_string_to_value(const DDS_TypeSupport_meta *enum_tsm,
                                           const qeocore_type_t *enum_type,
                                           const char *name,
                                           qeo_enum_value_t *value);

/**
 * Create a new type from a type definition and registers it in DDS.
 *
 * \param[in] factory The factory to be used for registering the type.
 * \param[in] tsm     The type definition (non-\c NULL)
 * \param[in] name    The name to be used for registering the type
 *                    (non-\c NULL)
 *
 * \return the newly created type, or \c NULL on invalid arguments or error
 */
qeocore_type_t *qeocore_type_register_tsm(const qeo_factory_t *factory,
                                          const DDS_TypeSupport_meta *tsm,
                                          const char *name);

/**
 * Registers a type in DDS.  This should be done before actually using the
 * type in creation of readers and/or writers.
 *
 * \note A type can not be registered twice.
 *
 * \param[in] factory The factory to be used for registering the type.
 * \param[in] type    The type (non-\c NULL, and of type ::QEOCORE_TYPECODE_STRUCT)
 * \param[in] name    The name to be used for registering the type
 *                    (non-\c NULL)
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_ENOMEM when out of resources
 * \retval ::QEO_EBADSTATE when the type is already registered
 * \retval ::QEO_EFAIL when the registration failed
 */
qeo_retcode_t qeocore_type_register(const qeo_factory_t *factory,
                                    qeocore_type_t *type,
                                    const char *name);

/**
 * Retrieves the id of a member of a registered type.
 *
 * \note This function does not work for TSM-based types.
 *
 * \param[in] type The type from which to retrieve a member id (non-\c NULL).
 * \param[in] name The name of the member (non-\c NULL).
 * \param[out] id A pointer to an id of a member which will be filled in upon
 *                returning (non-\c NULL).
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeocore_type_get_member_id(const qeocore_type_t *type,
                                         const char *name,
                                         qeocore_member_id_t *id);

/**
 * Unregisters the type and release the resources associated with it.
 *
 * \note Reference counting is used on registered types.  Each reader and
 *       writer that reference the type will trigger an increment.  In order
 *       to completely release all resources, all readers and writers using
 *       the type should have been closed.
 *
 * \param[in] type the type
 */
void qeocore_type_free(qeocore_type_t *type);

/**
 * Create a new string type.
 *
 * \param[in] sz the size of the string or zero if unbounded
 *
 * \return the newly created string type or \c NULL on failure
 *
 * \par Example usage
 * See ::qeocore_type_struct_add.
 */
qeocore_type_t *qeocore_type_string_new(size_t sz);

/**
 * Create a new primitive type.
 *
 * \param[in] tc the type code of primitive type
 *
 * \return the newly created primitive type or \c NULL on failure
 *
 * \par Example usage
 * See ::qeocore_type_struct_add.
 */
qeocore_type_t *qeocore_type_primitive_new(qeocore_typecode_t tc);

/**
 * Create a new unbounded sequence type.
 *
 * \param[in] elem_type the type of the elements in the sequence
 *
 * \return the newly created sequence type or \c NULL on failure
 *
 * \par Example usage
 * The following sample will create a byte (\c int8_t) sequence type .
 * \code
 * qeocore_type_t *sequence_type = NULL;
 * qeocore_type_t *element_type = NULL;
 *
 * element_type = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8);
 * sequence_type = qeocore_type_sequence_new(element_type);
 * qeocore_type_free(element_type); // clean up the element because no longer needed
 * \endcode
 */
qeocore_type_t *qeocore_type_sequence_new(qeocore_type_t *elem_type);

/**
 * Create a new enumeration type.
 *
 * \warning The name of the enumeration should be unique.
 *
 * \param[in] name the name of the enumeration type
 * \param[in] values the sequence with the enumeration constants
 *
 * \return the newly created enumeration type or \c NULL on failure
 *
 * \par Example usage
 * The following sample will create an enumeration type with three constants.
 * \code
 * qeocore_type_t *enum_type = NULL;
 * qeocore_enum_constants_t vals = DDS_SEQ_INITIALIZER(qeocore_enum_constant_t);
 *
 * dds_seq_require(&vals, 3); // reserve space for three enumeration constants
 * DDS_SEQ_ITEM(vals, 0).name = "ENUM_CONST_1";
 * DDS_SEQ_ITEM(vals, 1).name = "ENUM_CONST_2";
 * DDS_SEQ_ITEM(vals, 2).name = "ENUM_CONST_3";
 * enum_type = qeocore_type_enum_new("my_enumeration", &vals); // create the type
 * dds_seq_cleanup(&vals); // clean up the sequence because no longer needed
 * \endcode
 */
qeocore_type_t *qeocore_type_enum_new(const char *name,
                                      const qeocore_enum_constants_t *values);

/**
 * Create a new structure type.
 *
 * \warning The name of the structure should be unique.
 *
 * \param[in] name the name of the structure
 *
 * \return the newly created structure type or \c NULL on failure
 *
 * \par Example usage
 * See ::qeocore_type_struct_add.
 */
qeocore_type_t *qeocore_type_struct_new(const char *name);

/**
 * Add a member to a structure type.
 *
 * \param[in] container the structure type (non-\c NULL)
 * \param[in] member the member type (non-\c NULL)
 * \param[in] name the name of the member
 * \param[in,out] id a pointer to the id of the member, if it the id being
 *                   pointed to is ::QEOCORE_MEMBER_ID_DEFAULT then it will be
 *                   calculated and returned
 * \param[in] flags flags to modify behaviour
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EFAIL when the registration failed
 *
 * \par Example usage
 * The following sample will create a structure type with two member fields.
 * \code
 * qeocore_type_t *struct_type = NULL;
 * qeocore_type_t *member_type = NULL;
 * qeocore_member_id member_id = QEOCORE_MEMBER_ID_DEFAULT;
 *
 * // create empty structure
 * struct_type = qeocore_type_struct_new("my_struct");
 *
 * // create/add first member
 * member_type = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8);
 * qeocore_type_struct_add(struct_type, member_type, "my_1st_member", &member_id, QEOCORE_FLAG_NONE));
 * // member_id now contains the identifier for the first member
 * qeocore_type_free(member_type); // clean up the member because no longer needed
 *
 * // create/add second member
 * member_type = qeocore_type_string_new(0);
 * qeocore_type_struct_add(struct_type, member_type, "my_2nd_member", &member_id, QEOCORE_FLAG_NONE));
 * // member_id now contains the identifier for the second member
 * qeocore_type_free(member_type); // clean up the member because no longer needed
 * \endcode
 */
qeo_retcode_t qeocore_type_struct_add(qeocore_type_t *container,
                                      qeocore_type_t *member,
                                      const char *name,
                                      qeocore_member_id_t *id,
                                      unsigned int flags);

#endif /* QEOCORE_DYNTYPE_H_ */
