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

/* typecode.h -- Various functions for type conversions and marshalling. */

#ifndef __typecode_h_
#define __typecode_h_

#include "dds/dds_dcps.h"
#ifdef XTYPES_USED
#include "type_data.h"
#endif

typedef struct DDS_TypeSupport_st TypeSupport_t;

#ifdef DDS_TYPECODE
#include "vtc.h"
#endif
#include "db.h"

/* Type support modes: */
typedef enum {
	MODE_CDR,	/* Standard CDR encoded type. */
	MODE_PL_CDR,	/* Parameter list CDR (i.e. sparse) type. */
	MODE_XML,	/* XML encoded type. */
	MODE_RAW	/* Raw data. */
#ifdef DDS_TYPECODE
      ,	MODE_V_TC,	/* Vendor specific Typecode. */
	MODE_X_TC	/* X-Types Typecode. */
#endif
} TS_MODE;

#ifdef DDS_TYPECODE
#define	MAX_TS_MODES	6
#else
#define MAX_TS_MODES	4
#endif

#ifndef XTYPES_USED

/** Identifies the CDR type properties: */
typedef struct cdr_typesupport_st CDR_TypeSupport;
struct cdr_typesupport_st {
	unsigned	typecode:5;	/**< Basic type identifier. */
	unsigned	key:1;		/**< Key type. */
	unsigned	refcnt:9;	/**< Reference count. */
	unsigned	marker:1;	/**< Marker. */
	unsigned	length:16;	/**< Length of the type. */
	const char	*name;		/**< Name of type/field. */
	CDR_TypeSupport	*ts;		/**< Pointer to full type description.*/
};

#endif

typedef enum {
	BT_Participant,
	BT_Publication,
	BT_Subscription,
	BT_Topic,
	BT_None
} Builtin_Type_t;

typedef struct pl_typesupport_st PL_TypeSupport;
struct pl_typesupport_st {
	int		builtin;
	Builtin_Type_t	type;
#ifdef XTYPES_USED
	Type		*xtype;
#endif
};

#ifdef DDS_TYPECODE

typedef enum {
	TSO_Meta,		/* Locally created from TSM. */
	TSO_Builtin,		/* Locally created for builtin topic. */
	TSO_Dynamic,		/* Locally created from Dynamic type. */
	TSO_Typecode		/* Converted from received typecode. */
} TS_Origin_t;

#endif

/* Generic typecode info structure.
   Notes:  - If ts_keys is set, this means that the type can have multiple
	     instances and that key field access functions will be used.
	   - The ts_mkeysize field may be 0 when the size is not known upfront,
	     i.e. the key data contains unbounded fields.  Otherwise, is gives
	     the total length of the big-endian marshalled key fields.
	   - There are multiple marshalling mechanisms permitted, but this will
	     only be used on reception.
	   - On transmission, when the destination is on the same device, in 
	     the same execution environment and the data is not unbounded,
	     marshalling will be avoided.  In all other cases, the ts_prefer
	     marshalling mechanism will be used. */
struct DDS_TypeSupport_st {
	const char		*ts_name;	/* Default type name. */
	size_t			ts_length;	/* Unmarshalled sample size. */
	size_t			ts_mkeysize;	/* Marshalled key fields size.*/
	TS_MODE			ts_prefer;	/* Preferred remote mode. */
	unsigned		ts_keys:1;	/* Key fields are present. */
	unsigned		ts_fksize:1;	/* Fixed key fields size. */
	unsigned		ts_dynamic:1;	/* Dynamic data size. */
#ifdef DDS_TYPECODE
	unsigned		ts_origin:2;	/* Who created this type. */
#endif
	const DDS_TypeSupport_meta *ts_meta;	/* Original metatype. */
	unsigned		ts_users;	/* # of users of this type. */
	union {
#ifdef XTYPES_USED
	 Type			*cdr;		/* CDR marshalling info. */
#else
	 CDR_TypeSupport	*cdr;		/* CDR marshalling info. */
#endif
	 PL_TypeSupport		*pl;		/* Parameter List CDR info. */
	 void			*xml;		/* XML marshalling info. */
#ifdef DDS_TYPECODE
	 VTC_Header_t		*tc;		/* Typecode data. */
#endif
	}			ts [MAX_TS_MODES];
#define	ts_cdr	ts [MODE_CDR].cdr
#define	ts_pl	ts [MODE_PL_CDR].pl
#define	ts_xml	ts [MODE_XML].xml
#define	ts_raw	ts [MODE_RAW].xml
#ifdef DDS_TYPECODE
#define	ts_vtc	ts [MODE_V_TC].tc
#define	ts_xtc	ts [MODE_X_TC].tc
#endif
};

#ifndef MAX_KEY_BUFFER
#define	MAX_KEY_BUFFER		256
#endif

int dds_typesupport_init (void);

/* Initialize Typesupport code. */

void dds_typesupport_final (void);

/* Cleanup Typesupport code. */

size_t DDS_MarshalledDataSize (const void          *sample,
			       int                 dynamic,
			       const TypeSupport_t *ts,
			       DDS_ReturnCode_t    *ret);

/* Return the buffer size needed for marshalled data. */

DDS_ReturnCode_t DDS_MarshallData (void                *buffer,
			           const void          *sample,
			           int                 dynamic,
			           const TypeSupport_t *ts);

/* Marshall payload data using the proper type support processing. */

size_t DDS_UnmarshalledDataSize (DBW                 data,
			         const TypeSupport_t *ts,
			         DDS_ReturnCode_t    *error);

/* Return the buffer size needed for unmarshalled data. */

DDS_ReturnCode_t DDS_UnmarshallData (void                *buffer,
				     DBW                 *data,
				     const TypeSupport_t *ts);

/* Unmarshall payload data using the proper type support processing. */

size_t DDS_KeySizeFromNativeData (const unsigned char *data,
				  int                 dynamic,
				  const TypeSupport_t *ts,
				  DDS_ReturnCode_t    *error);

/* Return the size of the key fields from a given native data sample. */

DDS_ReturnCode_t DDS_KeyFromNativeData (unsigned char       *key,
					const void          *data,
				        int                 dynamic,
					int                 secure,
					const TypeSupport_t *ts);

/* Extract the key fields from a non-marshalled native data sample (data). */

DDS_ReturnCode_t DDS_KeyToNativeData (void                *data,
				      int                 dynamic,
				      int                 secure,
				      const void          *key,
				      const TypeSupport_t *ts);

/* Copy the key fields to a native data sample. */

size_t DDS_KeySizeFromMarshalled (DBW                 data,
				  const TypeSupport_t *ts,
				  int                 key,
				  DDS_ReturnCode_t    *error);

/* Return the size of the key fields from a marshalled data/key sample. */

DDS_ReturnCode_t DDS_KeyFromMarshalled (unsigned char       *dst,
					DBW                 data,
					const TypeSupport_t *ts,
					int                 key,
					int                 secure);

/* Extract the key fields from a marshalled data/key sample. */

DDS_ReturnCode_t DDS_HashFromKey (unsigned char       hash [16],
				  const unsigned char *key,
				  size_t              key_size,
				  int                 secure,
				  const TypeSupport_t *ts);

/* Calculate the hash value from a key. */

DDS_ReturnCode_t DDS_TypeSupport_delete (TypeSupport_t *ts);

/* Delete a previously constructed type. */

#define	XDF_TS_HEADER	1	/* Display Typesupport header. */
#define	XDF_SIZE	2	/* Display sizes. */
#define	XDF_ESIZE	4	/* Display element sizes. */
#define	XDF_OFFSET	8	/* Display field offsets. */

#define	XDF_ALL	(XDF_TS_HEADER | XDF_SIZE | XDF_ESIZE | XDF_OFFSET)

void DDS_TypeSupport_dump_type (unsigned            indent,
				const TypeSupport_t *ts,
				unsigned            flags);

/* Dump typecode data for visual analysis. */

void DDS_TypeSupport_dump_data (unsigned            indent,
				const TypeSupport_t *ts,
				const void          *data,
				int                 native,
				int                 dynamic,
				int                 field_names);

/* Dump a data sample in a formatted manner using the specified typecode. */

void DDS_TypeSupport_dump_key (unsigned            indent,
			       const TypeSupport_t *ts,
			       const void          *key,
			       int                 native,
			       int                 dynamic,
			       int                 secure,
			       int                 field_names);

/* Dump CDR-formatted key data using the specified typecode. */


#endif /* !__typecode_h_ */

