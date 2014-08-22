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
#include "pool.h"
#include "skiplist.h"

/*#define VERBOSE	** Enable to see test in action. */
/*#define SMALL_SAMPLE	** Use a small set of numbers to enable debugging. */
/*#define USE_POOL	** Enable skiplist pools if defined. */

#ifdef SMALL_SAMPLE
#define	SMALLER	% (SAMPLE_SIZE * 10)
#else
#define SMALLER
#endif
#ifdef VERBOSE
#define	SAMPLE_SIZE 15
#else
#define SAMPLE_SIZE 100000
#endif

typedef struct data_node {
	unsigned	key;
	unsigned	*data;
} DATA_NODE;

unsigned long	nins, ndel, nlookup;
unsigned	maxlevel;

int cmp_fct (const void *np, const void *data)
{
	return (((const DATA_NODE *) np)->key - *((const unsigned *) data));
}

#ifdef VERBOSE
int disp_fct (Skiplist_t *l, void *np, void *arg)
{
	printf ("%u ", ((DATA_NODE *) np)->key);
	fflush (stdout);
	return (1);
}
#endif
#ifdef USE_POOL
void sl_pool_display (void)
{
	size_t	sizes [PPT_SIZES];

	print_pool_hdr (0);
	memset (sizes, 0, sizeof (sizes));
	sl_pool_dump (sizes);
	print_pool_end (sizes);
}
#endif

int main (int argc, char **argv)
{
	Skiplist_t	*l;
	DATA_NODE	*v;
	unsigned	i, j, k;
	int		new_node = 0;
	static unsigned	keys [SAMPLE_SIZE];

#ifdef USE_POOL
	sl_pool_init (2, 2, 0, sizeof (DATA_NODE), SAMPLE_SIZE >> 4, ~0, ~0);
#endif

	l = sl_new (sizeof (DATA_NODE));

	printf ("Test skiplists: # of samples = %u\n", SAMPLE_SIZE);
	for (j = 0; j < 4; j++) {

		printf ("Pass %d of %d ...", j + 1, 4);
#ifdef VERBOSE
		printf ("\n");
#else
		fflush (stdout);
#endif
		/* 1. Fill the list with random values. */
		for (k = 0; k < SAMPLE_SIZE; k++) {
			do {
				keys [k] = random () SMALLER;
				v = (DATA_NODE *) sl_insert (l, &keys [k], &new_node, cmp_fct);
				nins++;
			}
			while (!new_node);
			v->key = keys [k];
			v->data = &keys [k];
#ifdef VERBOSE
			printf ("+%u ", keys [k]);
			fflush (stdout);
#endif
		};
#ifdef VERBOSE
		printf ("\n");
		sl_walk (l, disp_fct, NULL);
		printf ("\n");
#endif
		/* 2. Do lots of list search and insert/delete actions. */
		for (i = 0; i < 4; i++) {
#ifdef USE_POOL
	/*		sl_pool_display ();	** Displays intermediate pool results. */
#endif
			/* 2.1. Lookup every list entry. */
			for (k = 0; k < SAMPLE_SIZE; k++) {
				if ((v = (DATA_NODE *) sl_search (l, &keys [k], cmp_fct)) == NULL)
					printf ("error in search #%u,#%u\n", i, k);
				if (v->data != &keys [k])
					printf ("search returned wrong value\n");
				nlookup++;
			};

			/* 2.2. Delete and insert again every list entry. */
			for (k = 0; k < SAMPLE_SIZE; k++) {
#ifdef VERBOSE
				printf ("-%u ", keys [k]);
				fflush (stdout);
#endif
				if (!sl_delete (l, &keys [k], cmp_fct))
					printf ("error in delete\n");
				ndel++;
				do {
					keys [k] = random () SMALLER;
					v = (DATA_NODE*) sl_insert (l, &keys [k], &new_node, cmp_fct);
					nins++;
				}
				while (!new_node);
				v->key = keys [k];
				v->data = &keys [k];
#ifdef VERBOSE
				printf ("+%u ", keys [k]);
				fflush (stdout);
#endif
			};
#ifdef VERBOSE
			printf ("\n");
			sl_walk (l, disp_fct, NULL);
			printf ("\n");
#endif
		};
		if (l->level > maxlevel)
			maxlevel = l->level;

		/* 3. Delete every list entry. */
		for (k = 0; k < SAMPLE_SIZE; k++) {
#ifdef VERBOSE
			printf ("-%u ", keys [k]);
			fflush (stdout);
#endif
			if (!sl_delete (l, &keys [k], cmp_fct))
				printf ("error in delete\n");

			ndel++;
		}
		printf ("done\n");
#ifdef VERBOSE
		sl_walk (l, disp_fct, NULL);
		printf ("\n");
#endif
	}
	sl_free (l);
	printf ("Test completed successfully!\n");
	printf ("Maximum level:               %u.\n", maxlevel);
	printf ("# of list insert operations: %lu\n", nins);
	printf ("# of list delete operations: %lu\n", ndel);
	printf ("# of list lookup operations: %lu\n", nlookup);
	printf ("Total # of list operations:  %lu\n", nins + ndel + nlookup);
#ifdef USE_POOL
	sl_pool_display ();
	sl_pool_free ();
#elif !defined (USE_MALLOC)
	print_alloc_stats ();
#endif

	return (0);
}
