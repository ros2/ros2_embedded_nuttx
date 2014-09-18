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

/* main.c -- Test program for the Shapes functionality. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <poll.h>
#endif
#include "tty.h"
#include "random.h"
#include "libx.h"
#ifdef DDS_SECURITY
#include "dds/dds_security.h"
#ifdef DDS_NATIVE_SECURITY
#include "nsecplug/nsecplug.h"
#else
#include "msecplug/msecplug.h"
#include "assert.h"
#include "../../plugins/secplug/xmlparse.h"
#endif
#include "../../plugins/security/engine_fs.h"
#endif
#include "cmdline.h"
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "dds/dds_aux.h"
#ifdef XTYPES_USED
#include "dds/dds_xtypes.h"
#include "dds/dds_dwriter.h"
#include "dds/dds_dreader.h"
#endif

/*#define TRACE_DISC	** Define to trace discovery endpoints. */
/*#define TRACE_DATA	** Define to trace data endpoints. */
/*#define TRACE_DIED	** Define this to print shapes that died. */

/*#define USE_TAKE	** Use take() i.o. read() to get items. */
#define	RELIABLE	/* Use Reliable mode i.o. Best-Effort. */
#define TRANSIENT_LOCAL /* Define to use Transient-Local Durability QoS.*/
/*#define KEEP_ALL	** Use KEEP_ALL history. */
#define HISTORY 1	/* History depth */

#define	MIN_X	5
#define	MAX_X	240
#define	MIN_Y	5
#define	MAX_Y	256

#define	MAIN_SX	35
#define	MAIN_SY	2
#define	MAIN_W	37
#define	MAIN_H	21
#define	MAIN_EX	(MAIN_SX + MAIN_W - 1)
#define	MAIN_EY	(MAIN_SY + MAIN_H - 1)
	
typedef enum {
	ST_SQUARE,
	ST_CIRCLE,
	ST_TRIANGLE
} SHAPE_TYPE;

#define	NSHAPETYPES	3

typedef struct shape_st SHAPE;
struct shape_st {
	SHAPE		*next;
	SHAPE_TYPE	type;
	char		color [129];
	Color_t		c;
	long		cur_x;
	long		cur_y;
	long		delta_x;
	long		delta_y;
	long		size;
	unsigned	delay;
	DDS_HANDLE	h;
	unsigned	row;
	DDS_Timer	timer;
};

typedef struct shape_writer_st {
	DDS_DataWriter	w;
	SHAPE		*instances;
} SHAPE_WRITER;

typedef struct shape_reader_st {
	DDS_DataReader	r;
	SHAPE		*instances;
} SHAPE_READER;

typedef enum {
	S_Init,
	S_Main,
	S_Publisher,
	S_Subscriber,
	S_Debug
} Screen_t;

const char		 *progname;
#ifdef DDS_DEBUG
int			 debug;			/* Debug mode. */
int			 cli_mode;		/* CLI mode. */
#endif
int			 verbose;		/* Verbose if set. */
int			 trace;			/* Trace messages if set. */
int			 exclusive;		/* Exclusive ownership. */
unsigned		 strength;		/* Exclusive strength. */
int			 white_bg;		/* Black-on-white. */
int			 aborting;		/* Abort program if set. */
int			 quit_done;		/* Quit when Tx/Rx done. */
int			 sfile;			/* Security file. */
char			 *sname;		/* Security filename. */
unsigned		 domain_id;		/* Domain. */
#ifdef DDS_SECURITY
char		 	 *engine_id;		/* Engine id. */
char			 *cert_path;		/* Certificates path. */
char			 *key_path;		/* Private key path. */
char			 *realm_name;		/* Realm name. */
#endif
#ifdef XTYPES_USED
int			 dyn_type;		/* Use dynamic Type functions. */
int			 dyn_data;		/* Use dynamic Data functions. */
DDS_DynamicType		 dtype;			/* Dynamic type. */
#endif
DDS_Topic		 topic [NSHAPETYPES];	/* Topic type. */
DDS_ContentFilteredTopic ftopic [NSHAPETYPES];	/* Filtered topic type. */
DDS_TopicDescription	 topic_desc [NSHAPETYPES];/* Topic description. */
SHAPE_WRITER		 writers [NSHAPETYPES];	/* Active published shapes. */
SHAPE_READER		 readers [NSHAPETYPES];	/* Shape listeners. */
unsigned		 subscriptions;		/* Shape subscriptions. */
DDS_Publisher		 pub;
DDS_Subscriber		 sub;
DDS_DomainParticipant	 part;
Screen_t		 cur_screen = S_Init;
char			 background [MAIN_H][MAIN_W + 1];
int			 pause_traffic = 0;
int			 menu_screen;
unsigned		 nsteps = ~0;

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "shapes -- simple non-graphical shapes demo program.\r\n");
	fprintf (stderr, "Usage: shapes [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -p <shape> <color> Publish a shape (square, circle or triangle).\r\n");
	fprintf (stderr, "   -s <shape>         Subscribe on a shape (idem).\r\n");
	fprintf (stderr, "   -x [<strength>]    Exclusive ownership with given strength (number).\r\n");
#ifdef DDS_DEBUG
	fprintf (stderr, "   -d                 Start in debug mode.\r\n");
#endif
	fprintf (stderr, "   -i <domain>        Domain Identifier.\r\n");
#ifdef DDS_SECURITY
	fprintf (stderr, "   -e <name>          Pass the name of the engine.\r\n");
	fprintf (stderr, "   -c <path>          Path of the certificate to use.\r\n");
	fprintf (stderr, "   -k <path>          Path of the private key to use.\r\n");
	fprintf (stderr, "   -z <realm>         The realm name.\r\n");
	fprintf (stderr, "   -j <filename>      Specify a security.xml file.\r\n");
#endif
	fprintf (stderr, "   -t                 Trace transmitted/received info.\r\n");
	fprintf (stderr, "   -b                 Black-on-white display i.o. White-on-black.\r\n");
#ifdef XTYPES_USED
	fprintf (stderr, "   -y                 Use Dynamic Type/Data functions.\r\n");
#endif
	fprintf (stderr, "   -v                 Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv                Extra verbose: log detailed functionality.\r\n");
	exit (1);
}

unsigned rnd (unsigned max)
{
	return (fastrand () % max);
}

static const char *types_str [] = { "Square", "Circle", "Triangle" };

/* shape_add -- Add a shape to the list of published shapes. */

SHAPE *shape_add (SHAPE_TYPE t, const char *color)
{
	SHAPE		*s;
	unsigned	i;
	char		buf [20];

	for (s = writers [t].instances; s; s = s->next)
		if (!astrcmp (s->color, color)) {
			printf ("Shape already exists!\r\n");
			return (NULL);
		}
	s = (SHAPE *) malloc (sizeof (SHAPE));
	if (!s) {
		printf ("Out of memory!");
		usage ();
	}
	strncpy (s->color, color, sizeof (s->color));
	s->c = tty_color_type (color);
	for (i = 0; i < strlen (color); i++)
		if (s->color [i] >= 'a' && s->color [i] <= 'z')
			s->color [i] -= 'a' - 'A';
	s->type = t;
	s->cur_x = rnd (MAX_X - MIN_X) + MIN_X;
	s->cur_y = rnd (MAX_Y - MIN_Y) + MIN_Y;
	s->delta_x = 1 + rnd (5);
	s->delta_y = 1 + rnd (5);
	s->size = 30;
	s->delay = 125;
	snprintf (buf, sizeof (buf), "P:%s(%s)", types_str [t], color);
	s->timer = DDS_Timer_create (buf);
	s->next = writers [t].instances;
	writers [t].instances = s;
	return (s);
}

SHAPE_TYPE shape_type (const char *cp)
{
	SHAPE_TYPE	t;

	if (!astrcmp (cp, "square"))
		t = ST_SQUARE;
	else if (!astrcmp (cp, "circle"))
		t = ST_CIRCLE;
	else if (!astrcmp (cp, "triangle"))
		t = ST_TRIANGLE;
	else {
		t = ST_SQUARE;
		printf ("Invalid shape type (only circle, square or triangle allowed)!\r\n");
		usage ();
	}
	return (t);
}

/* get_num -- Get a number from the command line arguments. */

int get_num (const char **cpp, unsigned *num, unsigned min, unsigned max)
{
	const char	*cp = *cpp;

	while (isspace (*cp))
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

/* get_str -- Get a string from the command line arguments. */

int get_str (const char **cpp, const char **name)
{
	const char	*cp = *cpp;

	while (isspace (*cp))
		cp++;

	*name = cp;
	while (*cp)
		cp++;

	*cpp = cp;
	return (1);
}

#define	INC_ARG()	if (!*cp) { i++; cp = argv [i]; }

/* do_switches -- Command line switch decoder. */

int do_switches (int argc, const char **argv)
{
	int		i;
	const char	*cp, *color, *type;
	SHAPE_TYPE	t;
#ifdef DDS_SECURITY
	const char      *arg_input;
#endif

	progname = argv [0];
	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
				case 'p':
					INC_ARG ();
					if (!get_str (&cp, &type)) {
						printf ("shape type expected!\r\n");
						usage ();
					}
					t = shape_type (type);
					INC_ARG();
					if (!get_str (&cp, &color)) {
						printf ("color expected!\r\n");
						usage ();
					}
					shape_add (t, color);
					break;
				case 's':
					INC_ARG ();
					if (!get_str (&cp, &type)) {
						printf ("shape type expected!\r\n");
						usage ();
					}
					t = shape_type (type);
					subscriptions |= 1 << t;
					break;
#ifdef DDS_DEBUG
				case 'd':
					debug = 1;
					break;
#endif
				case 'i':
					INC_ARG ();
					get_num (&cp, &domain_id, 0, 255);
					break;
#ifdef DDS_SECURITY
			        case 'e':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage();
					engine_id = malloc (strlen (arg_input) + 1);
					strcpy (engine_id, arg_input);
					break;
			        case 'c':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					cert_path = malloc (strlen (arg_input) + 1);
					strcpy (cert_path, arg_input);
					break;
			        case 'k':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					key_path = malloc (strlen (arg_input) + 1);
					strcpy (key_path, arg_input);

					break;
			        case 'z':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					realm_name = malloc (strlen (arg_input) + 1);
					strcpy (realm_name, arg_input);
					break;
			        case 'j':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					sname = malloc (strlen (arg_input) + 1);
					strcpy (sname, arg_input);
					sfile = 1;
					break;
#endif
				case 't':
					trace = 1;
					break;
				case 'v':
					verbose = 1;
					if (*cp == 'v') {
						verbose = 2;
						cp++;
					}
					break;
				case 'x':
					INC_ARG ();
					exclusive = 1;
					get_num (&cp, &strength, 0, 0x7fffffff);
					break;
				case 'b':
					white_bg = 1;
					break;
#ifdef XTYPES_USED
				case 'y':
					dyn_type = dyn_data = 1;
					break;
#endif
				default:
					usage ();
				break;
			}
		}
	}
	return (i);
}

typedef struct shape_type_st {
	char		color [129];
	int		x;
	int		y;
	int		shapesize;
} ShapeType_t;

#define	NMSGS	4

static DDS_TypeSupport_meta shape_tsm [] = {
	{ CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "ShapeType", sizeof (struct shape_type_st), 0, 4, 0, NULL },
	{ CDR_TYPECODE_CSTRING,TSMFLAG_KEY, "color", 129, offsetof (struct shape_type_st, color), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "x", 0, offsetof (struct shape_type_st, x), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "y", 0, offsetof (struct shape_type_st, y), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "shapesize", 0, offsetof (struct shape_type_st, shapesize), 0, 0, NULL }
};

static DDS_TypeSupport shape_ts;

DDS_ReturnCode_t register_ShapeType_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;
#ifdef XTYPES_USED
	DDS_TypeDescriptor 	*desc;
	DDS_MemberDescriptor 	*md;
	DDS_DynamicTypeMember	dtm;
	DDS_AnnotationDescriptor *key_ad;
	DDS_DynamicTypeBuilder	sb, ssb;
	DDS_DynamicType 	s, ss;

	if (dyn_type) {
		desc = DDS_TypeDescriptor__alloc ();
		if (!desc)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		md = DDS_MemberDescriptor__alloc ();
		if (!md)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		
		key_ad = DDS_AnnotationDescriptor__alloc ();
		if (!key_ad)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		key_ad->type = DDS_DynamicTypeBuilderFactory_get_builtin_annotation ("Key");
		if (!key_ad->type)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "ShapeType";
		sb = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!sb)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		ssb = DDS_DynamicTypeBuilderFactory_create_string_type (128);
		if (!ssb)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		ss = DDS_DynamicTypeBuilder_build (ssb);
		if (!ss)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		md->name = "color";
		md->index = md->id = 0;
		md->type = ss;
		md->default_value = NULL;
		if (!md->type)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		error = DDS_DynamicTypeBuilder_add_member (sb, md);
		if (error)
			return (error);

		dtm = DDS_DynamicTypeMember__alloc ();
		if (!dtm)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		error = DDS_DynamicTypeBuilder_get_member (sb, dtm, 0);
		if (error)
			return (error);

		error = DDS_DynamicTypeMember_apply_annotation (dtm, key_ad);
		if (error)
			return (error);

		md->name = "x";
		md->index = md->id = 1;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
		if (!md->type)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		error = DDS_DynamicTypeBuilder_add_member (sb, md);
		if (error)
			return (error);

		md->name = "y";
		md->index = md->id = 2;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
		if (!md->type)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		error = DDS_DynamicTypeBuilder_add_member (sb, md);
		if (error)
			return (error);

		md->name = "shapesize";
		md->index = md->id = 3;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
		if (!md->type)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		error = DDS_DynamicTypeBuilder_add_member (sb, md);
		if (error)
			return (error);

		s = DDS_DynamicTypeBuilder_build (sb);
		if (!s)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		DDS_DynamicTypeBuilderFactory_delete_type (ssb);
		DDS_DynamicTypeBuilderFactory_delete_type (sb);

		dtype = s;
		shape_ts = (DDS_TypeSupport) DDS_DynamicTypeSupport_create_type_support (s);

		DDS_DynamicTypeBuilderFactory_delete_type (ss);

		DDS_TypeDescriptor__free (desc);
		DDS_MemberDescriptor__free (md);
		DDS_AnnotationDescriptor__free (key_ad);
		DDS_DynamicTypeMember__free (dtm);
	}
	else
#endif
		shape_ts = DDS_DynamicType_register (shape_tsm);

       	if (!shape_ts)
               	return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, shape_ts, "ShapeType");
	return (error);
}

void unregister_ShapeType_type (DDS_DomainParticipant part)
{
	DDS_DomainParticipant_unregister_type (part, shape_ts, "ShapeType");
#ifdef XTYPES_USED
	if (dyn_type)
		DDS_DynamicTypeBuilderFactory_delete_type (dtype);
#endif
}

typedef enum {
	SM_NEW,
	SM_UPDATE,
	SM_DELETE,
	SM_NO_WRITERS
} SHAPE_MODE;

static void shape_trace (char        type,
			 DDS_HANDLE  h,
			 SHAPE_TYPE  t,
			 ShapeType_t *st,
			 SHAPE_MODE  m)
{
	printf ("%c [%u] - %s(%s): ", type, h, types_str [t], st->color);
	if (m == SM_DELETE)
		printf ("<shape deleted>");
	else if (m == SM_NO_WRITERS)
		printf ("<last writer gone>");
	else {
		printf ("X=%d, Y=%d, size=%d", st->x, st->y, st->shapesize);
		if (m == SM_NEW)
			printf (" <new shape>");
	}
	printf ("\r\n");
}

static void shape_move (SHAPE *s)
{
	s->cur_x += s->delta_x;
	if (s->delta_x < 0 && s->cur_x < MIN_X) {
		s->cur_x = MIN_X;
		s->delta_x = -s->delta_x;
	}
	else if (s->delta_x > 0 && s->cur_x > MAX_X) {
		s->cur_x = MAX_X;
		s->delta_x = -s->delta_x;
	}
	s->cur_y += s->delta_y;
	if (s->delta_y < 0 && s->cur_y < MIN_Y) {
		s->cur_y = MIN_Y;
		s->delta_y = -s->delta_y;
	}
	else if (s->delta_y > 0 && s->cur_y > MAX_Y) {
		s->cur_y = MAX_Y;
		s->delta_y = -s->delta_y;
	}
}

#define	CX_HOME	74

static void shape_timeout (uintptr_t user)
{
	SHAPE		*s = (SHAPE *) user;
	SHAPE_WRITER	*wp = &writers [s->type];
	ShapeType_t	data;
	DDS_ReturnCode_t rc;
#ifdef XTYPES_USED
	DDS_DynamicData	dd;
	DDS_MemberId	id;
#endif

	if (aborting)
		return;

	if (!pause_traffic) {
		shape_move (s);
		if (cur_screen == S_Main) {
			tty_gotoxy (21, s->row);
			tty_attr_reset ();
			tty_printf ("%3ld %3ld", s->cur_x, s->cur_y);
			tty_gotoxy (CX_HOME, 1);
		}
#ifdef XTYPES_USED
		if (dyn_data) {
			dd = DDS_DynamicDataFactory_create_data (dtype);
			if (!dd)
				fatal ("shape_timeout: Can't create dynamic data!");

			id = DDS_DynamicData_get_member_id_by_name (dd, "color");
			rc = DDS_DynamicData_set_string_value (dd, id, s->color);
			if (rc)
				fatal ("Can't add data member(color) (%s)!", DDS_error (rc));

			id = DDS_DynamicData_get_member_id_by_name (dd, "x");
			rc = DDS_DynamicData_set_int32_value (dd, id, s->cur_x);
			if (rc)
				fatal ("Can't add data member(x) (%s)!", DDS_error (rc));


			id = DDS_DynamicData_get_member_id_by_name (dd, "y");
			rc = DDS_DynamicData_set_int32_value (dd, id, s->cur_y);
			if (rc)
				fatal ("Can't add data member(y) (%s)!", DDS_error (rc));

			id = DDS_DynamicData_get_member_id_by_name (dd, "shapesize");
			rc = DDS_DynamicData_set_int32_value (dd, id, s->size);
			if (rc)
				fatal ("Can't add data member(shapesize) (%s)!", DDS_error (rc));

			rc = DDS_DynamicDataWriter_write (wp->w, dd, s->h);
			DDS_DynamicDataFactory_delete_data (dd);
		}
		else {
#endif
			strncpy (data.color, s->color, sizeof (data.color));
			data.x = s->cur_x;
			data.y = s->cur_y;
			data.shapesize = s->size;
			rc = DDS_DataWriter_write (wp->w, &data, s->h);
#ifdef XTYPES_USED
		}
#endif
		if (rc)
			fatal ("Can't write shape data (%s)!", DDS_error (rc));

		if (nsteps && nsteps != ~0U && !--nsteps)
			pause_traffic = 1;

		if (cur_screen == S_Debug && trace)
			shape_trace ('W', s->h, s->type, &data, SM_UPDATE);
	}
	DDS_Timer_start (s->timer, s->delay, user, shape_timeout);
}

static void shape_topic_create (SHAPE_TYPE t,
		                int        filter,
		                unsigned   x1,
		                unsigned   x2,
		                unsigned   y1,
		                unsigned   y2)
{
	static const char *expression = "x > %0 and x < %1 and y > %2 and y < %3";
	char name [40];
	char spars [4][20];
	char *sp;
	unsigned i;
	DDS_StringSeq parameters;

	topic [t] = DDS_DomainParticipant_create_topic (part, types_str [t], "ShapeType",
									NULL, NULL, 0);

	/* Create Topic. */
	if (!topic [t])
		fatal ("DDS_DomainParticipant_create_topic () failed!");

	if (verbose)
		printf ("DDS Topic (%s) created.\r\n", types_str [t]);

	/* If filter requested, create a content filtered topic. */
	if (filter) {
		snprintf (name, sizeof (name), "Filter_%d", t);
		snprintf (spars [0], sizeof (spars [0]), "%u", x1);
		snprintf (spars [1], sizeof (spars [1]), "%u", x2);
		snprintf (spars [2], sizeof (spars [2]), "%u", y1);
		snprintf (spars [3], sizeof (spars [3]), "%u", y2);
		DDS_SEQ_INIT (parameters);
		for (i = 0; i < 4; i++) {
			sp = spars [i];
			dds_seq_append (&parameters, &sp);
		}
		ftopic [t] = DDS_DomainParticipant_create_contentfilteredtopic (
				part, name, topic [t], expression, &parameters);
		if (!ftopic [t])
			fatal ("Couldn't create content-filtered topic!");
	}
	else
		strncpy (name, types_str [t], sizeof (name));

	/* Create Topic Description. */
	topic_desc [t] = DDS_DomainParticipant_lookup_topicdescription (part, name);
	if (!topic_desc [t]) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for topic!");
	}
}

static void shape_activate (SHAPE *s)
{
	DDS_DataWriterQos 	wr_qos;
	SHAPE_WRITER		*wp = &writers [s->type];
	ShapeType_t		data;

	if (!wp->w) {
		if (!pub) {	/* Create a publisher. */
			pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0);
			if (!pub)
				fatal ("DDS_DomainParticipant_create_publisher () failed!");

			if (verbose)
				printf ("DDS Publisher created.\r\n");
		}
		if (!topic [s->type])
			shape_topic_create (s->type, 0, 0, 0, 0, 0);

		/* Setup writer QoS parameters. */
		DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
#ifdef TRANSIENT_LOCAL
		wr_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
#endif
#ifdef RELIABLE
		wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
#endif
#ifdef KEEP_ALL
		wr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
		wr_qos.history.depth = DDS_LENGTH_UNLIMITED;
		wr_qos.resource_limits.max_samples_per_instance = HISTORY;
		wr_qos.resource_limits.max_instances = HISTORY * 10;
		wr_qos.resource_limits.max_samples = HISTORY * 4;
#else
		wr_qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
		wr_qos.history.depth = HISTORY;
#endif
		if (exclusive) {
			wr_qos.ownership.kind = DDS_EXCLUSIVE_OWNERSHIP_QOS;
			wr_qos.ownership_strength.value = strength;
		}
#ifdef TRACE_DATA
		rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
		/* Create a Data Writer. */
		wp->w = DDS_Publisher_create_datawriter (pub, topic [s->type], &wr_qos, NULL, 0);
		if (!wp->w) {
			fatal ("Unable to create writer");
			DDS_DomainParticipantFactory_delete_participant (part);
		}
		if (verbose)
			printf ("DDS Writer (%s) created.\r\n", types_str [s->type]);
	}
	strncpy (data.color, s->color, sizeof (data.color));
	s->h = DDS_DataWriter_register_instance (wp->w, &data);
	shape_timeout ((uintptr_t) s);
}

typedef enum {
	DT_Erase,
	DT_Blink,
	DT_Normal,
	DT_Dead
} DrawType_t;

void draw_shape (SHAPE_TYPE st,
		 Color_t    color,
		 unsigned   x,
		 unsigned   y,
		 DrawType_t dt)
{
	unsigned	cx, cy;
	Color_t		bg = (white_bg) ? TC_White : TC_Black;
	static char	shape_type_ch [] = "SCT";

	cx = 1 + (x / 7);
	cy = 1 + (y / 14);
	tty_gotoxy (MAIN_SX + cx, MAIN_SY + cy);
	switch (dt) {
		case DT_Erase:
			tty_attr_reset ();
			tty_line_drawing (1);
			tty_printf ("%c", background [cy][cx]);
			tty_line_drawing (0);
			tty_gotoxy (70, 1);
			return;
		case DT_Blink:
			tty_attr_set (TA_Blink | TA_Bright);
			tty_color (color, bg);
			break;
		case DT_Normal:
			tty_color (color, bg);
			break;
		case DT_Dead:
			tty_attr_set (TA_Dim | TA_Underline);
			tty_color (color, bg);
			break;
		default:
			tty_gotoxy (70, 1);
	  		return;
	}
	tty_printf ("%c", shape_type_ch [st]);
	tty_attr_reset ();
	tty_gotoxy (CX_HOME, 1);
}

void shape_alive_timeout (uintptr_t user)
{
	SHAPE	*sp = (SHAPE *) user;

	if (cur_screen == S_Main)
		tty_gotoxy (1, 23);

#ifdef TRACE_DIED
	printf ("Shape died: %s (%s)!      \r\n", types_str [sp->type], sp->color);
#endif
	if (cur_screen == S_Main)
		draw_shape (sp->type, sp->c, sp->cur_x, sp->cur_y, DT_Dead);
}

void shape_new (SHAPE_TYPE t, ShapeType_t *s, DDS_HANDLE h)
{
	SHAPE	*sp;
	char	buf [20];

	sp = (SHAPE *) malloc (sizeof (SHAPE));
	if (!sp) {
		printf ("Out of memory!");
		return;
	}
	strncpy (sp->color, s->color, sizeof (sp->color));
	sp->c = tty_color_type (s->color);
	sp->type = t;
	sp->cur_x = s->x;
	sp->cur_y = s->y;
	sp->delta_x = 0;
	sp->size = s->shapesize;
	sp->h = h;
	sp->next = readers [t].instances;
	readers [t].instances = sp;
	snprintf (buf, sizeof (buf), "S:%s(%s)", types_str [t], s->color);
	sp->timer = DDS_Timer_create (buf);
	if (cur_screen == S_Main)
		draw_shape (sp->type, sp->c, sp->cur_x, sp->cur_y, DT_Blink);
	else if (cur_screen == S_Debug && trace)
		shape_trace ('R', h, t, s, SM_NEW);

	DDS_Timer_start (sp->timer, 5000, (uintptr_t) sp, shape_alive_timeout);
}

SHAPE *shape_lookup (SHAPE_TYPE t, DDS_HANDLE h)
{
	SHAPE	*sp;

	for (sp = readers [t].instances; sp; sp = sp->next)
		if (sp->h == h)
			return (sp);

	return (NULL);
}

void shape_update (SHAPE *sp, ShapeType_t *np)
{
	DrawType_t	dt;

	if (cur_screen == S_Main)
		draw_shape (sp->type, sp->c, sp->cur_x, sp->cur_y, DT_Erase);
	sp->cur_x = np->x;
	sp->cur_y = np->y;
	sp->delta_x++;
	if (cur_screen == S_Main) {
		dt = (sp->delta_x < 10) ? DT_Blink : DT_Normal;
		draw_shape (sp->type, sp->c, sp->cur_x, sp->cur_y, dt);
	}
	else if (cur_screen == S_Debug && trace)
		shape_trace ('R', sp->h, sp->type, np, SM_UPDATE);
	DDS_Timer_start (sp->timer, 5000, (uintptr_t) sp, shape_alive_timeout);
}

void shape_delete (SHAPE *sp)
{
	SHAPE		*prev_sp, *xsp;
	ShapeType_t	data;

	for (prev_sp = NULL, xsp = readers [sp->type].instances;
	     xsp;
	     prev_sp = xsp, xsp = xsp->next)
		if (xsp == sp) {
			if (prev_sp)
				prev_sp->next = sp->next;
			else
				readers [sp->type].instances = sp->next;
			if (cur_screen == S_Main)
				draw_shape (sp->type, sp->c, sp->cur_x, sp->cur_y, DT_Erase);
			else if (cur_screen == S_Debug && trace) {
				strncpy (data.color, sp->color, sizeof (data.color));
				shape_trace ('R', sp->h, sp->type, &data, SM_DELETE);
			}
			DDS_Timer_stop (sp->timer);
			DDS_Timer_delete (sp->timer);
			free (sp);
			break;
		}
}

void shape_no_writers (SHAPE *sp)
{
	SHAPE		*xsp;
	ShapeType_t	data;

	for (xsp = readers [sp->type].instances;
	     xsp;
	     xsp = xsp->next)
		if (xsp == sp) {
			if (cur_screen == S_Main)
				draw_shape (sp->type, sp->c, sp->cur_x, sp->cur_y, DT_Dead);
			else if (cur_screen == S_Debug && trace) {
				strncpy (data.color, sp->color, sizeof (data.color));
				shape_trace ('R', sp->h, sp->type, &data, SM_NO_WRITERS);
			}
			break;
		}
}

void shape_read (SHAPE_TYPE t, DDS_DataReader dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
#ifdef XTYPES_USED
	static DDS_DynamicDataSeq drx_sample = DDS_SEQ_INITIALIZER (void *);
#endif
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	ShapeType_t		*sample;
	DDS_ReturnCode_t	error;
	SHAPE			*sp;
#ifdef XTYPES_USED
	DDS_DynamicData		dd;
	DDS_MemberId		id;
	ShapeType_t		shape;
#endif

	/*printf ("shape_read(%s): got notification!\r\n", types_str [t]);*/
	for (;;) {
#ifdef XTYPES_USED
		if (dyn_data)
#ifdef USE_TAKE
			error = DDS_DynamicDataReader_take (dr, &drx_sample, &rx_info, 1, ss, vs, is);
#else
			error = DDS_DynamicDataReader_read (dr, &drx_sample, &rx_info, 1, ss, vs, is);
#endif
		else
#endif
#ifdef USE_TAKE
			error = DDS_DataReader_take (dr, &rx_sample, &rx_info, 1, ss, vs, is);
#else
			error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
#endif
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				printf ("Unable to read samples: error = %s!\r\n", DDS_error (error));
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			info = DDS_SEQ_ITEM (rx_info, 0);
#ifdef XTYPES_USED
			if (info->valid_data && dyn_data) {
				dd = DDS_SEQ_ITEM (drx_sample, 0);
				if (!dd)
					fatal ("Empty dynamic sample!");

				id = DDS_DynamicData_get_member_id_by_name (dd, "color");
				error = DDS_DynamicData_get_string_value (dd, shape.color, id);
				if (error)
					fatal ("Can't get data member(color) (%s)!", DDS_error (error));

				id = DDS_DynamicData_get_member_id_by_name (dd, "x");
				error = DDS_DynamicData_get_int32_value (dd, &shape.x, id);
				if (error)
					fatal ("Can't get data member(x) (%s)!", DDS_error (error));

				id = DDS_DynamicData_get_member_id_by_name (dd, "y");
				error = DDS_DynamicData_get_int32_value (dd, &shape.y, id);
				if (error)
					fatal ("Can't get data member(y) (%s)!", DDS_error (error));

				id = DDS_DynamicData_get_member_id_by_name (dd, "shapesize");
				error = DDS_DynamicData_get_int32_value (dd, &shape.shapesize, id);
				if (error)
					fatal ("Can't get data member(shapesize) (%s)!", DDS_error (error));

				sample = &shape;
			}
			else
#endif
				if (info->valid_data)
					sample = DDS_SEQ_ITEM (rx_sample, 0);
				else
					sample = NULL;

			if (info->instance_state == DDS_ALIVE_INSTANCE_STATE) {
				if (info->view_state == DDS_NEW_VIEW_STATE)
					shape_new (t, sample, info->instance_handle);
				else {
					sp = shape_lookup (t, info->instance_handle);
					if (sp)
						shape_update (sp, sample);
				}
			}
			else {
				sp = shape_lookup (t, info->instance_handle);
				if (sp) {
					if (info->instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
						shape_delete (sp);
					else
						shape_no_writers (sp);
				}
			} 
#ifdef XTYPES_USED
			if (dyn_data)
				DDS_DynamicDataReader_return_loan (dr, &drx_sample, &rx_info);
			else
#endif
				DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else
			return;
	}
}

void read_square (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	ARG_NOT_USED (l)

	shape_read (ST_SQUARE, dr);
}

void read_circle (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	ARG_NOT_USED (l)

	shape_read (ST_CIRCLE, dr);
}

void read_triangle (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	ARG_NOT_USED (l)

	shape_read (ST_TRIANGLE, dr);
}

static DDS_DataReaderListener r_listener_square = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	NULL,		/* Requested incompatible QoS. */
	read_square,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

static DDS_DataReaderListener r_listener_circle = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	NULL,		/* Requested incompatible QoS. */
	read_circle,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

static DDS_DataReaderListener r_listener_triangle = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	NULL,		/* Requested incompatible QoS. */
	read_triangle,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

static DDS_DataReaderListener *r_listeners [] = {
	&r_listener_square,
	&r_listener_circle,
	&r_listener_triangle
};

void reader_activate (SHAPE_TYPE t,
		      int        filter,
		      unsigned   x1,
		      unsigned   x2,
		      unsigned   y1,
		      unsigned   y2)
{
	DDS_DataReaderQos	rd_qos;

	/* Create a topic */
	if (!topic [t])
		shape_topic_create (t, filter, x1, x2, y1, y2);

	if (!sub) {
		sub = DDS_DomainParticipant_create_subscriber (part, NULL, NULL, 0); 
		if (!sub)
			fatal ("DDS_DomainParticipant_create_subscriber () returned an error!");

		if (verbose)
			printf ("DDS Subscriber created.\r\n");
	}

	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
#ifdef TRANSIENT_LOCAL
	rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
#endif
#ifdef RELIABLE
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
#endif
#ifdef KEEP_ALL
	rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	rd_qos.history.depth = DDS_LENGTH_UNLIMITED;
	rd_qos.resource_limits.max_samples_per_instance = HISTORY;
	rd_qos.resource_limits.max_instances = HISTORY * 10;
	rd_qos.resource_limits.max_samples = HISTORY * 4;
#else
	rd_qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
	rd_qos.history.depth = HISTORY;
#endif
	if (exclusive)
		rd_qos.ownership.kind = DDS_EXCLUSIVE_OWNERSHIP_QOS;

	/* Create a reader. */
#ifdef TRACE_DATA
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	readers [t].r = DDS_Subscriber_create_datareader (sub, topic_desc [t], &rd_qos, r_listeners [t], DDS_DATA_AVAILABLE_STATUS);
	if (!readers [t].r)
		fatal ("DDS_DomainParticipant_create_datareader () returned an error!");

	if (verbose)
		printf ("DDS Reader (%s) created.\r\n", types_str [t]);
}

void init_background (void)
{
	unsigned	x, y;

	for (y = 0; y < MAIN_H; y++) {
		for (x = 0; x < MAIN_W; x++)
			background [y][x] = ' ';
		background [y][MAIN_W] = '\0';
	}
}

void draw (unsigned x, unsigned y, char c)
{
	background [y][x] = c;
}

void draw_rectangle (unsigned sx, unsigned sy, unsigned ex, unsigned ey, int dashed)
{
	unsigned	x, y;

	tty_line_drawing (1);
	draw (sx, sy, tty_corners [TC_UpperLeft]);
	for (x = sx + 1; x < ex; x++)
		if (dashed && (x & 1) == 0)
			draw (x, sy, ' ');
		else
			draw (x, sy, tty_borders [TB_Top]);
	draw (ex, sy, tty_corners [TC_UpperRight]);
	for (y = sy + 1; y < ey; y++)
		if (dashed && (y & 1) == 0) {
			draw (sx, y, ' ');
			draw (ex, y, ' ');
		}
		else {
			draw (sx, y, tty_borders [TB_Left]);
			draw (ex, y, tty_borders [TB_Right]);
		}
	draw (sx, ey, tty_corners [TC_LowerLeft]);
	for (x = sx + 1; x < ex; x++)
		if (dashed && (x & 1) == 0)
			draw (x, ey, ' ');
		else
			draw (x, ey, tty_borders [TB_Bottom]);
	draw (ex, ey, tty_corners [TC_LowerRight]);
	tty_line_drawing (0);
}

void draw_background (void)
{
	unsigned y;

	tty_line_drawing (1);
	for (y = 0; y < MAIN_H; y++) {
		tty_gotoxy (MAIN_SX, MAIN_SY + y);
		tty_printf ("%s", background [y]);
	}
	tty_line_drawing (0);
}

void shapes_cleanup (void)
{
	SHAPE_TYPE	t;
	SHAPE		*s;

	for (t = 0; t < NSHAPETYPES; t++) {
		while (writers [t].instances) {
			s = writers [t].instances;
			DDS_DataWriter_unregister_instance (writers [t].w, NULL, s->h);
			/*log_printf (USER_ID, 0, "Unregistered writer!\r\n");*/
			writers [t].instances = s->next;
			DDS_Timer_stop (s->timer);
			DDS_Timer_delete (s->timer);
			free (s);
		}
		while (readers [t].instances) {
			s = readers [t].instances;
			readers [t].instances = s->next;
			DDS_Timer_stop (s->timer);
			DDS_Timer_delete (s->timer);
			free (s);
		}
	}

	usleep (1000);
}

void print_option (char c, char *s)
{
	tty_attr_set (TA_Bright | TA_Underline);
	tty_printf ("%c", c);
	tty_attr_reset ();
	tty_printf ("%s  ", s);
}

const char *shape_type_str [] = { "Square", "Circle", "Triangle" };

int shape_type_menu (SHAPE_TYPE *type)
{
	char	ch;

	tty_attr_reset ();
	tty_gotoxy (4, 6);
	tty_printf ("Shape type: ");
	tty_gotoxy (8, 8);
	print_option ('S', "quare");
	tty_gotoxy (8, 9);
	print_option ('C', "ircle");
	tty_gotoxy (8, 10);
	print_option ('T', "riangle");
	tty_gotoxy (8, 12);
	tty_printf ("or <esc> to return");
	tty_gotoxy (10, 14);
	tty_printf ("Choice? ");
	for (;;) {
		ch = tty_getch ();
		switch (ch) {
			case 's':
			case 'S':
				*type = ST_SQUARE;
				break;
			case 'c':
			case 'C':
				*type = ST_CIRCLE;
				break;
			case 't':
			case 'T':
				*type = ST_TRIANGLE;
				break;
		  	case '\x1b':
				return (1);
			default:
				printf ("\a");
				continue;
		}
		break;
	}
	tty_gotoxy (4, 6);
	tty_printf ("Shape type = %s", shape_type_str [*type]);
	tty_erase_eos ();
	return (0);
}

const char *shape_color_str [] = {
	"Black", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White"
};

int color_menu (Color_t *color)
{
	char	ch;

	tty_attr_reset ();
	tty_gotoxy (4, 7);
	tty_printf ("Shape color:");
	tty_gotoxy (8, 9);
	print_option ('R', "ed");
	tty_gotoxy (8, 10);
	print_option ('G', "reen");
	tty_gotoxy (8, 11);
	print_option ('Y', "ellow");
	tty_gotoxy (8, 12);
	print_option ('B', "lue");
	tty_gotoxy (8, 13);
	print_option ('M', "agenta");
	tty_gotoxy (8, 14);
	print_option ('C', "yan");

	tty_gotoxy (8, 16);
	tty_printf ("or <esc> to return");
	tty_gotoxy (10, 18);
	tty_printf ("Choice? ");
	for (;;) {
		ch = tty_getch ();
		switch (ch) {
			case 'r':
			case 'R':
				*color = TC_Red;
				break;
			case 'g':
			case 'G':
				*color = TC_Green;
				break;
			case 'y':
			case 'Y':
				*color = TC_Yellow;
				break;
			case 'b':
			case 'B':
				*color = TC_Blue;
				break;
			case 'm':
			case 'M':
				*color = TC_Magenta;
				break;
			case 'c':
			case 'C':
				*color = TC_Cyan;
				break;
		  	case '\x1b':
				return (1);
			default:
				printf ("\a");
				continue;
		}
		break;
	}
	tty_gotoxy (4, 7);
	tty_printf ("Shape color = %s", shape_color_str [*color]);
	tty_erase_eos ();
	tty_printf ("\r\n\r\n");
	return (0);
}

int filter_menu (int      *filter,
		 unsigned *x1,
		 unsigned *x2,
		 unsigned *y1,
		 unsigned *y2)
{
	char		ch;
	unsigned	i;
	unsigned	numbers [4];
	static const char *cstr [4] = {"X1", "X2", "Y1", "Y2"};
	unsigned min [4] = { MIN_X, MIN_X, MIN_Y, MIN_Y };
	unsigned max [4] = { MAX_X - 2, MAX_X, MAX_Y - 2, MAX_Y };
	char		input [80];

	tty_attr_reset ();
	tty_gotoxy (4, 7);
	tty_printf ("Filter: ");
	tty_gotoxy (8, 9);
	print_option ('Y', "es");
	tty_gotoxy (8, 10);
	print_option ('N', "o");
	tty_gotoxy (8, 12);
	tty_printf ("or <esc> to return");
	tty_gotoxy (10, 14);
	tty_printf ("Choice? ");
	for (;;) {
		ch = tty_getch ();
		switch (ch) {
			case 'y':
			case 'Y':
				*filter = 1;
				break;
			case 'n':
			case 'N':
				*filter = 0;
				break;
		  	case '\x1b':
				return (1);
			default:
				printf ("\a");
				continue;
		}
		break;
	}
	tty_gotoxy (4, 7);
	tty_attr_reset ();
	tty_printf ("Filter = %s", (*filter) ? "Yes" : "No");
	tty_erase_eos ();
	if (!*filter)
		return (0);

	tty_attr_reset ();

	/* Get coordinates. */
	for (i = 0; i < 4; i++) {
		tty_gotoxy (4, 8 + i);
		tty_attr_reset ();
		tty_printf ("%s (%u..%u) : ", cstr [i], min [i], max [i]);
		if (tty_gets (sizeof (input), input, 1, 1))
			return (1);

		numbers [i] = atoi (input);
		if (numbers [i] < min [i] || numbers [i] > max [i]) {
			tty_printf ("Coordinate not in range!");
			return (1);
		}
		if (i == 0)
			min [1] = i + 2;
		else if (i == 2)
			min [3] = i + 2;
	}
	*x1 = numbers [0];
	*x2 = numbers [1];
	*y1 = numbers [2];
	*y2 = numbers [3];
	draw_rectangle (1 + (*x1 / 7), 1 + (*y1 / 14),
			1 + (*x2 / 7), 1 + (*y2 / 14), 1);
	return (0);
}

void publish_menu (void)
{
	SHAPE		*sp;
	SHAPE_TYPE	type;
	Color_t		color;

	cur_screen = S_Publisher;
	tty_gotoxy (36, 1);
	tty_erase_eos ();
	tty_gotoxy (4, 4);
	tty_attr_set (TA_Underline | TA_Bright);
	tty_printf ("Publish");
	if (shape_type_menu (&type))
		return;

	if (color_menu (&color))
		return;

	if ((sp = shape_add (type, shape_color_str [color])) == NULL)
		return;

	shape_activate (sp);
}

void subscribe_menu (void)
{
	SHAPE_TYPE	type;
	int		filter;
	unsigned	x1, x2, y1, y2;

	cur_screen = S_Subscriber;
	tty_gotoxy (36, 1);
	tty_erase_eos ();
	tty_gotoxy (4, 4);
	tty_attr_set (TA_Underline | TA_Bright);
	tty_printf ("Subscribe");
	if (shape_type_menu (&type))
		return;

	if ((subscriptions & (1 << type)) != 0) {
		tty_printf ("Already subscribed to this shape!");
		return;
	}
	if (filter_menu (&filter, &x1, &x2, &y1, &y2))
		return;

	subscriptions |= 1 << type;
	reader_activate (type, filter, x1, x2, y1, y2);
}

void draw_main_screen (void)
{
	SHAPE_WRITER		*wp;
	SHAPE			*sp;
	SHAPE_TYPE		t;
	unsigned		y;

	tty_attr_reset ();
	tty_erase_screen ();
	tty_attr_set (TA_Bright | TA_Reverse);
	tty_printf ("Shapes demo (c) 2013, Technicolor");

	/* Display main commands. */
	tty_gotoxy (37, 1);
	tty_attr_reset ();
	print_option ('P', "ublish");
	print_option ('S', "ubscribe");
#ifdef DDS_DEBUG
	print_option ('C', "ommand");
#endif
	print_option ('Q', "uit");

	/* Display Subscriptions. */
	y = 3;
	tty_gotoxy (1, y++);
	tty_attr_set (TA_Bright | TA_Underline);
	tty_printf ("Subscriptions:");
	tty_attr_reset ();
	y++;
	for (t = ST_SQUARE; t <= ST_TRIANGLE; t++)
		if ((subscriptions & (1 << t)) != 0) {
			tty_gotoxy (4, y++);
			tty_printf ("%s", shape_type_str [t]);
			if (ftopic [t])
				tty_printf (" (filtered)");
		}
	y++;

	/* Display Publications. */
	tty_gotoxy (1, y++);
	tty_attr_set (TA_Bright | TA_Underline);
	tty_printf ("Publications:");
	tty_attr_reset ();
	y++;
	for (t = ST_SQUARE; t <= ST_TRIANGLE; t++)
		if ((wp = &writers [t]) != NULL)
			for (sp = wp->instances; sp; sp = sp->next) {
				sp->row = y++;
				tty_gotoxy (4, sp->row);
				tty_printf ("%s", shape_type_str [t]);
				tty_gotoxy (13, sp->row);
				tty_printf ("%s", sp->color);
				tty_gotoxy (21, sp->row);
				tty_printf ("%3ld %3ld", sp->cur_x, sp->cur_y);
			}

	/* Draw borders. */
	draw_background ();

	tty_gotoxy (CX_HOME, 1);
	cur_screen = S_Main;
}

#ifdef DDS_DEBUG

void draw_debug_screen (void)
{
	cur_screen = S_Debug;
	printf ("Shapes command shell -- type 'h' or '?' for help.\r\n");
}
#endif

#ifdef DDS_SECURITY

#define fail_unless     assert

static void enable_security (void)
{
#ifdef DDS_SECURITY
	DDS_Credentials		credentials;
	DDS_ReturnCode_t	error;
#ifdef MSECPLUG_WITH_SECXML
	/*int dhandle, thandle;*/
#endif

	error = DDS_SP_set_policy ();
	if (error)
		fatal ("DDS_SP_set_policy() returned error (%s)!", DDS_error (error));

#ifdef MSECPLUG_WITH_SECXML
	if (sfile) {
		if (DDS_SP_parse_xml (sname))
			fatal ("SP: no DDS security rules in '%s'!\r\n", sname);
	}
	else if (DDS_SP_parse_xml ("security.xml"))
		fatal ("MSP: no DDS security rules in 'security.xml'!\r\n");
#else
	DDS_SP_add_domain();
	if (!realm_name)
		DDS_SP_add_participant ();
	else 
		DDS_SP_set_participant_access (DDS_SP_add_participant (), strcat(realm_name, "*"), 2, 0);
#endif
	if (!cert_path || !key_path)
		fatal ("Error: you must provide a valid certificate path and a valid private key path\r\n");

	if (engine_id) {
		DDS_SP_init_engine (engine_id, init_engine_fs);
		credentials.credentialKind = DDS_ENGINE_BASED;
		credentials.info.engine.engine_id = engine_id;
		credentials.info.engine.cert_id = cert_path;
		credentials.info.engine.priv_key_id = key_path;
	}
	else {
		credentials.credentialKind = DDS_FILE_BASED;
		credentials.info.filenames.private_key_file = key_path;
		credentials.info.filenames.certificate_chain_file = cert_path;
	}
	error = DDS_Security_set_credentials ("Technicolor Shapes", &credentials);
#endif
}

static void cleanup_security (void)
{
	/* Cleanup security submodule. */
	DDS_SP_access_db_cleanup ();
	DDS_SP_engine_cleanup ();

	/* Cleanup malloc-ed memory. */
	if (engine_id)
		free (engine_id);
	if (cert_path)
		free (cert_path);
	if (key_path)
		free (key_path);
	if (realm_name)
		free (realm_name);
}

#endif

int main (int argc, const char *argv [])
{
	DDS_PoolConstraints	reqs;
	SHAPE_WRITER		*wp;
	SHAPE			*sp;
	SHAPE_TYPE		t;
	char			ch;
	int			error;
#ifdef DDS_DEBUG
	CmdLineStatus_t		ret;
	CmdLine_t		*cmdline;
	char			*command;
#endif

	do_switches (argc, argv);

	if (verbose > 1)
		DDS_Log_stdio (1);
	/*if (trace)
		rtps_trace = 1;*/

	DDS_get_default_pool_constraints (&reqs, ~0, 100);
/*	reqs.max_rx_buffers = 32;
	reqs.min_local_readers = reqs.max_local_readers = 9;
	reqs.min_local_writers = reqs.max_local_writers = 7;
	reqs.min_changes = 64;
	reqs.min_instances = 48;*/
	DDS_set_pool_constraints (&reqs);
	DDS_entity_name ("Technicolor Shapes");
#ifdef DDS_DEBUG
	DDS_Debug_abort_enable (&aborting);
	DDS_Debug_control_enable (&pause_traffic, &nsteps, NULL);
	DDS_Debug_menu_enable (&menu_screen);
	cmdline = cl_new ();
	if (!cmdline)
		fatal ("cl_new() failed!");
	cl_load (cmdline, ".shapes_hist");
#endif
#ifdef TRACE_DISC
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		enable_security ();
#endif

	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						domain_id, NULL, NULL, 0);
	if (!part)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");

#ifdef TRACE_DISC
	rtps_dtrace_set (0);
#endif
	if (verbose)
		printf ("DDS Domain Participant created.\r\n");

	/* Register the message topic type. */
	error = register_ShapeType_type (part);
	if (error) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_register_type ('ShapeType') failed (%s)!", DDS_error (error));
	}
	if (verbose)
		printf ("DDS Topic type ('%s') registered.\r\n", "ShapeType");

	for (t = ST_SQUARE; t <= ST_TRIANGLE; t++) {
		if ((subscriptions & (1 << t)) != 0)
			reader_activate (t, 0, 0, 0, 0, 0);
		if ((wp = &writers [t]) != NULL)
			for (sp = wp->instances; sp; sp = sp->next)
				shape_activate (sp);
	}
	tty_init ();
	DDS_Handle_attach (tty_stdin,
			   POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL,
			   tty_input,
			   NULL);
	init_background ();
	draw_rectangle (0, 0, MAIN_W - 1, MAIN_H - 1, 0);
#ifdef DDS_DEBUG
	cli_mode = debug;
	if (!debug)
#endif
		draw_main_screen ();
#ifdef DDS_DEBUG
	else
		draw_debug_screen ();
#endif
	while (!aborting) {
#ifdef DDS_DEBUG
		if (!debug) {
#endif
			ch = tty_getch ();
			switch (ch) {
				case 'p':
				case 'P':
					publish_menu ();
					draw_main_screen ();
					break;
				case 's':
				case 'S':
					subscribe_menu ();
					draw_main_screen ();
					break;
#ifdef DDS_DEBUG
				case 'c':
				case 'C':
					debug = 1;
					tty_gotoxy (1, 1);
					tty_erase_screen ();
					draw_debug_screen ();
					break;	
#endif
				case 'q':
				case 'Q':
					aborting = 1;
					break;
			  	default:
					printf ("\a");
			  		break;
			}
			if (cur_screen == S_Main) {
				tty_gotoxy (1,23);
				tty_erase_eos ();
				tty_printf ("\r\n");
			}
#ifdef DDS_DEBUG
		}
		else {
			printf (">");
			fflush (stdout);
			for (;;) {
				ch = tty_getch ();
				ret = cl_add_char (cmdline, ch, &command);
				if (ret == CLS_DONE_OK)
					break;
			}
			DDS_Debug_command (command);
			if (aborting && (!cli_mode || menu_screen)) {
				aborting = 0;
				debug = 0;
				draw_main_screen ();
			}
		}
#endif
	}
	shapes_cleanup ();
	unregister_ShapeType_type (part);

	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Entities deleted\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Participant deleted\r\n");

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		cleanup_security ();
#endif
#ifdef DDS_DEBUG
	cl_save (cmdline, ".shapes_hist");
	cl_delete (cmdline);
#endif
	return (0);
}

