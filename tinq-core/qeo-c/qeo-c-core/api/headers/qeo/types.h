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
 * Qeo types
 */

#ifndef QEO_TYPES_H_
#define QEO_TYPES_H_

#include <stdint.h>

#ifndef XTYPES_USED
/**
 * Define to indicate support for dynamic types
 */
#define XTYPES_USED
#endif
#include <dds/dds_tsm.h>
#include <dds/dds_seq.h>

/**
 * Define to use if you want to make use of the Open domain.
 *
 */
#define QEO_IDENTITY_OPEN       (qeo_identity_t *)0x1

/**
 * Define to use in normal cases
 *
 */
#define QEO_IDENTITY_DEFAULT    (qeo_identity_t *)NULL

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Opaque structure representing a Qeo factory.
 */
typedef struct qeo_factory_s qeo_factory_t;

/**
 * Opaque structure representing a Qeo identity.
 */
typedef struct qeo_identity_s qeo_identity_t;

/**
 * Possible actions when iterating.
 */
typedef enum {
    QEO_ITERATE_CONTINUE,      /**< continue iteration */
    QEO_ITERATE_ABORT,         /**< abort iteration (iteration will return ::QEO_OK) */
} qeo_iterate_action_t;

/**
 * Qeo boolean type.
 */
typedef unsigned char qeo_boolean_t;

/**
 * Qeo enumeration constant's value type.
 */
typedef int32_t qeo_enum_value_t;

/* ---[ policies API ]------------------------------------------------------ */

/// \name Policy
/// \{

/**
 * Opaque structure representing an identity used in policy files.
 *
 */
typedef struct qeo_policy_identity_s qeo_policy_identity_t;

/**
 * Get the user's ID from the identity.
 *
 * \param[in] identity the identity from which to get the user's ID
 *
 * \return the user's ID or -1 when the input arguments are invalid
 */
int64_t qeo_policy_identity_get_uid(const qeo_policy_identity_t *identity);

/**
 * Enumeration of return values to be used as return values for the policy
 * update callback.
 *
 */
typedef enum {
    QEO_POLICY_ALLOW,                  /**< allow the identity */
    QEO_POLICY_DENY                    /**< deny the identity */
} qeo_policy_perm_t;

/// \}

#ifdef __cplusplus
}
#endif

#endif /* QEO_TYPES_H_ */
