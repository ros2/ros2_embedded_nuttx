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

#ifndef __cdr_h_
#define __cdr_h_

#include "typecode.h"

#define	cdr_is_container(tc)	((tc) >= CDR_TYPECODE_STRUCT && (tc) <= CDR_TYPECODE_ARRAY)
#define	cdr_is_struct(tc)	((tc) >= CDR_TYPECODE_STRUCT && (tc) <= CDR_TYPECODE_ENUM)
#define	cdr_is_array(tc)	((tc) == CDR_TYPECODE_ARRAY || (tc) == CDR_TYPECODE_SEQUENCE)
#define	cdr_is_string(tc)	((tc) == CDR_TYPECODE_CSTRING || (tc) == CDR_TYPECODE_WSTRING)

/** Identifies an element of a struct */
typedef struct CDR_TypeSupport_structelem {
	CDR_TypeSupport	ts;	/**< The basic type info. */
	unsigned	offset;	/**< The offset in the struct. */
	int		label;	/**< The label of the field in case of a union. */
} CDR_TypeSupport_structelem;

/** Identifies the generic properties of a container. */
typedef struct CDR_TypeSupport_container {
	CDR_TypeSupport	ts;	  /**< Inherited basic type properties. */
	unsigned	numelems; /**< Nbr of elements in the struct. */
} CDR_TypeSupport_container;

/** Identifies the properties of a struct */
typedef struct CDR_TypeSupport_struct {
	CDR_TypeSupport_container	container;
	CDR_TypeSupport_structelem	elements [1]; /**< Array of size at 
		least 1, containing the basic type properties of the elements.*/
} CDR_TypeSupport_struct;

/** Identifies the properties of an array */
typedef struct CDR_TypeSupport_array {
	CDR_TypeSupport_container	container;
	CDR_TypeSupport			el_ts;
} CDR_TypeSupport_array;

/** Array with the lengths of the basic types. */
extern const unsigned typecode_len [CDR_TYPECODE_MAX + 1];

void cdr_init (void);

/** CDR marshalled data size calculation from native data.
 *
 * \param[in] hsize The destination offset of the marshalled data for
 *		    alignment purposes due to an additional encapsulation
 *		    header (typically 4 in DDS-CDR, 1 in CORBA-CDR).
 * \param[in] data A pointer to the unmarshalled data (C struct).
 * \param[in] ts The corresponding DDS type description.
 * \param[in] key Key marshalling mode, i.e. only the key fields are processed.
 * \param[in] msize Key mode: pad strings to their maximum size.
 * \param[out] error A pointer to an error code that will be filled in when
 *                   an error occurs.  An error code of 0 implies no error.
 *
 * \return The resulting marshalled data size if successful or 0.
 **/
size_t cdr_marshalled_size (size_t                hsize,
			    const void            *data,
			    const CDR_TypeSupport *ts,
			    int                   key,
			    int                   msize,
			    DDS_ReturnCode_t      *error);

/** CDR marshalling from native data.
 *
 * \param[in] dest A pointer to a data buffer for the marshalled data.
 * \param[in] hsize The destination offset of the marshalled data for
 *		    alignment purposes due to an additional encapsulation
 *		    header (typically 4 in DDS-CDR, 1 in CORBA-CDR).
 * \param[in] data A pointer to the unmarshalled original data (flat C struct).
 * \param[in] ts The corresponding DDS type description.
 * \param[in] key Key marshalling if set: only the key fields are added.
 * \param[in] msize Key marshalling: maximum sized fields are created.
 * \param[in] swap Change endianness of marshalled data.
 *
 * \return An error code (>0) if not successful.
 **/
DDS_ReturnCode_t cdr_marshall (void                  *dest,
			       size_t                hsize,
			       const void            *data,
			       const CDR_TypeSupport *ts,
			       int                   key,
			       int                   msize,
			       int                   swap);

/** Get the unmarshalled, i.e. native data size from CDR-encoded source data.
 *
 * \param[in] data A pointer to the unmarshalled data (C struct).
 * \param[in] hsize The source offset of the marshalled data for
 *		    alignment purposes due to an additional encapsulation
 *		    header (typically 4 in DDS-CDR, 1 in CORBA-CDR).
 * \param[in] ts The corresponding DDS type description.
 * \param[in] key If set, the marshalled data is a concatenated key list.
 * \param[in] msize Key unmarshalling: maximum sized fields in key.
 * \param[in] swap If set, the marshalled data is swapped and needs to be
 *                 swapped again for the correct order.
 * \param[out] error A pointer to an error code that will be filled in when
 *                   an error occurs.  An error code of 0 implies no error.
 **/
size_t cdr_unmarshalled_size (const void            *data,
			      size_t                hsize,
			      const CDR_TypeSupport *ts,
			      int                   key,
			      int                   msize,
			      int                   swap,
			      DDS_ReturnCode_t      *error);

/** Get the unmarshalled, i.e. native data from CDR-encoded source data.
 *
 * \param[in] dest The resulting unmarshalled data.
 * \param[in] data The CDR data that needs to be unmarshalled.
 * \param[in] hsize The source offset of the marshalled data for
 *		    alignment purposes due to an additional encapsulation
 *		    header (typically 4 in DDS-CDR, 1 in CORBA-CDR).
 * \param[in] ts The corresponding CDR type description.
 * \param[in] key If set, the marshalled data is a concatenated key list.
 * \param[in] msize Key unmarshalling: maximum sized fields in key.
 * \param[in] swap If set, the marshalled data is swapped and needs to be
 *                 swapped again for the correct order.
 * \return A non-zero DDS error code if something went wrong.
 */
DDS_ReturnCode_t cdr_unmarshall (void                  *dest,
				 const void            *data,
			         size_t                hsize,
				 const CDR_TypeSupport *ts,
				 int                   key,
			         int                   msize,
				 int                   swap);

/** Calculate the size of the CDR-encoded keu fields.
 *
 * \param[in] data CDR marshalled key list data.
 * \param[in] hsize The source offset of the marshalled data for
 *		    alignment purposes due to an additional encapsulation
 *		    header (typically 4 in DDS-CDR, 1 in CORBA-CDR).
 * \param[in] ts The corresponding CDR type description.
 * \param[in] key The original marshalled data is a concatenated key list.
 * \param[in] msize If set, the key list contains max-sized entries.
 * \param[in] swap The marshalled data is in non-native byte order.
 * \param[out] error A pointer to an error code that will be filled in when
 *                   an error occurs.  An error code of 0 implies no error.
 * \return Total size of the CDR-encapsulated key fields.
 */
size_t cdr_key_size (const void            *data,
		     size_t                hsize,
		     const CDR_TypeSupport *ts,
		     int                   key,
		     int                   msize,
		     int                   swap,
		     DDS_ReturnCode_t      *error);

/** CDR-encoded key fields extraction from marshalled data.
 *
 * \param[in] dest The resulting key data.
 * \param[in] data CDR marshalled original data.
 * \param[in] hsize The destination offset of the marshalled data for
 *		    alignment purposes due to an additional encapsulation
 *		    header (typically 4 in DDS-CDR, 1 in CORBA-CDR).
 * \param[in] ts The corresponding CDR type description.
 * \param[in] key The original marshalled data is a concatenated key list.
 * \param[in] msize If set, the key list needs to be max-sized.
 * \param[in] swap If set, the marshalled data will be swapped.
 * \return A DDS error code if unsuccessful.
 */
DDS_ReturnCode_t cdr_key_fields (void                  *dest,
				 size_t                dhsize,
				 const void            *data,
				 size_t                hsize,
				 const CDR_TypeSupport *ts,
				 int                   key,
				 int                   msize,
				 int                   swap,
				 int                   iswap);


/** CDR-encoded data field offset calculation from marshalled data.
 *
 * \param[in] data CDR marshalled original data.
 * \param[in] hsize The destination offset of the marshalled data for
 *		    alignment purposes due to an additional encapsulation
 *		    header (typically 4 in DDS-CDR, 1 in CORBA-CDR).
 * \param[in] field Field index of data to retrieve.
 * \param[in] ts The corresponding CDR type description.
 * \param[in] swap If set, the marshalled data will be swapped.
 * \return A DDS error code if unsuccessful.
 */
size_t cdr_field_offset (const void            *data,
		         size_t                hsize,
		         unsigned              field,
		         const CDR_TypeSupport *ts,
			 int                   swap,
		         DDS_ReturnCode_t      *error);

/** CDR dump typecode.
 *
 * \param[in] indent The indentation level.
 * \param[in] ts The CDR type description.
 */
void cdr_dump_type (unsigned indent, CDR_TypeSupport *ts);

/** CDR dump typecode + corresponding data.
 *
 * \param[in] indent The indentation level.
 * \param[in] ts The CDR type description.
 * \param[in] data The data sample that should be dumped.
 */
void cdr_dump_data (unsigned indent, CDR_TypeSupport *ts, const void *data);

#endif
