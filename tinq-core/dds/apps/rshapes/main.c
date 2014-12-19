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
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <poll.h>
#endif
#include <ctype.h>
#include "libx.h"
#include "tty.h"
#include "random.h"
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "dds/dds_aux.h"

#define	DOMAIN_ID	0

/*#define TRACE_DISC	** Define to trace discovery endpoints. */
/*#define TRACE_DATA	** Define to trace data endpoints. */
/*#define TRACE_DIED	** Define this to print shapes that died. */

#define USE_TRANSIENT_LOCAL /* Define to use Transient-Local Durability QoS.*/

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
	long		color;
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
int			 aborting;		/* Abort program if set. */
int			 quit_done;		/* Quit when Tx/Rx done. */
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

#ifndef DDS_DEBUG
int pause_traffic = 0;
unsigned sleep_time = 1000;
#endif

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "shapes -- simple non-graphical shapes demo program.\r\n");
	fprintf (stderr, "Usage: shapes [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -p <shape> <color> Publish a shape (square, circle or triangle).\r\n");
	fprintf (stderr, "   -s <shape>         Subscribe on a shape (idem).\r\n");
#ifdef DDS_DEBUG
	fprintf (stderr, "   -d                 Start in debug mode.\r\n");
#endif
	fprintf (stderr, "   -t	         	Trace transmitted/received info.\r\n");
	fprintf (stderr, "   -v		        Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv	        Extra verbose: log detailed functionality.\r\n");
	exit (1);
}

unsigned rnd (unsigned max)
{
	return (fastrand () % max);
}

# if 0
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
	snprintf (buf, sizeof (buf), "%s:%s", types_str [t], color);
	s->timer = DDS_Timer_create (buf);
	s->next = writers [t].instances;
	writers [t].instances = s;
	return (s);
}
# endif

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
	const char	*cp, *type;
#if 0
	const char	*color;
#endif
	SHAPE_TYPE	t;

	progname = argv [0];
	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
# if 0
				case 'p':
					INC_ARG();
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
# endif
				case 's':
					INC_ARG();
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
				default:
					usage ();
				break;
			}
		}
	}
	return (i);
}

#define	NMSGS	4

#if 0	/* Original shapes type layout. */
typedef struct shape_type_st {
	char		color [128];
	int		x;
	int		y;
	int		shapesize;
} ShapeType_t;

static DDS_TypeSupport_meta shape_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 0, "ShapeType", sizeof (struct shape_type_st), 0, 4, 0, NULL },
	{ CDR_TYPECODE_CSTRING,1, "color", 128, offsetof (struct shape_type_st, color), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "x", 0, offsetof (struct shape_type_st, x), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "y", 0, offsetof (struct shape_type_st, y), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "shapesize", 0, offsetof (struct shape_type_st, shapesize), 0, 0, NULL }
};

#else

typedef struct shape_type_st {
	struct rk_st {
		struct rid_st {
			long long	device_id;
			long		runtime_id;
		}			rebus_id;
		long			class_id [3];
		struct ck_st {
			unsigned long	id;
		}			class_key;
	}				rebus_key;
	struct d_st {
		unsigned long		size;
		struct c_st {
			long		x;
			long		y;
		}			coords;
		long			color;
	}				data;
} ShapeType_t;

static DDS_TypeSupport_meta shape_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 0, "ShapeType", sizeof (ShapeType_t), 0, 2, 0, NULL },
	{ CDR_TYPECODE_STRUCT, 1, "rebus_key", sizeof (struct rk_st), offsetof (struct shape_type_st, rebus_key), 3, 0, NULL },
	{ CDR_TYPECODE_STRUCT, 1, "rebus_id",  sizeof (struct rid_st), offsetof (struct rk_st, rebus_id), 2, 0, NULL },
	{ CDR_TYPECODE_LONGLONG, 1, "device_id", 0, offsetof (struct rid_st, device_id), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   1, "runtime_id", 0, offsetof (struct rid_st, runtime_id), 0, 0, NULL },
	{ CDR_TYPECODE_ARRAY,  1, "class_id", sizeof (unsigned long) * 3, offsetof (struct rk_st, class_id), 3, 0, NULL },
	{ CDR_TYPECODE_LONG,   1, NULL, 0, 0, 0, 0, NULL },
	{ CDR_TYPECODE_STRUCT, 1, "class_key", sizeof (struct ck_st), offsetof (struct rk_st, class_key), 1, 0, NULL},
	{ CDR_TYPECODE_ULONG,  1, "id", 0, offsetof (struct ck_st, id), 0, 0, NULL },
	{ CDR_TYPECODE_STRUCT, 0, "data", sizeof (struct d_st), offsetof (ShapeType_t, data), 3, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "size", 0, offsetof (struct d_st, size), 0, 0, NULL },
	{ CDR_TYPECODE_STRUCT, 0, "coords", sizeof (struct c_st), offsetof (struct d_st, coords), 2, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "x", 0, offsetof (struct c_st, x), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "y", 0, offsetof (struct c_st, y), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "color", 0, offsetof (struct d_st, color), 0, 0, NULL }
};

#endif

DDS_ReturnCode_t register_ShapeType_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;
	DDS_TypeSupport		dds_ts;

	dds_ts = DDS_DynamicType_register (shape_tsm);
        if (!dds_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, dds_ts, "demo.shapes.square.shapedata");
	if (error)
		return (error);

	error = DDS_DomainParticipant_register_type (part, dds_ts, "demo.shapes.circle.shapedata");
	if (error)
		return (error);

	error = DDS_DomainParticipant_register_type (part, dds_ts, "demo.shapes.triangle.shapedata");
	if (error)
		return (error);

	return (error);
}

static const char *types_str [] = { "Square", "Circle", "Triangle" };

typedef enum {
	SM_NEW,
	SM_UPDATE,
	SM_DELETE
} SHAPE_MODE;

static void shape_trace (char        type,
			 DDS_HANDLE  h,
			 SHAPE_TYPE  t,
			 ShapeType_t *st,
			 SHAPE_MODE  m)
{
	printf ("%c [%u] - %s(%ld): ", type, h, types_str [t], st->data.color);
	printf ("Key: %ld ", st->rebus_key.class_key.id);
	if (m == SM_DELETE)
		printf ("<shape died>");
	else {
		printf ("X=%ld, Y=%ld, size=%ld", st->data.coords.x, st->data.coords.y, st->data.size);
		if (m == SM_NEW)
			printf (" <new shape>");
	}
	printf ("\r\n");
}

#define	CX_HOME	74

#if 0
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

static void shape_timeout (uintptr_t user)
{
	SHAPE		*s = (SHAPE *) user;
	SHAPE_WRITER	*wp = &writers [s->type];
	ShapeType_t	data;
	DDS_ReturnCode_t error;

	if (aborting)
		return;

	if (!pause_traffic) {
		strncpy (data.color, s->color, strlen (s->color) + 1);
		shape_move (s);
		if (cur_screen == S_Main) {
			tty_gotoxy (21, s->row);
			tty_attr_reset ();
			tty_printf ("%3ld %3ld", s->cur_x, s->cur_y);
			tty_gotoxy (CX_HOME, 1);
		}
		data.x = s->cur_x;
		data.y = s->cur_y;
		data.shapesize = s->size;
		error = DDS_DataWriter_write (wp->w, &data, s->h);
		if (error)
			fatal ("Can't write shape data (%s)!", DDS_error (rc));

		if (cur_screen == S_Debug && trace)
			shape_trace ('W', s->h, s->type, &data, SM_UPDATE);

		if (nsteps && nsteps != ~0 && !--nsteps)
			pause_traffic = 1;
	}
	DDS_Timer_start (s->timer, s->delay, user, shape_timeout);
}

#endif

static void shape_topic_create (SHAPE_TYPE t,
		                int        filter,
		                unsigned   x1,
		                unsigned   x2,
		                unsigned   y1,
		                unsigned   y2)
{
	static const char *expression = "x > %0 and x < %1 and y > %2 and y < %3";
	static const char *topic_names [] = {
		"demo.shapes.square.shapedata",
		"demo.shapes.circle.shapedata",
		"demo.shapes.triangle.shapedata",
	};
	char name [60];
	char spars [4][20];
	char *sp;
	unsigned i;
	DDS_StringSeq parameters;

	topic [t] = DDS_DomainParticipant_create_topic (part, topic_names [t], topic_names [t],
									NULL, NULL, 0);

	/* Create Topic. */
	if (!topic [t])
		fatal ("DDS_DomainParticipant_create_topic ('%s') failed!", topic_names [t]);

	if (verbose)
		printf ("DDS Topic (%s) created.\r\n", topic_names [t]);

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
		strncpy (name, topic_names [t], sizeof (name));

	/* Create Topic Description. */
	topic_desc [t] = DDS_DomainParticipant_lookup_topicdescription (part, name);
	if (!topic_desc [t]) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for topic (%s)'!", topic_names [t]);
	}
}

# if 0
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
#ifdef USE_TRANSIENT_LOCAL
		wr_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
#endif
		wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

#ifdef TRACE_DATA
		rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
		/* Create a Data Writer. */
		wp->w = DDS_Publisher_create_datawriter (pub, topic [s->type], &wr_qos, NULL, 0);
		if (!wp->w) {
			fatal ("Unable to create writer (%s)", types_str [s->type]);
			DDS_DomainParticipantFactory_delete_participant (part);
		}
		if (verbose)
			printf ("DDS Writer (%s) created.\r\n", types_str [s->type]);
	}
	strncpy (data.color, s->color, sizeof (data.color));
	s->h = DDS_DataWriter_register_instance (wp->w, &data);
	shape_timeout ((uintptr_t) s);
}
# endif

typedef enum {
	DT_Erase,
	DT_Blink,
	DT_Normal,
	DT_Dead
} DrawType_t;

#define SHAPE_BG	TC_Black
/*#define SHAPE_BG	TC_White*/

void draw_shape (SHAPE_TYPE st,
		 Color_t    color,
		 unsigned   x,
		 unsigned   y,
		 DrawType_t dt)
{
	unsigned	cx, cy;
	static char	shape_type_ch [] = "SCT";

	cx = x /*1 + (x / 7)*/;
	cy = y /*1 + (y / 14)*/;
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
			tty_color (color, SHAPE_BG);
			break;
		case DT_Normal:
			tty_color (color, SHAPE_BG);
			break;
		case DT_Dead:
			tty_attr_set (TA_Dim | TA_Underline);
			tty_color (color, SHAPE_BG);
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
	sp->color = s->data.color;
	sp->c = s->data.color;
	sp->type = t;
	sp->cur_x = s->data.coords.x;
	sp->cur_y = s->data.coords.y;
	sp->delta_x = 0;
	sp->size = s->data.size;
	sp->h = h;
	sp->next = readers [t].instances;
	readers [t].instances = sp;
	snprintf (buf, sizeof (buf), "S:%s(%s)", types_str [t], tty_color_names [sp->c]);
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
	sp->cur_x = np->data.coords.x;
	sp->cur_y = np->data.coords.y;
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
				data.data.color = sp->color;
				shape_trace ('R', sp->h, sp->type, &data, SM_DELETE);
			}
			free (sp);
			break;
		}
}

void shape_read (SHAPE_TYPE t, DDS_DataReader dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	ShapeType_t		*sample;
	DDS_ReturnCode_t	error;
	SHAPE			*sp;

	/*printf ("shape_read(%s): got notification!\r\n", types_str [t]);*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				printf ("Unable to read samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (info->instance_state == DDS_ALIVE_INSTANCE_STATE) {
				if (info->view_state == DDS_NEW_VIEW_STATE)
					shape_new (t, sample, info->instance_handle);
				else {
					sp = shape_lookup (t, info->instance_handle);
					shape_update (sp, sample);
				}
			}
			else {
				sp = shape_lookup (t, info->instance_handle);
				shape_delete (sp);
			} 
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
#ifdef USE_TRANSIENT_LOCAL
	rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
#endif
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

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
		if (tty_gets (sizeof(input), input, 1, 1))
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

# if 0
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
# endif

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
	tty_printf ("Shapes demo (c) 2011, Technicolor");

	/* Display main commands. */
	tty_gotoxy (37, 1);
	tty_attr_reset ();
# if 0
	print_option ('P', "ublish");
# endif
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
				tty_printf ("%ld", sp->color);
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
	printf (">");
	fflush (stdout);
}
#endif

int main (int argc, const char *argv [])
{
	DDS_PoolConstraints	reqs;
#if 0
	SHAPE_WRITER		*wp;
	SHAPE			*sp;
#endif
	SHAPE_TYPE		t;
	char			ch;
	int			error;

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
	DDS_entity_name ("Technicolor Rebus Shapes");
	DDS_Debug_abort_enable (&aborting);
	DDS_Debug_control_enable (&pause_traffic, &nsteps, NULL);
	DDS_Debug_menu_enable (&menu_screen);

#ifdef TRACE_DISC
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						DOMAIN_ID, NULL, NULL, 0);
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
		fatal ("DDS_DomainParticipant_register_type ('%s') failed (%s)!\r\n", "ShapeType", DDS_error (error));
	}
	if (verbose)
		printf ("DDS Topic type ('%s') registered.\r\n", "ShapeType");

	for (t = ST_SQUARE; t <= ST_TRIANGLE; t++) {
		if ((subscriptions & (1 << t)) != 0)
			reader_activate (t, 0, 0, 0, 0, 0);
# if 0
		if ((wp = &writers [t]) != NULL)
			for (sp = wp->instances; sp; sp = sp->next)
				shape_activate (sp);
# endif
	}
	tty_init ();
	init_background ();
	draw_rectangle (0, 0, MAIN_W - 1, MAIN_H - 1, 0);
#ifndef DDS_DEBUG
	DDS_Handle_attach (tty_stdin,
			   POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL,
			   tty_input,
			   NULL);
#else
	DDS_Debug_start_fct (tty_stdin, tty_input);
	cli_mode = debug;
	if (!debug)
#endif
		draw_main_screen ();
#ifndef DDS_DEBUG
	else
		draw_debug_screen ();
#endif
	while (!aborting) {
		ch = tty_getch ();
		if (!debug) {
			switch (ch) {
# if 0
				case 'p':
				case 'P':
					publish_menu ();
					draw_main_screen ();
					break;
# endif
				case 's':
				case 'S':
					subscribe_menu ();
					draw_main_screen ();
					break;
#ifndef DDS_DEBUG
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
		}
#ifndef DDS_DEBUG
		else if (aborting && !cli_mode) {
			aborting = 0;
			debug = 0;
			draw_main_screen ();
		}
#endif
	}
#if 0
	else {
		cur_screen = S_Debug;
		while (!aborting)
			ch = tty_getch ();
	}
#endif
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

	return (0);
}

