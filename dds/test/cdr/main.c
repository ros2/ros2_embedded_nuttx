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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dds/dds_types.h"
#include "dds/dds_xtypes.h"
#include "typecode.h"
#ifdef DDS_TYPECODE
#include "vtc.h"
#endif
#include "xcdr.h"
#include "xdata.h"
#include "pool.h"
#include "test.h"

extern void test_structs (void);
extern void test_endianness (void);
extern void test_array (void);
extern void test_sequences (void);
extern void test_cstrings (void);
extern void test_unions (void);
extern void test_keys (void);
extern void test_dyn_structs (void);
extern void test_dyn_arrays (void);
extern void test_dyn_mutable (void);
extern void test_dyn_maps (void);
extern void test_holder (void);

const char	*progname;
int		introspect;
int		dump_type;
int		dump_data;
int		parse_data;
int		dump_typecode;
int		verbose;

typedef struct test_st {
	const char	*name;
	void		(*fct) (void);
	const char	*info;
} Test_t;

const Test_t tests [] = {
	{ "struct", test_structs, "Static Structure types." },
	{ "endian", test_endianness, "Big- and little-endian marshalling." },
	{ "array", test_array, "Static Array types." },
	{ "seq", test_sequences, "Static Sequence types." },
	{ "string", test_cstrings, "Static String types." },
	{ "union", test_unions, "Static Union types." },
	{ "key", test_keys, "Static key fields tests." },
	{ "dstruct", test_dyn_structs,   "Dynamic Structure types." },
	{ "darray", test_dyn_arrays, "Dynamic Array types." },
	{ "mutable", test_dyn_mutable, "Mutable types." },
	{ "holder", test_holder, "DataHolder type." },
//	{ "map", test_dyn_maps, "Map types." },
};

void list_tests (void)
{
	const Test_t	*tp;
	unsigned	i;

	for (i = 0, tp = tests; i < sizeof (tests) / sizeof (Test_t); tp++, i++)
		printf ("\t%-12s\t%s\r\n", tp->name, tp->info);
}

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "%s -- test program for the DDS Typecode API.\r\n", progname);
	fprintf (stderr, "Usage: %s [switches] {<testname>}\r\n", progname);
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -l         List all test names.\r\n");
	fprintf (stderr, "   -i         Introspect types.\r\n");
	fprintf (stderr, "   -t         Dump types.\r\n");
	fprintf (stderr, "   -d         Dump data.\r\n");
	fprintf (stderr, "   -p         Parse data.\r\n");
	fprintf (stderr, "   -y         Dump vendor typecode.\r\n");
	fprintf (stderr, "   -v         Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv        Very verbose (same as -itdyv).\r\n");
	fprintf (stderr, "   -h         Display help info.\r\n");
	exit (1);
}

/* get_num -- Get a number from the command line arguments. */

int get_num (const char **cpp, unsigned *num, unsigned min, unsigned max)
{
	const char	*cp = *cpp;

	while (isspace ((unsigned char) *cp))
		cp++;
	if (*cp < '0' || *cp > '9')
		return (0);

	*num = (unsigned) atoi (cp);
	if (*num < min || *num > max)
		return (0);

	while (*cp)
		cp++;

	*cpp = cp;
	return (1);
}

#define	INC_ARG()	if (!*cp) { i++; cp = argv [i]; }

/* do_switches -- Command line switch decoder. */

int do_switches (int argc, const char **argv)
{
	int		i, help;
	const char	*cp;

	progname = argv [0];
	help = 0;
	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
				case 'l':
					list_tests ();
					exit (0);
					break;
				case 'i':
					introspect = 1;
					break;
				case 't':
					dump_type = 1;
					break;
				case 'd':
					dump_data = 1;
					break;
				case 'y':
					dump_typecode = 1;
					break;
				case 'v':
					verbose = 1;
					if (*cp == 'v') {
						introspect = 1;
						dump_type = 1;
						dump_data = 1;
						parse_data = 1;
						dump_typecode = 1;
						cp++;
					}
					break;
				case 'h':
					help = 1;
				default:
					if (!help)
						fprintf (stderr, "Unknown option!\r\n");
					usage ();
				break;
			}
		}
	}
	return (i);
}

void run_test (const Test_t *tp)
{
	(*tp->fct) ();
}

void run_all (void)
{
	const Test_t	*tp;
	unsigned	i;

	for (i = 0, tp = tests; i < sizeof (tests) / sizeof (Test_t); i++, tp++)
		run_test (tp);
}

void run_named (const char *name)
{
	const Test_t	*tp;
	unsigned	i;

	for (i = 0, tp = tests; i < sizeof (tests) / sizeof (Test_t); i++, tp++)
		if (!strcmp (name, tp->name)) {
			run_test (tp);
			return;
		}

	printf ("No such test!\r\n");
	exit (1);
}

int main (int argc, const char *argv [])
{
	int	n;
	const POOL_LIMITS str = {
		2, ~0, 0
	}, refs = {
		2, ~0, 0
	}, dtypes = {
		2, ~0, 0
	}, ddata = {
		2, ~0, 0
	};

	n = do_switches (argc, argv);
	fail_unless (str_pool_init (&str, &refs, 20000, 1) == 0);
	fail_unless (dds_typesupport_init () == 0);
	fail_unless (xd_pool_init (&dtypes, &ddata) == 0);

	if (n == argc)
		run_all ();
	else
		while (n < argc)
			run_named (argv [n++]);

	xd_pool_final ();
	dds_typesupport_final ();
	str_pool_free ();
	return (0);
}

void rcl_access (void *p)
{
}

void rcl_done (void *p)
{
}

void pl_cache_reset (void)
{
}

void create_file (const char *prefix, const char *name, const void *p, size_t len)
{
	char buf [128];
	FILE *f;

	strcpy (buf, prefix);
	strcat (buf, "_");
	strcat (buf, name);
	f = fopen (buf, "w");
	fwrite (p, 1, len, f);
	fclose (f);
}

void dump_result (const char *prefix, const void *src, const void *dst, size_t len, const void *cdr, size_t clen)
{
	create_file (prefix, "src", src, len);
	create_file (prefix, "dst", dst, len);
	create_file (prefix, "cdr", cdr, clen);
}

#ifdef DUMP_DATA

void dump_seq (const char *name, const void *seq)
{
	const DDS_VoidSeq	*vseq = (DDS_VoidSeq *) seq;
	const unsigned char	*p;
	unsigned		i, j;

	dbg_printf ("sequence(%s)<", name);
	for (i = 0, p = vseq->_buffer; i < vseq->_length; i++, p += vseq->_esize) {
		dbg_printf ("\r\n\t%u:", i);
		for (j = 0; j < vseq->_esize; j++)
			dbg_printf (" %02x", p [j]);
	}
	dbg_printf (">\r\n");
}

#endif

void verify_result (const void *src, const void *dst, size_t len,
		    const void *cdr, size_t clen)
{
	if (memcmp (src, dst, len)) {
		printf (" -- failed - dumping data -- \r\n");
		dump_result ("dump", src, dst, len, cdr, clen);
		exit (1);
	}
}

void verify_key (const void *k1, size_t klen1, const void *k2, size_t klen2)
{
	if (klen1 != klen2 || memcmp (k1, k2, klen1)) {
		printf (" -- failed (key functions) - dumping data -- \r\n");
		create_file ("dump", "key1", k1, klen1);
		create_file ("dump", "key2", k2, klen2);
	}
}

void dump_region (const void *sample, size_t length)
{
	dbg_print_region (sample, length, 1, 1);
}

void introspect_annotation (DDS_AnnotationDescriptor *adp, int after)
{
	unsigned		i;
	DDS_ReturnCode_t	ret;
	DDS_TypeDescriptor	desc;
	DDS_Parameters		pars;
	MapEntry_DDS_ObjectName_DDS_ObjectName *p;

	if (after)
		dbg_printf ("  //");

	DDS_TypeDescriptor__init (&desc);
	ret = DDS_DynamicType_get_descriptor (adp->type, &desc);
	fail_unless (ret == DDS_RETCODE_OK);

	dbg_printf ("@%s", desc.name);
	DDS_SEQ_INIT (pars);
	ret = DDS_AnnotationDescriptor_get_all_value (adp, &pars);
	fail_unless (ret == DDS_RETCODE_OK);

	if (DDS_SEQ_LENGTH (pars) == 1 &&
	    !strcmp (DDS_SEQ_ITEM (pars, 0).key, "value")) { /* Shorthand notation! */
		if (strcmp (DDS_SEQ_ITEM (pars, 0).value, "true"))
			dbg_printf ("(%s)", DDS_SEQ_ITEM (pars, 0).value);
	}
	else {
		dbg_printf ("(");
		for (i = 0; i < DDS_SEQ_LENGTH (pars); i++) {
			if (i)
				dbg_printf (", ");
			p = DDS_SEQ_ITEM_PTR (pars, i);
			dbg_printf ("%s=\"%s\"", p->key, p->value);
		}
		dbg_printf (")");
	}
	if (!after)
		dbg_printf (" ");

	DDS_Parameters__clear (&pars);
	DDS_TypeDescriptor__clear (&desc);
}

#define	INDENT(n, i) for (i = 0; i < n; i++) dbg_printf ("    ")

void introspect_type (unsigned indent, const DDS_DynamicType t);

void introspect_enum_type (unsigned                 indent,
		           const DDS_DynamicType    t,
		           const DDS_TypeDescriptor *dp)
{
	unsigned		i, j, n;
	DDS_ReturnCode_t	ret;
	DDS_MemberDescriptor	md;
	DDS_AnnotationDescriptor ad;
	DDS_DynamicTypeMembersById members;
	MapEntry_DDS_MemberId_DDS_DynamicTypeMember *p;

	dbg_printf ("enum %s {\r\n", dp->name);
	indent++;
	DDS_MemberDescriptor__init (&md);
	ret = DDS_DynamicType_get_all_members (t, &members);
	fail_unless (ret == DDS_RETCODE_OK);

	DDS_SEQ_FOREACH_ENTRY (members, i, p) {
		INDENT (indent, j);
		if ((n = DDS_DynamicTypeMember_get_annotation_count (p->value)) != 0) {
			DDS_AnnotationDescriptor__init (&ad);
			for (i = 0; i < n; i++) {
				ret = DDS_DynamicType_get_annotation (t, &ad, i);
				fail_unless (ret == DDS_RETCODE_OK);

				introspect_annotation (&ad, 0);
				DDS_AnnotationDescriptor__clear (&ad);
			}
		}
		ret = DDS_DynamicTypeMember_get_descriptor (p->value, &md);
		fail_unless (ret == DDS_RETCODE_OK);

		dbg_printf ("%s", md.name);
		if (i + 1 < DDS_SEQ_LENGTH (members))
			dbg_printf (",");
		dbg_printf ("\r\n");
	}
	indent--;
	INDENT (indent, i);
	dbg_printf ("}");
	DDS_DynamicTypeMembersById__clear (&members);
	DDS_MemberDescriptor__clear (&md);
}

void introspect_alias_type (unsigned indent, const DDS_TypeDescriptor *dp)
{
	dbg_printf ("typedef ");
	introspect_type (indent + 1, dp->base_type);
	dbg_printf (" %s", dp->name);
}

void introspect_array_type (unsigned                 indent, 
			    const DDS_TypeDescriptor *dp,
			    int                      post)
{
	unsigned		i;
	uint32_t		*dimp;

	if (post) {
		DDS_SEQ_FOREACH_ENTRY (dp->bound, i, dimp)
			dbg_printf ("[%u]", *dimp);
	}
	else
		introspect_type (indent, dp->element_type);
}

void introspect_sequence_type (unsigned indent, const DDS_TypeDescriptor *dp)
{
	dbg_printf ("sequence<");
	introspect_type (indent, dp->element_type);
	dbg_printf (">");
	if (DDS_SEQ_LENGTH (dp->bound) == 1 && DDS_SEQ_ITEM (dp->bound, 0))
		dbg_printf ("<%u>", DDS_SEQ_ITEM (dp->bound, 0));
}

void introspect_string_type (const DDS_TypeDescriptor *dp)
{
	DDS_ReturnCode_t	ret;
	DDS_TypeDescriptor	desc;

	DDS_TypeDescriptor__init (&desc);
	ret = DDS_DynamicType_get_descriptor (dp->element_type, &desc);
	fail_unless (ret == DDS_RETCODE_OK);

	if (!strcmp (desc.name, "Char8"))
		dbg_printf ("string");
	else
		dbg_printf ("wstring");
	if (DDS_SEQ_LENGTH (dp->bound) == 1 && DDS_SEQ_ITEM (dp->bound, 0))
		dbg_printf ("<%u>", DDS_SEQ_ITEM (dp->bound, 0));
	DDS_TypeDescriptor__clear (&desc);
}

void introspect_map_type (unsigned indent, const DDS_TypeDescriptor *dp)
{
	dbg_printf ("map<");
	introspect_type (indent, dp->key_element_type);
	dbg_printf (", ");
	introspect_type (indent + 1, dp->element_type);
	if (DDS_SEQ_LENGTH (dp->bound) == 1 && DDS_SEQ_ITEM (dp->bound, 0))
		dbg_printf (", %u", DDS_SEQ_ITEM (dp->bound, 0));
	dbg_printf (">");
}

void introspect_union_type (unsigned                 indent, 
		            const DDS_DynamicType    t,
		            const DDS_TypeDescriptor *dp)
{
	unsigned		i, j, n;
	int32_t			*lp;
	DDS_ReturnCode_t	ret;
	DDS_TypeDescriptor	desc;
	DDS_MemberDescriptor	mdesc;
	DDS_AnnotationDescriptor ad;
	DDS_DynamicTypeMembersByName members;
	MapEntry_DDS_ObjectName_DDS_DynamicTypeMember *p;

	dbg_printf ("union %s ", dp->name);
	dbg_printf ("switch (");
	introspect_type (indent, dp->discriminator_type);
	dbg_printf (") {\r\n");
	indent++;
	DDS_TypeDescriptor__init (&desc);
	DDS_MemberDescriptor__init (&mdesc);
	ret = DDS_DynamicType_get_all_members_by_name (t, &members);
	fail_unless (ret == DDS_RETCODE_OK);

	DDS_SEQ_FOREACH_ENTRY (members, i, p) {
		ret = DDS_DynamicTypeMember_get_descriptor (p->value, &mdesc);
		fail_unless (ret == DDS_RETCODE_OK);

		INDENT (indent - 1, n);
		if (mdesc.default_label)
			dbg_printf ("  default:\r\n");
		else if (DDS_SEQ_LENGTH (mdesc.label) == 1)
			dbg_printf ("  case %d:\r\n", DDS_SEQ_ITEM (mdesc.label, 0));
		else
			DDS_SEQ_FOREACH_ENTRY (mdesc.label, j, lp) {
				dbg_printf ("  case %d:\r\n", *lp);
				if (j + 1 < DDS_SEQ_LENGTH (mdesc.label))
					INDENT (indent - 1, n);
			}
		INDENT (indent, n);
		introspect_type (indent, mdesc.type);
		dbg_printf (" %s", mdesc.name);
		
		ret = DDS_DynamicType_get_descriptor (mdesc.type, &desc);
		fail_unless (ret == DDS_RETCODE_OK);

		if (desc.kind == DDS_ARRAY_TYPE)
			introspect_array_type (indent, &desc, 1);
		dbg_printf (";");

		if ((n = DDS_DynamicTypeMember_get_annotation_count (p->value)) != 0) {
			DDS_AnnotationDescriptor__init (&ad);
			for (j = 0; j < n; j++) {
				ret = DDS_DynamicTypeMember_get_annotation (p->value, &ad, j);
				fail_unless (ret == DDS_RETCODE_OK);

				introspect_annotation (&ad, 1);
				DDS_AnnotationDescriptor__clear (&ad);
			}
		}
		dbg_printf ("\r\n");
	}
	indent--;
	INDENT (indent, i);
	dbg_printf ("}");
	DDS_DynamicTypeMembersByName__clear (&members);
	DDS_TypeDescriptor__clear (&desc);
	DDS_MemberDescriptor__clear (&mdesc);
}

void introspect_struct_type (unsigned                 indent, 
		             const DDS_DynamicType    t,
		             const DDS_TypeDescriptor *dp)
{
	unsigned		i, j, n;
	DDS_ReturnCode_t	ret;
	DDS_TypeDescriptor	desc;
	DDS_MemberDescriptor	mdesc;
	DDS_AnnotationDescriptor ad;
	DDS_DynamicTypeMembersById members;
	MapEntry_DDS_MemberId_DDS_DynamicTypeMember *p;

	dbg_printf ("struct %s", dp->name);
	if (dp->base_type) {
		ret = DDS_DynamicType_get_descriptor (dp->base_type, &desc);
		fail_unless (ret == DDS_RETCODE_OK);

		dbg_printf (": %s", desc.name);
	}
	dbg_printf (" {\r\n");
	indent++;
	DDS_TypeDescriptor__init (&desc);
	DDS_MemberDescriptor__init (&mdesc);
	ret = DDS_DynamicType_get_all_members (t, &members);
	fail_unless (ret == DDS_RETCODE_OK);

	DDS_SEQ_FOREACH_ENTRY (members, i, p) {
		ret = DDS_DynamicTypeMember_get_descriptor (p->value, &mdesc);
		fail_unless (ret == DDS_RETCODE_OK);

		INDENT (indent, n);
		introspect_type (indent, mdesc.type);
		dbg_printf (" %s", mdesc.name);

		ret = DDS_DynamicType_get_descriptor (mdesc.type, &desc);
		fail_unless (ret == DDS_RETCODE_OK);

		if (desc.kind == DDS_ARRAY_TYPE)
			introspect_array_type (indent, &desc, 1);
		dbg_printf (";");

		if ((n = DDS_DynamicTypeMember_get_annotation_count (p->value)) != 0) {
			DDS_AnnotationDescriptor__init (&ad);
			for (j = 0; j < n; j++) {
				ret = DDS_DynamicTypeMember_get_annotation (p->value, &ad, j);
				fail_unless (ret == DDS_RETCODE_OK);

				introspect_annotation (&ad, 1);
				DDS_AnnotationDescriptor__clear (&ad);
			}
		}
		dbg_printf ("\r\n");
	}
	indent--;
	INDENT (indent, i);
	dbg_printf ("}");
	DDS_DynamicTypeMembersById__clear (&members);
	DDS_TypeDescriptor__clear (&desc);
	DDS_MemberDescriptor__clear (&mdesc);
}

void introspect_annotation_type (unsigned                 indent, 
		                 const DDS_DynamicType    t,
		                 const DDS_TypeDescriptor *dp)
{
	unsigned		i, j;
	DDS_ReturnCode_t	ret;
	DDS_TypeDescriptor	desc;
	DDS_MemberDescriptor	mdesc;
	DDS_DynamicTypeMembersById members;
	MapEntry_DDS_MemberId_DDS_DynamicTypeMember *p;

	dbg_printf ("@Annotation\r\n");
	INDENT (indent, i);
	dbg_printf ("local interface %s {\r\n", dp->name);
	indent++;
	DDS_TypeDescriptor__init (&desc);
	DDS_MemberDescriptor__init (&mdesc);
	ret = DDS_DynamicType_get_all_members (t, &members);
	fail_unless (ret == DDS_RETCODE_OK);

	DDS_SEQ_FOREACH_ENTRY (members, i, p) {
		ret = DDS_DynamicTypeMember_get_descriptor (p->value, &mdesc);
		fail_unless (ret == DDS_RETCODE_OK);

		INDENT (indent, j);
		dbg_printf ("attribute ");

		ret = DDS_DynamicType_get_descriptor (mdesc.type, &desc);
		fail_unless (ret == DDS_RETCODE_OK);

		dbg_printf ("%s %s", desc.name, mdesc.name);
		if (desc.kind == DDS_ARRAY_TYPE)
			introspect_array_type (indent, &desc, 1);
		if (mdesc.default_value) {
			dbg_printf (" default ");
			if (desc.kind == DDS_STRING_TYPE)
				dbg_printf ("\"%s\"", mdesc.default_value);
			else if (desc.kind == DDS_CHAR_8_TYPE)
				dbg_printf ("\'%s\'", mdesc.default_value);
			else
				dbg_printf ("%s", mdesc.default_value);
		}
		dbg_printf (";\r\n");
	}
	indent--;
	INDENT (indent, i);
	dbg_printf ("}");
	DDS_DynamicTypeMembersById__clear (&members);
	DDS_MemberDescriptor__clear (&mdesc);
	DDS_TypeDescriptor__clear (&desc);
}

void introspect_type (unsigned indent, const DDS_DynamicType t)
{
	DDS_ReturnCode_t	ret;
	unsigned		i, n;
	DDS_TypeDescriptor	desc;
	DDS_AnnotationDescriptor ad;
	static const char	*type_names [] = {
		NULL,
		"boolean", "octet",
		"short", "unsigned short", "long", "unsigned long", "long long",
		"unsigned long long", "float", "double", "long double",
		"char", "wchar"
	};

	DDS_TypeDescriptor__init (&desc);
	ret = DDS_DynamicType_get_descriptor (t, &desc);
	fail_unless (ret == DDS_RETCODE_OK);

	if (!indent && desc.kind >= DDS_ARRAY_TYPE && desc.kind <= DDS_MAP_TYPE)
		dbg_printf ("typedef ");

	if ((n = DDS_DynamicType_get_annotation_count (t)) != 0) {
		DDS_AnnotationDescriptor__init (&ad);
		for (i = 0; i < n; i++) {
			ret = DDS_DynamicType_get_annotation (t, &ad, i);
			fail_unless (ret == DDS_RETCODE_OK);

			introspect_annotation (&ad, 0);
			DDS_AnnotationDescriptor__clear (&ad);
		}
	}
	switch (desc.kind) {
		case DDS_ENUMERATION_TYPE:
		case DDS_BITSET_TYPE:
			introspect_enum_type (indent, t, &desc);
			break;
		case DDS_ALIAS_TYPE:
			introspect_alias_type (indent, &desc);
			break;
		case DDS_ARRAY_TYPE:
			introspect_array_type (indent, &desc, 0);
			break;
		case DDS_SEQUENCE_TYPE:
			introspect_sequence_type (indent, &desc);
			break;
		case DDS_STRING_TYPE:
			introspect_string_type (&desc);
			break;
		case DDS_MAP_TYPE:
			introspect_map_type (indent, &desc);
			break;
		case DDS_UNION_TYPE:
			introspect_union_type (indent, t, &desc);
			break;
		case DDS_STRUCTURE_TYPE:
			introspect_struct_type (indent, t, &desc);
			break;
		case DDS_ANNOTATION_TYPE:
			introspect_annotation_type (indent, t, &desc);
			break;
		default:
			dbg_printf ("%s", type_names [desc.kind]);
			break;
	}
	if (!indent && desc.kind >= DDS_ARRAY_TYPE && desc.kind <= DDS_MAP_TYPE) {
		dbg_printf (" %s", desc.name);
		if (desc.kind == DDS_ARRAY_TYPE)
			introspect_array_type (indent, &desc, 1);
	}
	DDS_TypeDescriptor__clear (&desc);
}

void marshallUnmarshall (const void *sample, void **sample_out,
			 DDS_TypeSupport ts, int verify)
{
	size_t len, clen, klen, klen2;
	DDS_ReturnCode_t err;
	DBW out;
	unsigned char *bp, *key, *key2;
	unsigned char	h [16], h2 [16];
#ifdef DDS_TYPECODE
	unsigned char *tc;
#endif
#ifdef DUMP_DATA
	StructureType *stp;
	size_t dlen;
#endif
#ifdef INTROSPECTION
	DDS_DynamicType dt;
#endif
#ifdef COPY_DATA
	const void *old_sample = sample;

	sample = DDS_TypeSupport_data_copy (ts, old_sample);
	fail_unless (sample != NULL &&
		     DDS_TypeSupport_data_equals (ts, sample, old_sample));
#endif

#ifdef DUMP_TYPE
	if (dump_type)
		DDS_TypeSupport_dump_type (0, (TypeSupport_t *) ts, 15);
#endif
#ifdef INTROSPECTION
	if (introspect && ts->ts_prefer <= MODE_PL_CDR) {
		dt = DDS_DynamicTypeSupport_get_type ((DDS_DynamicTypeSupport) ts);
		fail_unless (dt != NULL);

		dbg_printf ("Introspection:\r\n");
		dbg_printf ("    ");
		introspect_type (1, dt);
		dbg_printf (";\r\n");
		DDS_DynamicTypeBuilderFactory_delete_type (dt);
	}
#endif
	clen = DDS_MarshalledDataSize (sample, 0, ts, &err);
	fail_unless (clen != 0 && err == DDS_RETCODE_OK);

	out.data = bp = malloc ((clen + 3) & ~3);
	out.length = clen;
	out.left = clen;
	fail_unless (bp != NULL);

#ifdef INIT_DATA
	memset (bp, 0xdf, (clen + 3) & ~3);
#endif
#ifdef DUMP_DATA
	if (dump_data) {
		dbg_printf ("Sample:\r\n");
		if (ts->ts_prefer == MODE_CDR)
			stp = (StructureType *) ts->ts_cdr;
		else
			stp = (StructureType *) ts->ts_pl->xtype;
		dlen = stp->size;
		dump_region (sample, dlen);
#ifdef PARSE_DATA
		if (parse_data) {
			dbg_printf ("\t");
			DDS_TypeSupport_dump_data (1, ts, sample, 1, 0, 1);
			dbg_printf ("\r\n");
		}
#endif
	}
#endif
	err = DDS_MarshallData (out.data, sample, 0, ts);
	fail_unless (0 == err);
#ifdef DUMP_DATA
	if (dump_data) {
		dbg_printf ("Marshalled:\r\n");
		dump_region (out.data, clen);
#ifdef PARSE_DATA
		if (parse_data) {
			dbg_printf ("\t");
			DDS_TypeSupport_dump_data (1, ts, out.data, 0, 0, 1);
			dbg_printf ("\r\n");
		}
#endif
	}
#endif
	len = DDS_UnmarshalledDataSize (out, ts, &err);
	fail_unless (err == DDS_RETCODE_OK);
	*sample_out = (void *) xmalloc ((len + 3) & ~3);
#ifndef DUMP_DATA
	if (verify)
#endif
		memset (*sample_out, 0, len);
	err = DDS_UnmarshallData (*sample_out, &out, ts);
	fail_unless (0 == err);
#ifdef DUMP_DATA
	if (dump_data) {
		dbg_printf ("Unmarshalled:\r\n");
		dump_region (*sample_out, len);
#ifdef PARSE_DATA
		if (parse_data) {
			dbg_printf ("\t");
			DDS_TypeSupport_dump_data (1, ts, *sample_out, 1, 0, 1);
			dbg_printf ("\r\n");
		}
#endif
	}
#endif
	if (verify)
		verify_result (sample, *sample_out, len, out.data, clen);
	if (ts->ts_keys) {
#ifdef DUMP_DATA
		if (dump_data) {
#ifdef PARSE_DATA
			if (parse_data) {
				dbg_printf ("Native key fields:\r\n\t");
				DDS_TypeSupport_dump_key (1, ts, sample, 1, 0, 1);
				dbg_printf ("\r\n");
			}
#endif
		}
#endif
		klen = DDS_KeySizeFromNativeData (sample, 0, ts, &err);
		fail_unless (klen != 0 && err == DDS_RETCODE_OK);
		key = malloc ((klen + 3) & ~3);
		fail_unless (key != NULL);
		err = DDS_KeyFromNativeData (key, sample, 0, ts);
		fail_unless (err == DDS_RETCODE_OK);
#ifdef DUMP_DATA
		if (dump_data) {
			dbg_printf ("Key from native:\r\n");
			dump_region (key, klen);
#ifdef PARSE_DATA
			if (parse_data) {
				dbg_printf ("\t");
				DDS_TypeSupport_dump_key (1, ts, key, 0, 0, 1);
				dbg_printf ("\r\n");
			}
#endif
		}
#endif

		out.data = bp;
		out.length = clen;
		out.left = clen;
		klen2 = DDS_KeySizeFromMarshalled (out, ts, 0, &err);
		fail_unless (klen2 != 0 && err == DDS_RETCODE_OK);
		key2 = malloc ((klen2 + 3) & ~3);
		fail_unless (key2 != NULL);
		err = DDS_KeyFromMarshalled (key2, out, ts, 0);
		fail_unless (err == DDS_RETCODE_OK);
#ifdef DUMP_DATA
		if (dump_data) {
			dbg_printf ("Key from marshalled:\r\n");
			dump_region (key2, klen2);
#ifdef PARSE_DATA
			if (parse_data) {
				dbg_printf ("\t");
				DDS_TypeSupport_dump_key (1, ts, key2, 0, 0, 1);
				dbg_printf ("\r\n");
			}
#endif
		}
#endif
		if (verify)
			verify_key (key, klen, key2, klen2);

		err = DDS_HashFromKey (h, key, klen, ts);
		fail_unless (err == DDS_RETCODE_OK);
#ifdef DUMP_DATA
		if (dump_data) {
			dbg_printf ("Hash from Key:\r\n");
			dump_region (h, 16);
		}
#endif
		err = DDS_HashFromKey (h2, key2, klen2, ts);
		fail_unless (err == DDS_RETCODE_OK);

		fail_unless (!memcmp (h, h2, 16));
		free (key);
		free (key2);
	}
	free (bp);
#ifdef COPY_DATA
	DDS_TypeSupport_data_free (ts, (void *) sample, 1);
#endif
#ifdef DUMP_TYPECODE
#ifdef DDS_TYPECODE
	if (ts->ts_prefer != MODE_CDR)
		return;

	tc = vtc_create (ts);
	fail_unless (tc != NULL);
#ifdef DUMP_DATA
	if (dump_typecode) {
		dbg_printf ("Typecode:\r\n");
		dlen = *((uint16_t *) (tc + 4)) + 6;
		dump_region (tc, dlen);
	}
#endif
	vtc_free (tc);
#endif
#endif
}

void marshallDynamic (const DDS_DynamicData dd, DDS_DynamicData *dd_out,
						 DDS_DynamicTypeSupport ts)
{
	unsigned char	*out, *key, *key2;
	int		swap;
	size_t		len, klen, klen2;
	DynDataRef_t	*ddr, *ddr_out;
	DynData_t	*ddo;
	DBW		ow;
	Type		*tp;
	DDS_ReturnCode_t rc;
	TypeSupport_t	*nts = (TypeSupport_t *) ts;
	unsigned char	h [16], h2 [16];
#ifdef INTROSPECTION
	DDS_DynamicType dt;
#endif
#ifdef DDS_TYPECODE
	unsigned char *tc;
#endif

#ifdef DUMP_TYPE
	if (dump_type)
		DDS_TypeSupport_dump_type (0, nts, XDF_ALL);
#endif
#ifdef INTROSPECTION
	if (introspect && nts->ts_prefer <= MODE_PL_CDR) {
		dt = DDS_DynamicTypeSupport_get_type (ts);
		fail_unless (dt != NULL);

		dbg_printf ("Introspection:\r\n");
		dbg_printf ("    ");
		introspect_type (1, dt);
		dbg_printf (";\r\n");
		DDS_DynamicTypeBuilderFactory_delete_type (dt);
	}
#endif
	ddr = (DynDataRef_t *) dd;
	fail_unless (nts->ts_prefer == MODE_CDR || nts->ts_prefer == MODE_PL_CDR);
	if (nts->ts_prefer == MODE_CDR)
		tp = nts->ts_cdr;
	else /*if (ts->ts_prefer == MODE_PL_CDR)*/
		tp = nts->ts_pl->xtype;
#if defined (DUMP_DATA) && defined (PARSE_DATA)
	if (dump_data && parse_data) {
		dbg_printf ("Sample:\r\n");
		dbg_printf ("\t");
		DDS_TypeSupport_dump_data (1, nts, ddr->ddata, 1, 1, 1);
		dbg_printf ("\r\n");
		dbg_printf ("Raw data:\r\n");
		xd_dump (1, ddr->ddata);
	}
#endif
	len = cdr_marshalled_size (4, ddr->ddata, tp, 1, 0, 0, NULL) + 4;
	fail_unless (len != 0);

	out = malloc (((len + 3) & ~3) + 4);
	fail_unless (out != NULL);

#ifdef INIT_DATA
	memset (out, 0xdf, ((len + 3) & ~3) + 4);
#endif
	for (swap = 0; swap <= 1; swap++) {
		len = cdr_marshall (out + 4, 4, ddr->ddata, tp, 1, 0, 0, swap, &rc);
		fail_unless (len && rc == DDS_RETCODE_OK);

		out [0] = out [2] = out [3] = 0;
		out [1] = (nts->ts_prefer << 1) | (ENDIAN_CPU ^ swap);
#ifdef DUMP_DATA
		if (dump_data) {
			dbg_printf ("\r\nMarshalled(swap=%d):\r\n", swap);
			dump_region (out, len);
#ifdef PARSE_DATA
			if (parse_data) {
				dbg_printf ("\t");
				DDS_TypeSupport_dump_data (1, nts, out, 0, 0, 1);
				dbg_printf ("\r\n");
			}
#endif
		}
#endif
		ddo = cdr_dynamic_data (out + 4, 4, tp, 0, 0, swap);
		fail_unless (ddo != NULL);

#if defined (DUMP_DATA) && defined (PARSE_DATA)
		if (dump_data && parse_data) {
			dbg_printf ("Unmarshalled:\r\n");
			dbg_printf ("\t");
			DDS_TypeSupport_dump_data (1, nts, ddo, 1, 1, 1);
			dbg_printf ("\r\n");
			dbg_printf ("Resulting data:\r\n");
			xd_dump (1, ddo);
		}
#endif
		ddr_out = xd_dyn_dref_alloc ();
		fail_unless (ddr_out != NULL);

		ddr_out->magic = DD_MAGIC;
		ddr_out->nrefs = 1;
		ddr_out->ddata = ddo;
		*dd_out = (DDS_DynamicData) ddr_out;
		fail_unless (DDS_DynamicData_equals (dd, *dd_out));

		if (nts->ts_keys) {
#ifdef DUMP_DATA
#ifdef PARSE_DATA
			if (dump_data && parse_data) {
				dbg_printf ("Native key fields:\r\n\t");
				DDS_TypeSupport_dump_key (1, nts, ddr->ddata, 1, 1, 1);
				dbg_printf ("\r\n");
			}
#endif
#endif
			klen = DDS_KeySizeFromNativeData ((unsigned char *) ddr->ddata, 1, nts, &rc);
			fail_unless (klen != 0 && rc == DDS_RETCODE_OK);
			key = malloc ((klen + 3) & ~3);
			fail_unless (key != NULL);
			rc = DDS_KeyFromNativeData (key, ddr->ddata, 1, nts);
			fail_unless (rc == DDS_RETCODE_OK);
#ifdef DUMP_DATA
			if (dump_data) {
				dbg_printf ("Key from native:\r\n");
				dump_region (key, klen);
#ifdef PARSE_DATA
				if (parse_data) {
					dbg_printf ("\t");
					DDS_TypeSupport_dump_key (1, nts, key, 0, 0, 1);
					dbg_printf ("\r\n");
				}
#endif
			}
#endif
			ow.data = out;
			ow.length = len;
			ow.left = len;
			klen2 = DDS_KeySizeFromMarshalled (ow, nts, 0, &rc);
			fail_unless (klen2 != 0 && rc == DDS_RETCODE_OK);
			key2 = malloc ((klen2 + 3) & ~3);
			fail_unless (key2 != NULL);
			rc = DDS_KeyFromMarshalled (key2, ow, nts, 0);
			fail_unless (rc == DDS_RETCODE_OK);
#ifdef DUMP_DATA
			if (dump_data) {
				dbg_printf ("Key from marshalled:\r\n");
				dump_region (key2, klen2);
#ifdef PARSE_DATA
				if (parse_data) {
					dbg_printf ("\t");
					DDS_TypeSupport_dump_key (1, nts, key2, 0, 0, 1);
					dbg_printf ("\r\n");
				}
#endif
			}
#endif
			verify_key (key, klen, key2, klen2);

			rc = DDS_HashFromKey (h, key, klen, nts);
			fail_unless (rc == DDS_RETCODE_OK);
#ifdef DUMP_DATA
			if (dump_data) {
				dbg_printf ("Hash from Key:\r\n");
				dump_region (h, 16);
			}
#endif
			rc = DDS_HashFromKey (h2, key2, klen2, nts);
			fail_unless (rc == DDS_RETCODE_OK);

			fail_unless (!memcmp (h, h2, 16));
			free (key);
			free (key2);
		}
		DDS_DynamicDataFactory_delete_data ((DDS_DynamicData) ddr_out);
	}
	free (out);
#ifdef DUMP_TYPECODE
#ifdef DDS_TYPECODE
	/* if (nts->ts_prefer != MODE_CDR) {
		if (dump_typecode)
			dbg_printf ("Not CDR ==> no vendor typecode available!\r\n");
		return;
	} */
	tc = vtc_create (nts);
	fail_unless (tc != NULL);
#ifdef DUMP_DATA
	if (dump_typecode) {
		dbg_printf ("Typecode:\r\n");
		len = *((uint16_t *) (tc + 4)) + 6;
		dump_region (tc, len);
	}
#endif
	vtc_free (tc);
#endif
#endif
}
