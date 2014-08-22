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

/* str.c -- Implements a string library for efficient storage of strings. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "hash.h"
#include "log.h"
#include "debug.h"
#include "error.h"
#include "pool.h"
#include "heap.h"
#include "str.h"

#define	MAX_HT		97	/* Just some number. */

typedef struct str_hash_st StrHash_t;
struct str_hash_st {
	StrHash_t	*next;
	String_t	*str;
};

enum mem_block_en {
	MB_STRING,		/* String contexts. */
	MB_STR_REF,		/* String references. */
	MB_STR_DATA,		/* String data pool. */

	MB_END
};

static const char *mem_names [] = {
	"STRING",
	"STR_REF",
	"STR_DATA"
};

static MEM_DESC_ST	mem_blocks [MB_END];	/* Memory blocks. */
static size_t		mem_size;		/* Total allocated memory. */
static unsigned		sheap;
static int		str_extend;
static StrHash_t	*str_ht [MAX_HT];
static int		str_init;		/* Str already initialized? */

size_t			str_alloc_max;
size_t			str_allocs;

lock_t			str_lock;

/* str_pool_init -- Initialize the string library.  Strings can be stored up to
		    the given limits.  The string data area size is specified by
		    dsize. */

int str_pool_init (const POOL_LIMITS *str,
		   const POOL_LIMITS *refs,
	           size_t            dsize,
		   int               extend)
{
	int	error;

	/* Check if already initialized. */
	if (mem_blocks [0].md_addr) {	/* Was already initialized -- reset. */
		mds_reset (mem_blocks, MB_END);
		heap_init (mds_block (&mem_blocks [MB_STR_DATA]), dsize, &sheap);
		memset (str_ht, 0, sizeof (str_ht));
		return (STR_OK);
	}

	if (str_init != 0)
		memset (str_ht, 0, sizeof (str_ht));
	
	str_init = 1;

	/* Define the different pool attributes. */
	MDS_POOL_TYPE (mem_blocks, MB_STRING, *str, sizeof (String_t));
	MDS_POOL_TYPE (mem_blocks, MB_STR_REF, *refs, sizeof (StrHash_t));
	MDS_BLOCK_TYPE (mem_blocks, MB_STR_DATA, dsize);

	/* All pools defined: allocate one big chunk of data that will be split in
	   separate pools. */
	mem_size = mds_alloc (mem_blocks, mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!mem_size) {
		fatal_printf ("str_pool_init: not enough memory available!\r\n");
		return (STR_ERR_NOMEM);
	}
	log_printf (STR_ID, 0, "str_pool_init: %lu bytes allocated for pools.\r\n",
							(unsigned long) mem_size);
#endif
	str_extend = extend;
	str_allocs = str_alloc_max = 0;
	error = heap_init (mds_block (&mem_blocks [MB_STR_DATA]), dsize, &sheap);
	if (error)
		return (STR_ERR_NOMEM);

	lock_init_nr (str_lock, "str");
	return (STR_OK);
}

/* str_pool_free -- Release all string pool data. */

void str_pool_free (void)
{

        lock_destroy (str_lock);

	if (str_allocs)
		warn_printf ("str_pool_free: %lu bytes of extra string data was allocated!",
								(unsigned long) str_allocs);
	heap_discard (sheap);
	mds_free (mem_blocks, MB_END);
}

/* str_hash -- Calculate the hash key of a string. */

#define str_hash(sp,l)	hashf((unsigned char *) sp, l)

/* str_lookup -- Lookup a string in the hash table. */

static String_t *str_lookup (unsigned h, const char *s, size_t length)
{
	StrHash_t	*p;

	for (p = str_ht [h]; p; p = p->next)
		if (str_len (p->str) == length &&
		    !memcmp (str_ptr (p->str), s, length))
			return (p->str);

	return (NULL);
}

#define	ALIGN(l)	(((l) + 3) & ~3)
#define	IS_ALIGNED(l)	(((l) & 3) == 0)

static int str_set_data (String_t *sp, const char *s, size_t length, unsigned copy)
{
	char	*cp;

	if (!s)
		length = 0;
	sp->length = length;
	if (length > STRD_SIZE) {
		cp = heap_alloc (sheap, ALIGN (length));
		if (!cp && str_extend) {
			cp = xmalloc (ALIGN (length));
			if (!cp) {
				sp->length = 0;
				return (STR_ERR_NOMEM);
			}
			sp->dynamic = 1;
			str_allocs += length;
			if (str_allocs > str_alloc_max)
				str_alloc_max = str_allocs;
		}
		else if (!cp) {
			sp->length = 0;
			return (STR_ERR_NOMEM);
		}
		sp->u.dp = cp;
	}
	else if (length)
		cp = sp->u.b;
	else {
		sp->u.dp = NULL;
		return (STR_OK);
	}
	memcpy (cp, s, copy);
	while (!IS_ALIGNED (length))
		cp [length++] = '\0';
	return (STR_OK);
}

int str_set_chunk (String_t *sp, const unsigned char *s, size_t ofs, size_t chunk_length)
{
	if (sp->length < ofs + chunk_length)
		return (STR_ERR_BAD);

	if (sp->length > STRD_SIZE) 
		memcpy ((char *) sp->u.dp + ofs, s, chunk_length);
	else
		memcpy (sp->u.b + ofs, s, chunk_length);

	return (STR_OK); 
}

/* str_set -- Set a string to the given parameters. */

static int str_set (String_t *sp, const char *s, size_t length, unsigned copy, int mutable)
{
	sp->users = 1;
	sp->dynamic = 0;
	sp->mutable = mutable;
	if (heap_max (sheap) < length && !str_extend) {
		sp->length = 0;
		sp->u.dp = NULL;
		return (STR_ERR_NOMEM);
	}
	return (str_set_data (sp, s, length, copy));
}

/* str_new -- Allocate a new string containing the given string data. */

String_t *str_new (const char *str, size_t length, size_t copy, int mutable)
{
	String_t	*sp;
	StrHash_t	*shp;
	unsigned	h;
	int		error;

	lock_take (str_lock);

	if (copy > length) 
		copy = length;

	if (!mutable && length == copy) {
		h = str_hash (str, length) % MAX_HT;
		if ((sp = str_lookup (h, str, length)) != NULL) {
			sp->users++;
			lock_release (str_lock);
			return (sp);
		}
	}
	else
		h = 0;	/* Some compilers warn -- h used before init! */

	if ((sp = mds_pool_alloc (&mem_blocks [MB_STRING])) == NULL) {
		warn_printf ("str_new: out of memory!");
		lock_release (str_lock);
		return (NULL);
	}
	error = str_set (sp, str, length, copy, mutable);
	if (error) {
		mds_pool_free (&mem_blocks [MB_STRING], sp);
		lock_release (str_lock);
		return (NULL);
	}
	if (!mutable && length == copy) {

		/* Save the string in the hash table. */
		shp = mds_pool_alloc (&mem_blocks [MB_STR_REF]);
		if (shp) {
			shp->next = str_ht [h];
			shp->str = sp;
			str_ht [h] = shp;
		}
		/* else
			we just don't store the string. */
	}
	lock_release (str_lock);
	return (sp);
}

/* str_replace -- Replace an existing string with the given data. */

int str_replace (String_t *sp, const char *str, size_t length)
{
	int	err;

	if (!sp || !sp->mutable)
		return (STR_ERR_BAD);

	lock_take (str_lock);
	if (!str)
		length = 0;
	if ((!str || length <= STRD_SIZE) && sp->length > STRD_SIZE) {
		if (sp->dynamic) {
			xfree ((char *) sp->u.dp);
			str_allocs -= sp->length;
			sp->dynamic = 0;
		}
		else
			heap_free ((char *) sp->u.dp);
		sp->u.dp = NULL;
		if (length)
			memcpy (sp->u.b, str, length);
		sp->length = length;
	}
	else if (length > STRD_SIZE) {
		if (sp->length <= STRD_SIZE) {
			if ((err = str_set_data (sp, str, length, length)) != STR_OK) {
				lock_release (str_lock);
				return (err);
			}
		}
		else if (length == sp->length)
			memcpy ((char *) sp->u.dp, str, length);
		else if (sp->dynamic)
			sp->u.dp = xrealloc ((char *) sp->u.dp, length);
		else {
			sp->u.dp = heap_realloc ((char *) sp->u.dp, length);
			if (!sp->u.dp && str_extend) {
				heap_free ((char *) sp->u.dp);
				sp->u.dp = xmalloc (length);
				if (sp->u.dp) {
					sp->dynamic = 1;
					str_allocs += length;
					if (str_allocs > str_alloc_max)
						str_alloc_max = str_allocs;
				}
			}
			if (!sp->u.dp) {
				sp->length = 0;
				lock_release (str_lock);
				return (STR_ERR_NOMEM);
			}
			memcpy ((char *) sp->u.dp, str, length);
		}
		sp->length = length;
	}
	else { /* length <= STRD_SIZE */
		if (length)
			memcpy (sp->u.b, str, length);
		sp->length = length;
	}
	lock_release (str_lock);
	return (STR_OK);
}

/* str_ref -- Make a virtual string copy for another user. */

String_t *str_ref (String_t *sp)
{
	if (!sp)
		return (NULL);

	lock_take (str_lock);
	sp->users++;
	lock_release (str_lock);

	return (sp);
}

/* str_unref -- Unreference a user from the string. */

void str_unref (String_t *sp)
{
	unsigned	h;
	StrHash_t	*p, *prev_p;

	lock_take (str_lock);
	if (!sp || --sp->users) {
		lock_release (str_lock);
		return;
	}

	if (!sp->mutable) {
		h = str_hash (str_ptr (sp), sp->length) % MAX_HT;
		for (p = str_ht [h], prev_p = NULL;
		     p;
		     prev_p = p, p = p->next) {
			if (p->str == sp) {
				if (prev_p)
					prev_p->next = p->next;
				else
					str_ht [h] = p->next;
				mds_pool_free (&mem_blocks [MB_STR_REF], p);
				break;
			}
		}
	}
	if (sp->length > STRD_SIZE) {
		if (sp->dynamic) {
			xfree ((char *) sp->u.dp);
			str_allocs -= sp->length;
			sp->dynamic = 0;
		}
		else
			heap_free ((char *) sp->u.dp);
	}
	mds_pool_free (&mem_blocks [MB_STRING], sp);
	lock_release (str_lock);
}

/* str_copy -- Copy a string completely. */

String_t *str_copy (String_t *sp)
{
	if (!sp)
		return (NULL);

	return (str_new (sp->u.dp, sp->length, sp->length, 1));
}


#ifdef DDS_DEBUG

/* str_dump -- Dump the string cache. */

void str_dump (void)
{
	StrHash_t	*p;
	unsigned	h, n, i;
	int		ascii;
	const char	*sp, *cp;

	for (h = 0; h < MAX_HT; h++)
		for (n = 0, p = str_ht [h]; p; p = p->next, n++) {
			if (!n)
				dbg_printf ("\r\n%u: ", h);
			sp = str_ptr (p->str);
			ascii = 1;
			for (i = 0, cp = sp; i < str_len (p->str); i++, cp++) {
				if (i == (unsigned) str_len (p->str) - 1U && 
				    *cp == '\0')
					break;

				if (*cp < ' ' || *cp > '~') {
					ascii = 0;
					break;
				}
			}
			if (ascii)
				dbg_printf ("\'%s\'", sp);
			else
				dbg_print_region (sp, str_len (p->str), 0, 0);
			dbg_printf ("*%u ", p->str->users);
		}
	dbg_printf ("\r\n");
}

/* str_pool_dump -- Dump all string pool statistics. */

void str_pool_dump (size_t sizes [])
{
	print_pool_table (mem_blocks, (unsigned) MB_END, sizes);
}

#endif

