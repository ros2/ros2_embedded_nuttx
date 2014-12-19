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
 * Qeo factory API
 */

#ifndef FACTORY_H_
#define FACTORY_H_

#include <qeo/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ===[ Qeo factory ]======================================================== */

/// \name Qeo Factory
/// \{

/**
 * Returns a Qeo factory instance for the default realm that can be used for
 * creating Qeo readers and writers.  The factory instance should be properly
 * closed if none of the readers and/or writers that have been created, are
 * needed anymore.  This will free any allocated resources associated with that
 * factory.
 *
 * \return The factory or \c NULL on failure.
 *
 * \see ::qeo_factory_close
 */
qeo_factory_t *qeo_factory_create();

/**
 * Returns a Qeo factory instance that can be used for creating Qeo readers and
 * writers.  The factory instance should be properly closed if none of the
 * readers and/or writers that have been created, are needed anymore.  This will
 * free any allocated resources associated with that factory.
 *
 * \param[in] id  The identity for which you want to create the factory.
 *                Use QEO_IDENTITY_DEFAULT for the default identity.
 *
 * \return The factory or \c NULL on failure.
 *
 * \see ::qeo_factory_close
 */
qeo_factory_t *qeo_factory_create_by_id(const qeo_identity_t *id);

/**
 * Close the factory and release any resources associated with it.
 *
 * \warning Make sure that any readers and/or writers created with this factory
 *          have been closed before calling this function.
 *
 * \param[in] factory  The factory to be closed.
 */
void qeo_factory_close(qeo_factory_t *factory);

/// \}

/* ===[ Miscellaneous ]====================================================== */

/// \name Miscellaneous
/// \{

/**
 * Returns a string representation of the Qeo library version.
 *
 * \return The Qeo library version string.
 */
const char *qeo_version_string(void);

/// \}

#ifdef __cplusplus
}
#endif

#endif /* FACTORY_H_ */
