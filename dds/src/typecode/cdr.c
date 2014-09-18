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

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#ifdef _WIN32
#include "win.h"
#else
#include <inttypes.h>
#include <arpa/inet.h>
#endif
#include "sys.h"
#include "pool.h"
#include "prof.h"
#include "error.h"
#ifdef DDS_DEBUG
#include "debug.h"
#endif
#include "cdr.h"

#ifdef _WIN32
#define INLINE
#else
#define INLINE inline
#endif

#ifdef PROFILE
PROF_PID (cdr_m_size)
PROF_PID (cdr_m)
PROF_PID (cdr_um_size)
PROF_PID (cdr_um)
PROF_PID (cdr_k_size)
PROF_PID (cdr_k_get)
PROF_PID (cdr_f_ofs)
#endif

#define	ALIGN(delta, boundary)	((delta + boundary - 1) & ~(boundary - 1))
#define CDR_ALIGN(delta, boundary, write, out) for (; delta < ALIGN (delta, boundary); \
						delta++) if (write) (out) [delta] = '\0';

/* cdr_marshalled_container_init -- Helper function to initialize the function
                                    cdr_marshalled_container. */

static INLINE void cdr_marshalled_container_init (const CDR_TypeSupport_container *ts,
						  const void                      **data,
						  unsigned                        *num_elems,
						  const CDR_TypeSupport           **el_ts,
						  CDR_TypeCode_t                  *tc,
						  size_t                          *el_size)
{
	*el_size = 0;
	*el_ts = NULL;
	*tc = CDR_TYPECODE_UNKNOWN;
	*num_elems = ts->numelems;
	if (ts->ts.typecode == CDR_TYPECODE_ARRAY ||
	    ts->ts.typecode == CDR_TYPECODE_SEQUENCE) {
		CDR_TypeSupport_array *ts_a = (CDR_TypeSupport_array *) ts;

		*tc = ts_a->el_ts.typecode;
		if (*tc == CDR_TYPECODE_ARRAY || *tc == CDR_TYPECODE_SEQUENCE ||
		    *tc == CDR_TYPECODE_STRUCT || *tc == CDR_TYPECODE_UNION)
			*el_ts = ts_a->el_ts.ts;
		else
			*el_ts = &ts_a->el_ts;
		if ((CDR_TYPECODE_CSTRING == (*el_ts)->typecode) &&
		    ((*el_ts)->length) == 0)
			/* In case of an unbounded CSTRING the element size is the char* size. */
			*el_size = sizeof (char *);
		else
			*el_size = (*el_ts)->length;
		if (ts->ts.typecode == CDR_TYPECODE_SEQUENCE) {
			if (!*data)
				*num_elems = 0;
			else {
				*num_elems = ((DDS_VoidSeq *)*data)->_length;
				*data = ((DDS_VoidSeq *)*data)->_buffer;
			}
		}
	}
}

/* cdr_marshall_simple -- Marshall simple types (char, int, string, ...) in the
			  output buffer. If key is set, only the key fields are
			  marshalled and concatenated in the output buffer.  If
			  not, all the fields are added to the output buffer.
			  If swap is set, the content of numbers is swapped. */


static size_t cdr_marshall_simple (char                  *out,
				   const void            *data,
				   const CDR_TypeSupport *ts,
				   int                   key,
				   int                   msize,
				   int                   swap,
				   size_t                offset)
{
	int		write = (out && data) ? 1 : 0;
	CDR_TypeCode_t	tc = ts->typecode;
	size_t		delta = offset;

	switch (tc) {
		case CDR_TYPECODE_CHAR:
		case CDR_TYPECODE_OCTET:
		case CDR_TYPECODE_BOOLEAN:
			if (!key || ts->key) {
				if (write)
					out [delta] = *(char *) data;
				delta++;
			}
			break;

		case CDR_TYPECODE_SHORT:
		case CDR_TYPECODE_USHORT:
			if (!key || ts->key) {
				CDR_ALIGN (delta, 2, write, out);
				if (write) {
					if (swap)
						memcswap16 (&out [delta], data);
					else
						memcpy16 (&out [delta], data);
				}
				delta += 2;
			}
			break;

		case CDR_TYPECODE_LONG:
		case CDR_TYPECODE_ULONG:
		case CDR_TYPECODE_FLOAT:
		case CDR_TYPECODE_ENUM:
			if (!key || ts->key) {
				CDR_ALIGN (delta, 4, write, out);
				if (write) {
					if (swap)
						memcswap32 (&out [delta], data);
					else
						memcpy32 (&out [delta], data);
				}
				delta += 4;
			}
			break;

		case CDR_TYPECODE_LONGLONG:
		case CDR_TYPECODE_ULONGLONG:
		case CDR_TYPECODE_DOUBLE:
			if (!key || ts->key) {
				CDR_ALIGN (delta, 8, write, out);
				if (write) {
					if (swap)
						memcswap64 (&out [delta], data);
					else
						memcpy64 (&out [delta], data);
				}
				delta += 8;
			}
			break;
#ifdef LONGDOUBLE
		case CDR_TYPECODE_LONGDOUBLE:
			if (!key || ts->key) {
				CDR_ALIGN (delta, 8, write, out);
				if (write) {
					if (swap) {
						memcswap64 (&out [delta], data);
						memcswap64 (&out [delta + 8], (uint64_t *) data + 1);
					}
					else {
						memcpy64 (&out [delta], data);
						memcpy64 (&out [delta + 8], (uint64_t *) data + 1);
					}
				}
				delta += 16;
			}
			break;
#endif
		case CDR_TYPECODE_CSTRING:
			if (!key || ts->key) {
				/* Bounded/unbounded string handling. */
				const char *ptr = (ts->length) ? data : (data) ? *(const char **) data : NULL;
				unsigned   len = (ptr) ? strlen (ptr) + 1 : 1;

				CDR_ALIGN (delta, 4, write, out);
				if (write) {
					if (swap)
						memcswap32 (&out [delta], &len);
					else
						memcpy32 (&out [delta], &len);
				}
				delta += 4;

				if (ptr) {
					if (write)
						memcpy (&out [delta], ptr, len);
					delta += len;
				}
				else {
					if (write)
						out [delta] = '\0';
					delta++;
				}
				if (key && msize && ts->length) {

					/* Pad to max. size in key mode. */
					if (len <= ts->length) {
						if (write)
							memset (&out [delta], 0,
									ts->length - len + 1);
						delta += ts->length - len + 1;
					}
				}
			}
			break;

		default:
			return (0);
	}
	return (delta);
}

/* cdr_marshall_container -- Marshall a C-structure, array or sequence data type
			     in the output buffer. If key is set, only the
			     key fields are marshalled and concatenated in the
			     output buffer.  If msize is set in combination with
			     key, strings are padded to the max. length.
			     If key is not set, all the fields are added to the
			     output buffer. If swap is set, the content of
			     numbers is swapped. */

static size_t cdr_marshall_container (char                            *out,
				      const void                      *data,
				      const CDR_TypeSupport_container *ts,
				      int                             key,
				      int                             msize,
				      int                             swap,
				      size_t                          offset)
{
	unsigned int		i;
	size_t			delta = offset;
	size_t			el_size;
	CDR_TypeCode_t		tc;
	const CDR_TypeSupport	*el_ts;
	unsigned		num_elems;
	int			write = out ? 1 : 0;
	unsigned		first_elem = 0;

	cdr_marshalled_container_init (ts, &data, &num_elems, &el_ts, &tc, &el_size);
	if (ts->ts.typecode == CDR_TYPECODE_SEQUENCE) {

		/* Insert the number of element in the sequence */
		if (!key || ts->ts.key) {
			uint32_t nelem = num_elems;

			CDR_ALIGN (delta, 4, write, out);
			if (write) {
				if (swap)
					memcswap32 (&out [delta], &nelem);
				else
					memcpy32 (&out [delta], &nelem);
			}
			delta += 4;
		}
		else
			return (delta);
	}
	else if (ts->ts.typecode == CDR_TYPECODE_UNION) {

		/* Insert the union discriminant. */
		if (!key || ts->ts.key) {
			CDR_ALIGN (delta, 4, write, out);
			if (write) {
				if (swap)
					memcswap32 (&out [delta], data);
				else
					memcpy32 (&out [delta], data);
			}
			delta += 4;

			if (data) {
				for (first_elem = 0; first_elem < num_elems; first_elem++) {
					const CDR_TypeSupport_struct *ts_struct = (const CDR_TypeSupport_struct *)ts;
					if (ts_struct->elements [first_elem].label == *((int *) data)) {
						break;
					}
				}

				/* label not found, return 0 */
				if (first_elem >= num_elems)
					return (0);
			}

			/* MAXIMUM mode, find the largest element of the union */
			else {
				/* TODO: Union marked as key not yet handled 
				for (i = 0; i < num_elems; i++) {
					const CDR_TypeSupport_struct *ts_struct = (const CDR_TypeSupport_struct *)ts;
					ts_struct->elements [i].ts.length
				}
				*/
				return (0);
			}
			num_elems = first_elem + 1;
		}
		else
			return (delta);
	}

	for (i = first_elem; i < num_elems; i++) {
		const char *ptr = NULL;

		if (ts->ts.typecode == CDR_TYPECODE_STRUCT ||
		    ts->ts.typecode == CDR_TYPECODE_UNION) {
			const CDR_TypeSupport_struct *ts_struct = (const CDR_TypeSupport_struct *)ts;

			tc = ts_struct->elements [i].ts.typecode;
			el_ts = ts_struct->elements [i].ts.ts ? ts_struct->elements [i].ts.ts : &ts_struct->elements [i].ts;

			/* Place the data ptr on the first element value. */
			if (data)
				ptr = (char *) data + ts_struct->elements [i].offset;
		}
		else if (data)
			ptr = (char *) data + el_size * i;

		switch (tc) {
#ifdef LONGDOUBLE
			case CDR_TYPECODE_LONGDOUBLE:
				break;
#endif
			case CDR_TYPECODE_CHAR:
			case CDR_TYPECODE_OCTET:
			case CDR_TYPECODE_BOOLEAN:
			case CDR_TYPECODE_SHORT:
			case CDR_TYPECODE_USHORT:
			case CDR_TYPECODE_LONG:
			case CDR_TYPECODE_ULONG:
			case CDR_TYPECODE_FLOAT:
			case CDR_TYPECODE_ENUM:
			case CDR_TYPECODE_LONGLONG:
			case CDR_TYPECODE_ULONGLONG:
			case CDR_TYPECODE_DOUBLE:
			case CDR_TYPECODE_CSTRING:
				delta = cdr_marshall_simple (out, ptr, el_ts, key, msize, swap, delta);
				break;

			case CDR_TYPECODE_UNION:
			case CDR_TYPECODE_STRUCT:
			case CDR_TYPECODE_ARRAY:
			case CDR_TYPECODE_SEQUENCE:
				delta = cdr_marshall_container (out, ptr, 
								(CDR_TypeSupport_container *) el_ts,
								key, msize, swap, delta);
				if (!delta)
					return (0);
				break;

			default:
				return (0);
		}
	}
	return (delta);
}

/* cdr_marshalled_size -- Return the marshalled size of native data.
			  Can be used for key fields only or for all fields. */

size_t cdr_marshalled_size (size_t                hsize,
			    const void            *data,
			    const CDR_TypeSupport *ts,
			    int                   key,
			    int                   msize,
			    DDS_ReturnCode_t      *error)
{
	size_t	length;

	prof_start (cdr_m_size);

	if ((ts->typecode != CDR_TYPECODE_STRUCT &&
	     ts->typecode != CDR_TYPECODE_SEQUENCE &&
	     ts->typecode != CDR_TYPECODE_ARRAY &&
	     ts->typecode != CDR_TYPECODE_UNION) ||
	    (length = cdr_marshall_container (NULL, data,
			(CDR_TypeSupport_container *) ts, key, msize, 0, hsize)) == 0) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	if (error)
		*error = DDS_RETCODE_OK;

	prof_stop (cdr_m_size, 1);
	return (length - hsize);
}

/* cdr_marshall -- Marshall native data to the CDR-marshalled format.
		   Can be used for key fields only or for all fields. */

DDS_ReturnCode_t cdr_marshall (void                  *dst,
			       size_t                hsize,
			       const void            *data,
			       const CDR_TypeSupport *ts,
			       int                   key,
			       int                   msize,
			       int                   swap)
{
	size_t	len;
	const CDR_TypeSupport_container *ts_container = (const CDR_TypeSupport_container *) ts;

	prof_start (cdr_m);

	if (!dst || (!key && !data) || !ts ||
	    ts->typecode != CDR_TYPECODE_STRUCT ||
	    !ts_container->numelems)
		return (DDS_RETCODE_BAD_PARAMETER);

	len = cdr_marshall_container ((char *) dst - hsize, data,
				      ts_container, key, msize, swap, hsize) - hsize;
	if (!len)
		return (DDS_RETCODE_BAD_PARAMETER);

	CDR_ALIGN (len, 4, 1, (char *) dst);

	prof_stop (cdr_m, 1);
	return (DDS_RETCODE_OK);
}

/* Field descriptor for the CDR-offset lookup (when CF_OFFSET): */
struct field_desc_st {
	unsigned	depth;		/* Current structure depth. */
	unsigned	field;		/* Field index. */
};

#define CF_TO_KEY	1	/* Destination data is concatenated key data. */
#define CF_TO_MSIZE	2	/* Destination data is maximum sized key data. */
#define CF_FROM_KEY	4	/* Source data is concatenated key data. */
#define CF_FROM_MSIZE	8	/* Source data is maximum sized key data. */
#define CF_SWAP		16	/* Change endianness by swapping data fields. */
#define CF_LENGTH	32	/* No data transfer needed, just get length. */
#define CF_OFFSET	64	/* No data transfer needed, get source offset. */

/* cdr_unmarshall_container -- Convert a marshalled container either to longest
			       sized concatenated key fields or to the native
			       data format. Alternatively, compute the required
			       total buffer length or key buffer length. */

static size_t cdr_unmarshall_container (char                            *dst,
					const char                      *src,
					const CDR_TypeSupport_container *ts,
					unsigned                        flags,
					size_t                          offset,
					size_t                          *doffset,
					void                            **ptr)
{
	size_t delta = offset, len = 0, offset2;
	const CDR_TypeSupport *ts2 = NULL;
	const CDR_TypeSupport_struct *ts_struct = (CDR_TypeSupport_struct *) ts;
	const CDR_TypeSupport_structelem *ep;
	unsigned i, numelems = ts->numelems;
	int union_discr;
	unsigned short selected_el_found = 0;
	size_t *totlen = NULL;
	struct field_desc_st *fdesc = NULL;
	int key = flags & (CF_TO_KEY | CF_FROM_KEY);

	if ((flags & CF_LENGTH) != 0)
		totlen = (size_t *) ptr;
	else if ((flags & CF_OFFSET) != 0)
		fdesc = (struct field_desc_st *) ptr;
	if ((flags & CF_TO_KEY) != 0)
		offset2 = *doffset;
	else
		offset2 = 0;
	if (ts->ts.typecode == CDR_TYPECODE_ARRAY ||
	    ts->ts.typecode == CDR_TYPECODE_SEQUENCE) {
		CDR_TypeSupport_array *ts_a = (CDR_TypeSupport_array *) ts;
		CDR_TypeCode_t tc = ts_a->el_ts.typecode;

		ts2 = &ts_a->el_ts;
		if (tc == CDR_TYPECODE_ARRAY || tc == CDR_TYPECODE_SEQUENCE ||
		    tc == CDR_TYPECODE_STRUCT || tc == CDR_TYPECODE_UNION)
			len = ts_a->el_ts.ts->length;
		else if (ts2->typecode == CDR_TYPECODE_CSTRING && !ts2->length)

			/* In case of an unbounded CSTRING the element
			   size is the char* size. */
			len = sizeof (char *);
		else
			len = ts_a->el_ts.length;

		if (ts->ts.typecode == CDR_TYPECODE_SEQUENCE) {
			uint32_t nelem;
			DDS_VoidSeq *seq = (DDS_VoidSeq *) dst;

			if ((flags & CF_FROM_KEY) == 0 || ts->ts.key) {
				int elem_swap;
				delta = (delta + 3) & ~3;


				if ((flags & CF_TO_MSIZE) == 0)
					elem_swap = (flags & CF_SWAP) != 0;
				else if ((flags & (CF_FROM_KEY | CF_FROM_MSIZE)) == CF_FROM_KEY)
					elem_swap = 0;
				else
					elem_swap = ((flags & CF_SWAP) != 0) ^ ENDIAN_CPU;

				if (elem_swap)
					memcswap32 (&nelem, &src [delta]);
				else
					memcpy32 (&nelem, &src [delta]);

				delta += 4;
				numelems = nelem;
				if (ts_a->container.numelems && (nelem > ts_a->container.numelems))
					return (0);

				if ((flags & CF_TO_KEY) == 0) {
					if ((flags & (CF_LENGTH | CF_OFFSET)) != 0) {
						if ((flags & CF_LENGTH) != 0)
							*totlen += nelem * len;
					}
					else if (!key || ts2->key) {
						seq->_buffer = *ptr;
						*ptr = ((char *) *ptr) + (nelem * len);
						seq->_length = seq->_maximum = nelem;
						seq->_esize = len;
						dst = seq->_buffer;
					}
				} 
				else if (ts2->key) {
					if ((flags & CF_LENGTH) != 0)
						offset2 = ((offset2 + 3) & ~3) + 4;
					else {
						CDR_ALIGN (offset2, 4, 1, dst);
						if ((flags & CF_SWAP) != 0)
							memcswap32 (&dst [offset2], &nelem);
						else
							memcpy32 (&dst [offset2], &nelem);
						offset2 += 4;
					}
				}
			} else {
				numelems = 0;
			}
		}
	}
	for (i = 0, ep = ts_struct->elements; i < numelems; i++, ep++) {

		/* If offset is requested and we reached the correct field,
		   then we're done. */
		if ((flags & CF_OFFSET) != 0 && !fdesc->depth && i == fdesc->field)
			return (delta);

		if (ts->ts.typecode == CDR_TYPECODE_STRUCT ||
		    ts->ts.typecode == CDR_TYPECODE_UNION) {
			len = ep->ts.length;
			ts2 = &ep->ts;

			/* Union discriminant and element handling. */
			if (ts->ts.typecode == CDR_TYPECODE_UNION) {
				int disc_swap;

				/* If we already encountered the discriminant stop iterating over
				 * elements. */
				if (selected_el_found)
					break;

				/* Retrieve the union discriminant. */
				delta = (delta + 3) & ~3;

				if ((flags & CF_FROM_KEY) != 0 && !ts->ts.key)
					continue;

				if ((flags & CF_TO_MSIZE) == 0)
					disc_swap = (flags & CF_SWAP) != 0;
				else if ((flags & (CF_FROM_KEY | CF_FROM_MSIZE)) == CF_FROM_KEY)
					disc_swap = 0;
				else
					disc_swap = ((flags & CF_SWAP) != 0) ^ ENDIAN_CPU;

				if (disc_swap)
					memcswap32 (&union_discr, &src [delta]);
				else
					memcpy32 (&union_discr, &src [delta]);

				/* If the current element is not selected skip to next element. */
				if (union_discr != ep->label)
					continue;

				selected_el_found = 1;

				/* Store discriminant. */
				if ((flags & (CF_LENGTH | CF_OFFSET)) == 0) {
					if ((flags & CF_SWAP) != 0)
						memcswap32 (&dst [offset2], &src[delta]);
					else
						memcpy32 (&dst [offset2], &src[delta]);
				}
				delta += 4;
			}

			/* Determine the offset of the element in the target structure. */
			if ((flags & CF_TO_KEY) == 0)
				offset2 = ep->offset;
		}
		else if ((flags & CF_TO_KEY) == 0)
			offset2 = i * len;

		switch (ts2->typecode) {
			case CDR_TYPECODE_CHAR:
			case CDR_TYPECODE_OCTET:
			case CDR_TYPECODE_BOOLEAN:
				if (!key || ts2->key) {
					if ((flags & (CF_LENGTH | CF_OFFSET)) == 0)
						dst [offset2] = src [delta];
					if ((flags & CF_TO_KEY) != 0)
						offset2++;
				}
				break;

			case CDR_TYPECODE_SHORT:
			case CDR_TYPECODE_USHORT:
				if ((flags & CF_FROM_KEY) == 0 || ts2->key)
					delta = (delta + 1) & ~1;

				if (!key || ts2->key) {
					if ((flags & CF_LENGTH) != 0) {
						if ((flags & CF_TO_KEY) != 0)
							offset2 = ((offset2 + 1) & ~1) + 2;
					}
					else if ((flags & CF_OFFSET) == 0) {
						if ((flags & CF_TO_KEY) != 0)
							CDR_ALIGN (offset2, 2, 1, dst);
						if ((flags & CF_SWAP) != 0)
							memcswap16 (&dst [offset2], &src [delta]);
						else
							memcpy16 (&dst [offset2], &src [delta]);
						if ((flags & CF_TO_KEY) != 0)
							offset2 += 2;
					}
				}
				break;

			case CDR_TYPECODE_LONG:
			case CDR_TYPECODE_ULONG:
			case CDR_TYPECODE_FLOAT:
			case CDR_TYPECODE_ENUM:
				if ((flags & CF_FROM_KEY) == 0 || ts2->key)
					delta = (delta + 3) & ~3;

				if (!key || ts2->key) {
					if ((flags & CF_LENGTH) != 0) {
						if ((flags & CF_TO_KEY) != 0)
							offset2 = ((offset2 + 3) & ~3) + 4;
					}
					else if ((flags & CF_OFFSET) == 0) {
						if ((flags & CF_TO_KEY) != 0)
							CDR_ALIGN (offset2, 4, 1, dst);
						if ((flags & CF_SWAP) != 0)
							memcswap32 (&dst [offset2], &src [delta]);
						else
							memcpy32 (&dst [offset2], &src [delta]);
						if ((flags & CF_TO_KEY) != 0)
							offset2 += 4;
					}
				}
				break;

			case CDR_TYPECODE_LONGLONG:
			case CDR_TYPECODE_ULONGLONG:
			case CDR_TYPECODE_DOUBLE:
				if ((flags & CF_FROM_KEY) == 0 || ts2->key)
					delta = (delta + 7) & ~7;

				if (!key || ts2->key) {
					if ((flags & CF_LENGTH) != 0) {
						if ((flags & CF_TO_KEY) != 0)
							offset2 = ((offset2 + 7) & ~7) + 8;
					}
					else if ((flags & CF_OFFSET) == 0) {
						if ((flags & CF_TO_KEY) != 0)
							CDR_ALIGN (offset2, 8, 1, dst);
						if ((flags & CF_SWAP) != 0)
							memcswap64 (&dst [offset2], &src [delta]);
						else
							memcpy64 (&dst [offset2], &src [delta]);
						if ((flags & CF_TO_KEY) != 0)
							offset2 += 8;
					}
				}
				break;
#ifdef LONGDOUBLE
			case CDR_TYPECODE_LONGDOUBLE:
				if ((flags & CF_FROM_KEY) == 0 || ts2->key)
					delta = (delta + 7) & ~7;

				if (!key || ts2->key) {
					if ((flags & CF_LENGTH) != 0) {
						if ((flags & CF_TO_KEY) != 0)
							offset2 = ((offset2 + 7) & ~7) + 16;
					}
					else if ((flags & CF_OFFSET) == 0) {
						if ((flags & CF_TO_KEY) != 0)
							CDR_ALIGN (offset2, 8, 1, dst);
						if ((flags & CF_SWAP) != 0) {
							memcswap64 (&dst [offset2], &src [delta]);
							memcswap64 (&dst [offset2 + 8], &src [delta + 8]);
						}
						else {
							memcpy64 (&dst [offset2], &src [delta]);
							memcpy64 (&dst [offset2 + 8], &src [delta + 8]);
						}
						if ((flags & CF_TO_KEY) != 0)
							offset2 += 16;
					}
				}
				break;
#endif
			case CDR_TYPECODE_CSTRING: {
				uint32_t cstrlen = 0;
				int len_swap;
				char *dcp;

				if ((flags & CF_FROM_KEY) == 0 || ts2->key) {

					/* Align the source offset. */
					delta = (delta + 3) & ~3;

					/* Calculate the string length. */
					if ((flags & CF_TO_MSIZE) == 0)
						len_swap = (flags & CF_SWAP) != 0;
					else if ((flags & (CF_FROM_KEY | CF_FROM_MSIZE)) == CF_FROM_KEY)
						len_swap = 0;
					else
						len_swap = ((flags & CF_SWAP) != 0) ^ ENDIAN_CPU;
					if (len_swap)
						memcswap32 (&cstrlen, &src [delta]);
					else
						memcpy32 (&cstrlen, &src [delta]);
					if (!cstrlen)
						return (0);

					/* Point source to string data. */
					delta += 4;
				}
				if (!key || ts2->key) {
					if ((flags & (CF_TO_KEY | CF_LENGTH)) == CF_TO_KEY) {

						/* Align destination offset for string length. */
						CDR_ALIGN (offset2, 4, 1, dst);
						if (ts2->key) {
							uint32_t wstrlen;
		
							if ((flags & CF_TO_MSIZE) != 0)
								wstrlen = htonl (cstrlen);
							else
								wstrlen = cstrlen;
							memcpy32 (&dst [offset2], &wstrlen);
							offset2 += 4;
						}
					}
					if ((flags & (CF_LENGTH | CF_OFFSET)) != 0) { /* Length calculation. */
						if ((flags & CF_TO_KEY) != 0) {
							offset2 = ((offset2 + 3) & ~3) + 4;
							if (ts2->length && 
							    (flags & CF_TO_MSIZE) != 0)
							 	offset2 += ts2->length + 1;
							else
								offset2 += cstrlen;
						}
						else if ((flags & CF_LENGTH) != 0 && !ts2->length)
							/* Unbounded string: increase total length
							   with unbounded string length. */
							*totlen += cstrlen;
					}
					else if (ts2->length) {	/* Bounded string copy. */
						if (cstrlen > ts2->length + 1U)
							return (0);
								
						if (cstrlen > 1)
							memcpy (&dst [offset2], &src [delta], cstrlen - 1);

						if ((flags & CF_TO_KEY) != 0) {
							if ((flags & CF_TO_MSIZE) != 0 &&
							    ts2->length + 1U > cstrlen - 1U) {
								memset (&dst [offset2 + cstrlen - 1], 0, 
									       ts2->length + 2 - cstrlen);
								offset2 += ts2->length + 1;
							}
							else {
								dst [offset2 + cstrlen -1] = '\0';
								offset2 += cstrlen;
							}
						}
						/* Zero out the remainder of the string if we are not  working on a 
						 * minimal size key (if there is a remainder) */
						else if (ts2->length >= cstrlen)
							memset (&dst [offset2 + cstrlen - 1], 0,
								ts2->length - cstrlen + 1); 
						if ((flags & CF_FROM_MSIZE) != 0)
							delta += ts2->length - cstrlen;
					}
					else {	/* Unbounded string: copy string data
					           to the destination buffer. */
						if ((flags & CF_TO_KEY) != 0) { /* Key? */
							memcpy (&dst [offset2], &src [delta], cstrlen);
							offset2 += cstrlen;
						}
						else if ((flags & CF_FROM_KEY) != 0) {

							/* Key data requested. */
							memcpy (&dcp, &dst [offset2], sizeof (char *));
							if (!dcp) {
								dcp = malloc (cstrlen);
								if (!dcp)
									return (0);
							}
							memcpy (dcp, &src [delta], cstrlen);
							memcpy (&dst [offset2], &dcp, sizeof (char *));
						}
						else {
							/* Copy to extra buffer space. */
							memcpy (*ptr, &src [delta], cstrlen);

							/* Set the char * to the correct value. */
							memcpy (&dst [offset2], ptr, sizeof (char *));
							*ptr = ((char *) *ptr) + cstrlen;
						}
					}
				}
				if ((flags & CF_FROM_KEY) == 0 || ts2->key)
					delta += cstrlen;
				continue;
			}
			case CDR_TYPECODE_SEQUENCE:
			case CDR_TYPECODE_UNION:
			case CDR_TYPECODE_STRUCT:
			case CDR_TYPECODE_ARRAY:
				if ((flags & CF_TO_KEY) != 0) {
					*doffset = offset2;

					if ((flags & CF_OFFSET) != 0)
						fdesc->depth++;
					delta = cdr_unmarshall_container (dst, src,
					 (CDR_TypeSupport_container *) ts2->ts,
					 		flags, delta, doffset, ptr);
					if ((flags & CF_OFFSET) != 0)
						fdesc->depth--;
					offset2 = *doffset;
				}
				else {
					if ((flags & CF_OFFSET) != 0)
						fdesc->depth++;
					delta = cdr_unmarshall_container (
							&dst [offset2], src,
					     (CDR_TypeSupport_container *) ts2->ts,
					 		flags, delta, doffset, ptr);
					if ((flags & CF_OFFSET) != 0)
						fdesc->depth--;
				}
				if (!delta && (flags & CF_FROM_KEY) == 0)
					return (0);

				continue;

			default:
				return (0);
		}
		if ((flags & CF_FROM_KEY) == 0 || ts2->key)
			delta += len;
	}
	*doffset = offset2;
	return (delta);
}

size_t cdr_unmarshalled_size (const void            *data,
			      size_t                hsize,
			      const CDR_TypeSupport *ts,
			      int                   key,
			      int                   msize,
			      int                   swap,
			      DDS_ReturnCode_t      *error)
{
	const CDR_TypeSupport_struct *ts_struct = (const CDR_TypeSupport_struct *) ts;
	size_t len = 0;
	size_t doffset = hsize;
	unsigned flags;
	DDS_ReturnCode_t err = DDS_RETCODE_BAD_PARAMETER;

	prof_start (cdr_um_size);

	if (!data || !ts || ts->typecode != CDR_TYPECODE_STRUCT ||
	    !ts_struct->container.numelems)
		goto out;

	flags = CF_LENGTH;
	if (key) {
		flags |= CF_FROM_KEY;
		if (msize)
			flags |= CF_FROM_MSIZE;
	}
	if (swap)
		flags |= CF_SWAP;
	if (cdr_unmarshall_container (NULL, (const char *) data - hsize,
			    	       (const CDR_TypeSupport_container *) ts,
				       flags, hsize, &doffset, (void **) &len)) {
		err = DDS_RETCODE_OK;
		len += ts->length;
	}

    out:
	if (error)
		*error = err;

	prof_stop (cdr_um_size, 1);
	return (len);
}

DDS_ReturnCode_t cdr_unmarshall (void                  *dest,
				 const void            *data,
			         size_t                hsize,
				 const CDR_TypeSupport *ts,
				 int                   key,
				 int                   msize,
				 int                   swap)
{
	const CDR_TypeSupport_struct *ts_struct = (const CDR_TypeSupport_struct *) ts;
	void *ptr;
	unsigned flags;
	size_t doffset = 0; 

	prof_start (cdr_um);

	if (!dest || !data || !ts ||
	    ts->typecode != CDR_TYPECODE_STRUCT ||
	    !ts_struct->container.numelems)
		return (DDS_RETCODE_BAD_PARAMETER);

	ptr = ((char *) dest) + ts_struct->container.ts.length;
	flags = 0;
	if (key) {
		flags |= CF_FROM_KEY;
		if (msize)
			flags |= CF_FROM_MSIZE;
	}
	if (swap)
		flags |= CF_SWAP;
	if (!cdr_unmarshall_container (dest, (const char *) data - hsize,
			    	       (const CDR_TypeSupport_container *) ts,
				       flags, hsize, &doffset, &ptr))
		return (DDS_RETCODE_BAD_PARAMETER);
	else {
		prof_stop (cdr_um, 1);
		return (DDS_RETCODE_OK);
	}
}

size_t cdr_key_size (const void            *data,
		     size_t                hsize,
		     const CDR_TypeSupport *ts,
		     int                   key,
		     int                   msize,
		     int                   swap,
		     DDS_ReturnCode_t      *error)
{
	const CDR_TypeSupport_struct *ts_struct = (const CDR_TypeSupport_struct *) ts;
	size_t len = 0;
	size_t doffset = hsize;
	unsigned flags;
	DDS_ReturnCode_t err = DDS_RETCODE_BAD_PARAMETER;

	prof_start (cdr_k_size);

	if (!data || !ts || ts->typecode != CDR_TYPECODE_STRUCT ||
	    !ts_struct->container.numelems)
		goto out;

	flags = CF_TO_KEY | CF_LENGTH;
	if (key)
		flags |= CF_FROM_KEY;
	if (msize)
		flags |= CF_TO_MSIZE;
	if (swap)
		flags |= CF_SWAP;
	if (cdr_unmarshall_container (NULL, (const char *) data - hsize,
			    	       (const CDR_TypeSupport_container *) ts,
				       flags, hsize, &doffset, (void **) &len)) {
		err = DDS_RETCODE_OK;
		len = doffset - hsize;
	}
    out:
	if (error)
		*error = err;

	prof_stop (cdr_k_size, 1);
	return (len);
}

DDS_ReturnCode_t cdr_key_fields (void                  *dest,
				 size_t                dhsize,
				 const void            *data,
				 size_t                hsize,
				 const CDR_TypeSupport *ts,
				 int                   key,
				 int                   msize,
				 int                   swap,
				 int                   iswap)
{
	unsigned flags;
	size_t doffset = hsize; 

	ARG_NOT_USED (iswap)

	prof_start (cdr_k_get);

	if (!dest || !data || !ts)
		return (DDS_RETCODE_BAD_PARAMETER);

	flags = CF_TO_KEY;
	if (key)
		flags |= CF_FROM_KEY;
	if (msize)
		flags |= CF_TO_MSIZE;
	if (swap)
		flags |= CF_SWAP;

	if (ts->typecode != CDR_TYPECODE_STRUCT ||
	    !cdr_unmarshall_container ((char *) dest - dhsize,
				       (const char *) data - hsize,
			    	       (const CDR_TypeSupport_container *) ts,
			    	       flags, hsize, &doffset, NULL))
		return (DDS_RETCODE_BAD_PARAMETER);
	else {
		prof_stop (cdr_k_get, 1);
		return (DDS_RETCODE_OK);
	}
}

size_t cdr_field_offset (const void            *data,
		         size_t                hsize,
		         unsigned              field,
		         const CDR_TypeSupport *ts,
			 int                   swap,
		         DDS_ReturnCode_t      *error)
{
	unsigned flags;
	struct field_desc_st fdesc;
	size_t doffset = hsize;
	size_t offset;

	prof_start (cdr_f_ofs);

	flags = CF_OFFSET;
	if (swap)
		flags |= CF_SWAP;
	fdesc.depth = 0;
	fdesc.field = field;
	offset = cdr_unmarshall_container (NULL,
				       (const char *) data - hsize,
			    	       (const CDR_TypeSupport_container *) ts,
			    	       flags, hsize, &doffset, (void **) &fdesc);
	if (!offset)
		*error = DDS_RETCODE_BAD_PARAMETER;
	else
		*error = DDS_RETCODE_OK;
	prof_stop (cdr_f_ofs, 1);
	return (offset);
}

void cdr_init (void)
{
	PROF_INIT ("C:MSize", cdr_m_size);
	PROF_INIT ("C:Marshall", cdr_m);
	PROF_INIT ("C:UMSize", cdr_um_size);
	PROF_INIT ("C:UMarshal", cdr_um);
	PROF_INIT ("C:KeySize", cdr_k_size);
	PROF_INIT ("C:KeyGet", cdr_k_get);
	PROF_INIT ("C:FieldOfs", cdr_f_ofs);
}

#ifdef DDS_DEBUG

static const char *cdr_typecode_str [] = {
	NULL, "short", "unsigned short", "long", "unsigned long",
	"long long", "unsigned long long", "float", "double",
#ifdef LONGDOUBLE
	"long double",
#endif
	"?fixed", "boolean", "char", "wchar", "octet", "string", "wstring",
	"struct", "union", "enum", "sequence", "array", "max"
};

void cdr_dump (unsigned indent, CDR_TypeSupport *ts);

void cdr_dump_array (unsigned              indent,
		     CDR_TypeSupport_array *ap,
		     int                   post)
{
	if (ap->container.ts.typecode == CDR_TYPECODE_ARRAY) {
		if (post)
			dbg_printf ("[%u]", ap->container.numelems);
		else
			cdr_dump (indent + 1, &ap->el_ts);
	}
	else if (ap->container.ts.typecode == CDR_TYPECODE_SEQUENCE) {
		dbg_printf ("<");
		cdr_dump (indent + 1, &ap->el_ts);
		dbg_printf (">");
	}
	else {
		dbg_printf ("<");
		if (ap->container.ts.length)
			dbg_printf ("%u", ap->container.ts.length);
		dbg_printf (">");
	}
}

void cdr_dump_struct (unsigned               indent,
		      CDR_TypeSupport_struct *sp)
{
	CDR_TypeSupport_structelem	*sep;
	unsigned			f;
	int				i;

	dbg_printf (" %s", sp->container.ts.name);
	if (sp->container.ts.marker)
		return;

	sp->container.ts.marker = 1;
	dbg_printf (" {\r\n");
	i = 0;
	indent++;
	for (f = 0, sep = sp->elements; f < sp->container.numelems; f++, sep++) {
		dbg_print_indent (indent, NULL);
		if (sp->container.ts.typecode == CDR_TYPECODE_ENUM) {
			dbg_printf ("%s", sep->ts.name);
			if (sep->label != i) {
				dbg_printf (" = %d", sep->label);
				i = sep->label;
			}
			i++;
			if (f < sp->container.numelems - 1)
				dbg_printf (",");
		}
		else {
			cdr_dump (indent, &sep->ts);
			dbg_printf (" %s",  sep->ts.name);
			if (sep->ts.typecode == CDR_TYPECODE_ARRAY)
				cdr_dump_array (indent, (CDR_TypeSupport_array *) sep->ts.ts, 1);

			dbg_printf (";");
			if ((sep->ts.typecode < CDR_TYPECODE_STRUCT ||
			     sep->ts.typecode == CDR_TYPECODE_ARRAY) && sep->ts.key)
				dbg_printf ("  //@key");
		}
		dbg_printf ("\r\n");
	}
	indent--;
	dbg_print_indent (indent, NULL);
	dbg_printf ("}");
}

void cdr_dump (unsigned indent, CDR_TypeSupport *ts)
{
	if (ts->typecode < CDR_TYPECODE_MAX) {
		if (ts->typecode != CDR_TYPECODE_ARRAY)
			dbg_printf ("%s", cdr_typecode_str [ts->typecode]);
	}
	else
		dbg_printf ("?typecode=%d?", ts->typecode);
	if (cdr_is_struct (ts->typecode)) {
		if (ts->ts)
			cdr_dump_struct (indent, (CDR_TypeSupport_struct *) ts->ts);
		else
			cdr_dump_struct (indent, (CDR_TypeSupport_struct *) ts);
	}
	else if (cdr_is_array (ts->typecode) || cdr_is_string (ts->typecode)) {
		if (ts->ts)
			cdr_dump_array (indent, (CDR_TypeSupport_array *) ts->ts, 0);
		else
			cdr_dump_array (indent, (CDR_TypeSupport_array *) ts, 0);
	}
}

void cdr_reset (CDR_TypeSupport *ts)
{
	CDR_TypeSupport_struct		*sp;
	CDR_TypeSupport_array		*ap;
	CDR_TypeSupport_structelem	*ep;
	unsigned			i;

	ts->marker = 0;
	if (cdr_is_struct (ts->typecode)) {
		sp = (CDR_TypeSupport_struct *) ts;
		for (i = 0, ep = sp->elements; i < sp->container.numelems; i++, ep++) {
			ep->ts.marker = 0;
			if (cdr_is_container (ep->ts.typecode))
				cdr_reset (ep->ts.ts);
		}
	}
	else if (cdr_is_array (ts->typecode)) {
		ap = (CDR_TypeSupport_array *) ts;
		if (cdr_is_container (ap->el_ts.typecode))
			cdr_reset (ap->el_ts.ts);
	}
}

void cdr_dump_type (unsigned indent, CDR_TypeSupport *ts)
{
	cdr_reset (ts);
	dbg_print_indent (indent, NULL);
	cdr_dump (indent, ts);
	dbg_printf (";\r\n");
}

#ifdef _WIN32
#define snprintf sprintf_s
#endif

static const char *cdr_enum_label_str (int label, CDR_TypeSupport_struct *sp)
{
	CDR_TypeSupport_structelem	*ep;
	unsigned			i;
	static char			buf [10];

	for (i = 0, ep = sp->elements; i < sp->container.numelems; i++, ep++)
		if (ep->label == label)
			return (ep->ts.name);

	snprintf (buf, sizeof (buf), "?(%d)", label);
	return (buf);
}

static void cdr_dump_struct_data (unsigned               indent,
				  CDR_TypeSupport_struct *sp,
				  const void             *data)
{
	CDR_TypeSupport_structelem	*ep;
	unsigned			i;

	dbg_printf ("{");
	indent++;
	for (i = 0, ep = sp->elements; i < sp->container.numelems; i++, ep++) {
		/*dbg_print_indent (indent, NULL);*/
		if (i)
			dbg_printf (", ");
		dbg_printf ("%s", ep->ts.name);
		if (ep->ts.typecode == CDR_TYPECODE_ARRAY)
			dbg_printf ("[]");
		dbg_printf ("=");
		cdr_dump_data (indent, &ep->ts, (const char *) data + ep->offset);
	}
	indent--;
	/*dbg_print_indent (indent, NULL);*/
	dbg_printf ("}");
}

static void cdr_dump_array_data (unsigned              indent,
				 CDR_TypeSupport_array *ap,
				 const void            *data)
{
	const unsigned char	*dp;
	unsigned		i;

	for (i = 0, dp = (unsigned char *) data;
	     i < ap->container.numelems;
	     i++, dp += ap->el_ts.length) {
		if (i)
			dbg_printf (", ");
		cdr_dump_data (indent, &ap->el_ts, dp);
	}
}

void cdr_dump_data (unsigned indent, CDR_TypeSupport *ts, const void *data)
{
	char		*cp;
	unsigned char	*ucp;
	int16_t		*sp;
	uint16_t	*usp;
	int32_t		*ip;
	uint32_t	*up;
	int64_t		*lip;
	uint64_t	*lup;
	float		*fp;
	double		*dp;
#ifdef LONGDOUBLE
	long double	*ldp;
#endif

	switch (ts->typecode) {
		case CDR_TYPECODE_SHORT:
			sp = (int16_t *) data;
			dbg_printf ("%d", *sp);
			break;
		case CDR_TYPECODE_USHORT:
			usp = (uint16_t *) data;
			dbg_printf ("%u", *usp);
			break;
		case CDR_TYPECODE_LONG:
			ip = (int32_t *) data;
			dbg_printf ("%d", *ip);
			break;
		case CDR_TYPECODE_ULONG:
			up = (uint32_t *) data;
			dbg_printf ("%u", *up);
			break;
		case CDR_TYPECODE_LONGLONG:
			lip = (int64_t *) data;
			dbg_printf ("%" PRId64, *lip);
			break;
		case CDR_TYPECODE_ULONGLONG:
			lup = (uint64_t *) data;
			dbg_printf ("%" PRIu64, *lup);
			break;
		case CDR_TYPECODE_FLOAT:
			fp = (float *) data;
			dbg_printf ("%f", *fp);
			break;
		case CDR_TYPECODE_DOUBLE:
			dp = (double *) data;
			dbg_printf ("%f", *dp);
			break;
#ifdef LONGDOUBLE
		case CDR_TYPECODE_LONGDOUBLE:
			ldp = (long double *) data;
			dbg_printf ("%lf", *ldp);
			break;
#endif
		case CDR_TYPECODE_BOOLEAN:
			ucp = (unsigned char *) data;
			dbg_printf ("%u", *ucp);
			break;
		case CDR_TYPECODE_OCTET:
			ucp = (unsigned char *) data;
			dbg_printf ("0x%x", *ucp);
			break;
		case CDR_TYPECODE_CSTRING:
		case CDR_TYPECODE_WSTRING:
			cp = (char *) data;
			dbg_printf ("\"%s\"", cp);
			break;
		case CDR_TYPECODE_STRUCT:
		case CDR_TYPECODE_UNION:
			if (ts->ts)
				cdr_dump_struct_data (indent, (CDR_TypeSupport_struct *) ts->ts, data);
			else
				cdr_dump_struct_data (indent, (CDR_TypeSupport_struct *) ts, data);
			break;
		case CDR_TYPECODE_ENUM:
			ip = (int32_t *) data;
			dbg_printf ("%s", cdr_enum_label_str (*ip, (CDR_TypeSupport_struct *) ts->ts));
			break;
		case CDR_TYPECODE_SEQUENCE:
		case CDR_TYPECODE_ARRAY:
			if (ts->ts)
				cdr_dump_array_data (indent, (CDR_TypeSupport_array *) ts->ts, data);
			break;
		default:
			break;
	}
}

#endif /* DDS_DEBUG */

