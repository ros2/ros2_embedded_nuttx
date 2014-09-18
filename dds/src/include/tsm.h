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

/* tsm.h -- Converts the DDS_TypeSupport_meta type description to the internal
	    X-Types data representation. */

#ifndef __tsm_h_
#define	__tsm_h_

#include "type_data.h"

/* Returned information flags: */
#define	IF_DYNAMIC	1	/* Structure contains dynamic members. */
#define	IF_TOPLEVEL	2	/* Set this when defining a new type. */

void tsm_set_generate_callback (uint32_t (*f) (const char *));

/* Set the callback function that is used to create the member Id */

Type *tsm_create_struct_union (TypeLib                    *lp,
			       const DDS_TypeSupport_meta **tsm,
			       unsigned                   *info_flags);

/* Create a Structure or a Union type based on the meta type info.
   The info_flags parameter should be set initially to IF_TOPLEVEL.
   On return it will be updated to indicate the dynamic aspects of the
   structure. */

void tsm_typeref_set_type (DDS_TypeSupport_meta       *tsm,
			   unsigned                   n,
			   const Type                 *type);

/* Substitute the TYPEREF field on the given offset into a TYPE field that
   refer to the real type. */ 

Type *tsm_create_typedef (TypeLib                    *lp,
			  const DDS_TypeSupport_meta *tsm,
			  unsigned                   *info_flags);

/* Create a type that is a Typedef to an existing Structure or Union type. */

#endif /* !__tsm_h_ */

