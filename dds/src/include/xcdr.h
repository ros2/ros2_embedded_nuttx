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

/* xcdr.h -- CDR marshaller code for X-Types. */

#ifndef __xcdr_h_
#define	__xcdr_h_

#include <stdio.h>
#include "xtypes.h"

size_t cdr_marshalled_size (size_t           hsize,
			    const void       *data,
			    const Type       *type,
			    int              dynamic,
			    int              key,
			    int              msize,
			    DDS_ReturnCode_t *error);

/* CDR-marshalled data size calculation from a data sample (data) either in
   native format (dynamic=0), or from a dynamic data sample (dynamic=1), with
   the destination data buffer having a given alignment offset (hsize).
   If key is set then key marshalling mode will be used, i.e. only the key
   fields are processed.  If key is set, the msize parameter indicates whether
   strings need to be padded to their maximum length.
   If successful, a non-0 length is returned.  If not, 0 will be returned and
   *error will be set to an error code. */

size_t cdr_marshall (void             *dest,
		     size_t           hsize,
		     const void       *data,
		     const Type       *type,
		     int              dynamic,
		     int              key,
		     int              msize,
		     int              swap,
		     DDS_ReturnCode_t *error);

/* Marshall a data sample (data) either from native data (dynamic=0) or from
   dynamic data (dynamic=1) in CDR format in the given buffer (dest) with the
   given alignment offset (hsize).
   If key is set then key marshalling mode will be used, i.e. only the key
   fields are processed.  If key is set, the msize parameter indicates whether
   strings need to be padded to their maximum length.
   If swap is set, then the endianness of the marshalled data is reversed.
   If successful, a non-0 length is returned.  If not, 0 will be returned and
   a specific error code is returned in *error. */

size_t cdr_unmarshalled_size (const void       *data,
			      size_t           hsize,
			      const Type       *type,
			      int              key,
			      int              msize,
			      int              swap,
			      size_t           size,
			      DDS_ReturnCode_t *error);

/* Get the unmarshalled, i.e. native data size from CDR-encoded source data
   (data) with the given header alignment (hsize).
   Note that this size can be larger than the actual native C-structure since
   dynamic data will be stored in the same buffer, after the structure itself.
   If key is set, then the marshalled data is a concatenated key list and key
   unmarshalling is intended.  In this case, msize indicates that the key
   fields are padded to their maximum sizes.
   If swap is set, then the marshalled data is swapped and needs to have its
   endianness reversed for the correct byte order.
   If size is given it overrides the actual type size, which can be useful for
   storing extra data fields which are not marshalled/unmarshalled.
   If successful, a non-0 length is returned.  If not, 0 will be returned and
   *error will be set to an error code. */

DDS_ReturnCode_t cdr_unmarshall (void       *dest,
				 const void *data,
			         size_t     hsize,
				 const Type *type,
				 int        key,
			         int        msize,
				 int        swap,
				 size_t     size);

/* Get the unmarshalled, i.e. native data from a CDR-encoded source data sample
   (data) with the given alignment (hsize) into the given destination buffer
   (dest, dsize).
   If key is set, then the marshalled data is a concatenated key list and key
   unmarshalling is needed.  In this case, msize indicates that the key
   fields are padded to their maximum sizes.
   If swap is set, then the marshalled data is swapped and needs to have its
   endianness reversed for the correct byte order.
   If size is given it overrides the actual type size, which can be useful for
   storing extra data fields which are not marshalled/unmarshalled.
   If successful, DDS_RETCODE_OK will be returned.  If not, a specific error is
   returned. */

DynData_t *cdr_dynamic_data (const void *data,
			     size_t     hsize,
			     const Type *type,
			     int        key,
			     int        copy,
			     int        swap);

/* Get a dynamic data container from CDR-encoded marshalled data (data) which
   has the given alignment offset (hsize).
   If key is set, then the source and destination dynamic data will have only
   the key fields set.
   If copy is set, no references to original data may be present and everything
   needs to be copied. 
   If swap is set, then the marshalled data is not in the correct endianness.
   If successful, a Dynamic Data item is returned containing the data descrip-
   tion, which can be queried, once converted to a DDS_DynamicData type, via the
   DDS_DynamicData_* primitives. */

size_t cdr_key_size (size_t           dhsize,	/* Dst: data offset.          */
		     const void       *data,	/* Src: data pointer.         */
		     size_t           hsize,	/* Src: data offset.          */
		     const Type       *type,	/* Type info.                 */
		     int              key,	/* Src: key fields only.      */
		     int              msize,	/* Dst: max-sized fields.     */
		     int              swap,	/* Swap reqd between src/dst. */
		     int              iswap,	/* Src: non-cpu endianness.   */
		     DDS_ReturnCode_t *error);	/* Resulting error code.      */

/* Get the size of a set of CDR-encoded concatenated key fields from marshalled
   data (data) which has the given alignment offset (hsize) with dhsize giving
   the alignment offset for the destination key data buffer.
   If key is set, then the original marshalled data contains only the key fields
   but without padding of strings.
   If msize is set, then the resulting key list must have strings padded to the
   maximum size.
   If swap is set, then the marshalled data is not in the correct endianness.
   If iswap is set, then the marshalled data endianness is different from the
   CPU's endianness.
   If successful, a non-0 length is returned.  If not, 0 will be returned and
   *error will be set to an error code. */

DDS_ReturnCode_t cdr_key_fields (void       *dest,	/* Dst: data pointer. */
				 size_t     dhsize,	/* Dst: data offset.  */
				 const void *data,	/* Src: data pointer. */
				 size_t     hsize,	/* Src: data offset.  */
				 const Type *type,	/* Type info.         */
				 int        key,	/* Src: key data only.*/
				 int        msize,	/* Dst: max-size reqd.*/
				 int        swap,	/* Swap src->dst.     */
				 int        iswap);	/* Src: non-cpu end.  */

/* Get CDR-encoded key fields from CDR-encoded data (data) which has the
   given alignment offset (hsize), and store it in the specified buffer (dest)
   with alignment offset (dhsize).
   If key is set, then the original marshalled data contains only the key fields
   but without padding of strings.
   If msize is set, then the resulting key list must have strings padded to the
   maximum size.
   If swap is set, then the marshalled data endianness is different from the
   destination endianness and needs to be converted.
   If iswap is set, then the marshalled data endianness is different from the
   CPU's endianness.
   If successful, DDS_RETCODE_OK will be returned.  If not, a specific error is
   returned. */

int64_t cdr_union_label (const Type *tp, const void *data);

/* Get the union label value from the given data. */

size_t cdr_field_offset (const void       *data,
		         size_t           hsize,
		         unsigned         field,
		         const Type       *type,
			 int              swap,
		         DDS_ReturnCode_t *error);

/* Get the field offset of a structure field (field = index) from CDR-encoded
   data (data) with the specified alignment offset (hsize).
   If swap is set, the marshalled data is in non-native byte order.
   If successful, a non-0 length is returned.  If not, 0 will be returned and
   *error will be set to a specific error code. */

DDS_ReturnCode_t cdr_dump_native (unsigned   indent,
				  const void *data,
				  const Type *type,
			          int        dynamic,
				  int        key,
				  int        names);

/* Dump a native data sample (data). The indent parameter specifies the number
   of initial tabs when printing the sample contents. If key is given, only the
   key fields will be displayed.  If names is set, then field names will be
   displayed together with field data. */

DDS_ReturnCode_t cdr_dump_cdr (unsigned   indent,
			       const void *data,
			       size_t     hsize,
			       const Type *type,
			       int        key,
			       int        msize,
			       int        swap,
			       int        names);

/* Dump a CDR-encoded data sample (data) with the specified alignment offset
   (hsize).  The indent parameter specifies the number of initial tabs when
   printing the sample contents. If names is set, then field names will be
   displayed as well as field data. */

#endif /* !__xcdr_h_ */

