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

/* main.c -- Test program for the Players functionality. */

/*BEGIN INCLUDES*/

#include "hunter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <poll.h>
#endif
#include "tty.h"
#include "random.h"
#include "libx.h"
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "dds/dds_aux.h"
#ifdef XTYPES_USED
#include "dds/dds_xtypes.h"
#include "dds/dds_dwriter.h"
#include "dds/dds_dreader.h"
#endif

/*END INCLUDES*/
/*BEGIN GLOBAL VARIABLES*/

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
unsigned		 domain_id;		/* Domain. */
#ifdef XTYPES_USED
int			 dyn_type;		/* Use dynamic Type functions. */
int			 dyn_data;		/* Use dynamic Data functions. */
DDS_DynamicType		 dtype;			/* Dynamic type. */
#endif
DDS_Topic		 topic [NPLAYERTYPES];	/* Topic type. */
DDS_ContentFilteredTopic ftopic [NPLAYERTYPES];	/* Filtered topic type. */
DDS_TopicDescription	 topic_desc [NPLAYERTYPES];/* Topic description. */
PLAYER_WRITER		 writers [NPLAYERTYPES];	/* Active published players. */
PLAYER_READER		 readers [NPLAYERTYPES];	/* Player listeners. */
unsigned		 subscriptions;		/* Player subscriptions. */
DDS_Publisher		 publisher;
DDS_Subscriber		 subscriber;
DDS_DomainParticipant	 participant;
Screen_t		 cur_screen = S_Init;
char			 background [MAIN_H][MAIN_W + 1];
int			 pause_traffic = 0;
int			 menu_screen;
unsigned		 nsteps = ~0;

static DDS_TypeSupport_meta player_tsm [] = {
	{ CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "PlayerType", sizeof (struct player_type_st), 0, 5, 0, NULL },
	{ CDR_TYPECODE_CSTRING,TSMFLAG_KEY, "color", 128, offsetof (struct player_type_st, color), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "x", 0, offsetof (struct player_type_st, x), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "y", 0, offsetof (struct player_type_st, y), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "gender", 0, offsetof (struct player_type_st, gender), 0, 0, NULL},
	{ CDR_TYPECODE_LONG,   0, "power", 0, offsetof (struct player_type_st, power), 0, 0, NULL}
};

static DDS_TypeSupport player_ts;

const char *player_type_str [] = { "Hunter", "Rabbit" };
const char *player_color_str [] = {
	"Black", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White"
};

static DDS_DataReaderListener r_listener_hunter = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	NULL,		/* Requested incompatible QoS. */
	read_hunter,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

static DDS_DataReaderListener r_listener_rabbit = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	NULL,		/* Requested incompatible QoS. */
	read_rabbit,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

static DDS_DataReaderListener *r_listeners [] = {
	&r_listener_hunter,
	&r_listener_rabbit
};

/*END GLOBAL VARIABLES*/
/*BEGIN FUNCTION PROTOTYPE*/

static void player_shoot(PLAYER *hunter,PLAYER *rabbit);
static void player_timeout (uintptr_t user);
static void player_topic_create (PLAYER_TYPE t, int filter, unsigned x1, unsigned x2, unsigned y1, unsigned y2);
static void player_trace (char type, DDS_HANDLE  h, PLAYER_TYPE t, PlayerType_t *st, PLAYER_MODE m);
static void rabbit_mate(PLAYER*, PLAYER*);
static void player_move (PLAYER *player);
static void player_activate (PLAYER *s);

/*END FUNCTION PROTOTYPE*/

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "players -- simple non-graphical players demo program.\r\n");
	fprintf (stderr, "Usage: players [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -p <player> <color> Publish a player (square, circle or triangle).\r\n");
	fprintf (stderr, "   -s <player>         Subscribe on a player (idem).\r\n");
	fprintf (stderr, "   -x [<strength>]    Exclusive ownership with given strength (number).\r\n");
#ifdef DDS_DEBUG
	fprintf (stderr, "   -d                 Start in debug mode.\r\n");
#endif
	fprintf (stderr, "   -i <domain>        Domain Identifier.\r\n");
	fprintf (stderr, "   -t                 Trace transmitted/received info.\r\n");
	fprintf (stderr, "   -b                 Black-on-white display i.o. White-on-black.\r\n");
#ifdef XTYPES_USED
	fprintf (stderr, "   -y                 Use Dynamic Type/Data functions.\r\n");
#endif
	fprintf (stderr, "   -v                 Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv                Extra verbose: log detailed functionality.\r\n");
	exit (1);
}

/* player_add -- Add a player to the list of published players. */

PLAYER *player_add (PLAYER_TYPE t, const char *color)
{
	PLAYER		*player;
	unsigned	i;
	char		buf [20];

	for (player = writers [t].instances; player; player = player->next)
		if (!astrcmp (player->color, color)) {
			printf ("Player already exists! (%d)\r\n", __LINE__);
			return (NULL);
		}
	player = (PLAYER *) malloc (sizeof (PLAYER));
	if (!player) {
		printf ("Out of memory!(%d)\r\n", __LINE__);
		usage ();
	}
	strncpy (player->color, color, sizeof (player->color));
	player->c = tty_color_type (color);
	for (i = 0; i < strlen (color); i++)
		if (player->color [i] >= 'a' && player->color [i] <= 'z')
			player->color [i] -= 'a' - 'A';
	player->type = t;
	player->cur_x = rand() % (MAX_X - MIN_X) + MIN_X;
	player->cur_y = rand() % (MAX_Y - MIN_Y) + MIN_Y;
	/*difference between rabbit and hunter*/
	if (t == ST_HUNTER) {
		player->delta_x = 1 + rand() % 5;
		player->delta_y = 1 + rand() % 5;
		/*shooting power*/
		player->power = 1 + rand() % 20;
	}
	else {
		player->delta_x = RSPEED;
		player->delta_y = RSPEED;
		/*life bar*/
		player->power = 100;
	}
	player->delay = 150;
	player->gender = rand() % 2;
	
	snprintf (buf, sizeof (buf), "P:%s(%s)", player_type_str [t], color);
	player->timer = DDS_Timer_create (buf);

	player->next = writers [t].instances;
	writers [t].instances = player;

	return (player);
}

PLAYER_TYPE player_type (const char *cp)
{
	PLAYER_TYPE	player_t;

	if (!astrcmp (cp, "hunter"))
		player_t = ST_HUNTER;
	else if (!astrcmp (cp, "rabbit"))
		player_t = ST_RABBIT;
	else {
		player_t = ST_HUNTER;
		printf ("Invalid player type (only hunter or rabbit allowed)!(%d)\r\n", __LINE__);
		usage ();
	}
	return (player_t);
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

/* do_switches -- Command line switch decoder. */

int do_switches (int argc, const char **argv)
{
	int		i;
	const char	*cp, *color, *type;
	PLAYER_TYPE	t;

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
						printf ("player type expected!(%d)\r\n", __LINE__);
						usage ();
					}
					t = player_type (type);
					INC_ARG();
					if (!get_str (&cp, &color)) {
						printf ("color expected!(%d)\r\n", __LINE__);
						usage ();
					}
					player_add (t, color);
					break;
				case 's':
					INC_ARG ();
					if (!get_str (&cp, &type)) {
						printf ("player type expected!(%d)\r\n", __LINE__);
						usage ();
					}
					t = player_type (type);
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

DDS_ReturnCode_t register_PlayerType_type (DDS_DomainParticipant participant)
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
		desc->name = "PlayerType";
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
			
		md->name = "gender";
		md->index = md->id = 3;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
		if (!md->type)
			return (DDS_RETCODE_OUT_OF_RESOURCES);
			
		error = DDS_DynamicTypeBuilder_add_member (sb, md);
		if (error)
			return (error);
			
		md->name = "power";
		md->index = md->id = 4;
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
		player_ts = (DDS_TypeSupport) DDS_DynamicTypeSupport_create_type_support (s);

		DDS_DynamicTypeBuilderFactory_delete_type (ss);

		DDS_TypeDescriptor__free (desc);
		DDS_MemberDescriptor__free (md);
		DDS_AnnotationDescriptor__free (key_ad);
		DDS_DynamicTypeMember__free (dtm);
	}
	else
#endif
		player_ts = DDS_DynamicType_register (player_tsm);

       	if (!player_ts)
               	return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (participant, player_ts, "PlayerType");
	return (error);
}

void unregister_PlayerType_type (DDS_DomainParticipant participant)
{
	DDS_DomainParticipant_unregister_type (participant, player_ts, "PlayerType");
#ifdef XTYPES_USED
	if (dyn_type)
		DDS_DynamicTypeBuilderFactory_delete_type (dtype);
#endif
}

static void player_trace (char        type,
			 DDS_HANDLE  h,
			 PLAYER_TYPE  t,
			 PlayerType_t *st,
			 PLAYER_MODE  m)
{
	printf ("%c [%u] - %s(%s): ", type, h, player_type_str [t], st->color);
	if (m == PM_DELETE)
		printf ("<player deleted>(%d)\r\n", __LINE__);
	else if (m == PM_NO_WRITERS)
		printf ("<last writer gone>(%d)\r\n", __LINE__);
	else {
		printf ("X=%d, Y=%d", st->x, st->y);
		if (m == PM_NEW)
			printf (" <new player>(%d)\r\n", __LINE__);
	}
	printf ("\r\n");
}

static void player_shoot(PLAYER *hunter,PLAYER *rabbit)
{
	
	/*reduce life based on shooting power*/
	rabbit->power = rabbit->power - hunter->power;
	
	/*check if dead*/
	if (rabbit->power <= 0){
		player_delete(rabbit);
		return;
	}
	
	/*slow wounded rabbit down*/
	if (rabbit->delta_x > 0)
		rabbit->delta_x = (RSPEED * rabbit->power / 100) +1;
	else
		rabbit->delta_x = -(RSPEED * rabbit->power / 100) -1;
	if (rabbit->delta_y > 0)
		rabbit->delta_y = (RSPEED * rabbit->power / 100) +1;
	else
		rabbit->delta_y = -(RSPEED * rabbit->power / 100) -1;
	
	/*teleport rabbit after being hit*/
	rabbit->cur_x = rand() % (MAX_X - MIN_X) + MIN_X;
	rabbit->cur_y = rand() % (MAX_Y - MIN_Y) + MIN_Y;
}

static void player_move (PLAYER *player)
{
	PLAYER* hunter;
	PLAYER* rabbit;
	PLAYER* nextRabbit;
	
	/*This is the standard movement*/
	player->cur_x += player->delta_x;
	if (player->delta_x < 0 && player->cur_x < MIN_X) {
		player->cur_x = MIN_X;
		player->delta_x = -player->delta_x;
	}
	else if (player->delta_x > 0 && player->cur_x > MAX_X) {
		player->cur_x = MAX_X;
		player->delta_x = -player->delta_x;
	}
	player->cur_y += player->delta_y;
	if (player->delta_y < 0 && player->cur_y < MIN_Y) {
		player->cur_y = MIN_Y;
		player->delta_y = -player->delta_y;
	}
	else if (player->delta_y > 0 && player->cur_y > MAX_Y) {
		player->cur_y = MAX_Y;
		player->delta_y = -player->delta_y;
	}
	/*TODO: when a hunter sees a rabbit, move towards the rabbit*/
	if(player->type == ST_HUNTER){
		for (rabbit = writers [ST_RABBIT].instances; rabbit; rabbit = nextRabbit){
			nextRabbit = rabbit->next;
			/*check if rabbit is close enough within hunter line of sight*/
			if ((player->cur_x-HLOS) < rabbit->cur_x &&
			rabbit->cur_x < (player->cur_x+HLOS) && 
			(player->cur_y-HLOS) < rabbit->cur_y &&
			rabbit->cur_y < (player->cur_y+HLOS)){
				/*move towards rabbit*/
				/*X-movement*/
				if (rabbit->cur_x < player->cur_x)
					if(player->delta_x > 0) player->delta_x = -player->delta_x;
				if (rabbit->cur_x > player->cur_x)
					if(player->delta_x < 0) player->delta_x = -player->delta_x;
				/*Y-movement*/
				if (rabbit->cur_y < player->cur_y)
					if(player->delta_y > 0) player->delta_y = -player->delta_y;
				if (rabbit->cur_y > player->cur_y)
					if(player->delta_y < 0) player->delta_y = -player->delta_y;
			}
			if ((player->cur_x-HKLOS) < rabbit->cur_x &&
			rabbit->cur_x < (player->cur_x+HKLOS) && 
			(player->cur_y-HKLOS) < rabbit->cur_y &&
			rabbit->cur_y < (player->cur_y+HKLOS)){
				/*Kill the rabbit*/
				player_shoot(player, rabbit);
			}
		}
	}
	else if(player->type == ST_RABBIT){
		/*When a rabbit sees a hunter, move away from the hunter*/
		for (hunter = writers [ST_HUNTER].instances; hunter; hunter = hunter->next){
			/*check if hunter is close enough within rabbit line of sight*/
			if ((player->cur_x-RLOS) < hunter->cur_x &&
			hunter->cur_x < (player->cur_x+RLOS) &&
			(player->cur_y-RLOS) < hunter->cur_y &&
			hunter->cur_y < (player->cur_y+RLOS)){
				/*move away from hunter*/
				if (hunter->cur_x < player->cur_x)
					if(!(player->delta_x > 0)) player->delta_x = -player->delta_x;
				if (hunter->cur_x > player->cur_x)
					if(!(player->delta_x < 0)) player->delta_x = -player->delta_x;
				/*Y-movement*/
				if (hunter->cur_y < player->cur_y)
					if(!(player->delta_y > 0)) player->delta_y = -player->delta_y;
				if (hunter->cur_y > player->cur_y)
					if(!(player->delta_y < 0)) player->delta_y = -player->delta_y;
			}
		}
		/*when a rabbit sees a rabbit -> mate*/
		for (rabbit = writers [ST_RABBIT].instances; rabbit; rabbit = rabbit->next){
			if(player != rabbit){
				/*check if hunter is close enough within rabbit line of sight*/
				if ((player->cur_x-RMD) < rabbit->cur_x &&
				rabbit->cur_x < (player->cur_x+RMD) &&
				(player->cur_y-RMD) < rabbit->cur_y &&
				rabbit->cur_y < (player->cur_y+RMD)){
					/*create new rabbit*/
					rabbit_mate(player, rabbit);
				}
			}
		}
	}
}

static void player_timeout (uintptr_t user)
{
	PLAYER		*s = (PLAYER *) user;
	PLAYER_WRITER	*wp = &writers [s->type];
	PlayerType_t	data;
	DDS_ReturnCode_t rc;
#ifdef XTYPES_USED
	DDS_DynamicData	dd;
	DDS_MemberId	id;
#endif

	if (aborting)
		return;

	if (!pause_traffic) {
		player_move (s);
		if (cur_screen == S_Main) {
			tty_gotoxy (21, s->row);
			tty_attr_reset ();
			tty_printf ("%3ld %3ld %3d %1d", s->delta_x, s->delta_y, s->power, s->gender);
			tty_gotoxy (CX_HOME, 1);
		}
#ifdef XTYPES_USED
		if (dyn_data) {
			dd = DDS_DynamicDataFactory_create_data (dtype);
			if (!dd)
				fatal ("player_timeout: Can't create dynamic data!(%d)\r\n", __LINE__);

			id = DDS_DynamicData_get_member_id_by_name (dd, "color");
			rc = DDS_DynamicData_set_string_value (dd, id, s->color);
			if (rc)
				fatal ("Can't add data member(color) (%s)!(%d)\r\n", DDS_error (rc), __LINE__);

			id = DDS_DynamicData_get_member_id_by_name (dd, "x");
			rc = DDS_DynamicData_set_int32_value (dd, id, s->cur_x);
			if (rc)
				fatal ("Can't add data member(x) (%s)!(%d)\r\n", DDS_error (rc), __LINE__);


			id = DDS_DynamicData_get_member_id_by_name (dd, "y");
			rc = DDS_DynamicData_set_int32_value (dd, id, s->cur_y);
			if (rc)
				fatal ("Can't add data member(y) (%s)!(%d)\r\n", DDS_error (rc), __LINE__);

			rc = DDS_DynamicDataWriter_write (wp->w, dd, s->h);
			DDS_DynamicDataFactory_delete_data (dd);
		}
		else {
#endif
			strncpy (data.color, s->color, sizeof (data.color));
			data.x = s->cur_x;
			data.y = s->cur_y;
			rc = DDS_DataWriter_write (wp->w, &data, s->h);
#ifdef XTYPES_USED
		}
#endif
		if (rc)
			fatal ("Can't write player data (%s)!(%d)\r\n", DDS_error (rc), __LINE__);

		if (nsteps && nsteps != ~0U && !--nsteps)
			pause_traffic = 1;

		if (cur_screen == S_Debug && trace)
			player_trace ('W', s->h, s->type, &data, PM_UPDATE);
	}
	DDS_Timer_start (s->timer, s->delay, user, player_timeout);
}

static void player_topic_create (PLAYER_TYPE t,
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

	topic [t] = DDS_DomainParticipant_create_topic (participant, player_type_str [t], "PlayerType",
									NULL, NULL, 0);

	/* Create Topic. */
	if (!topic [t])
		fatal ("DDS_DomainParticipant_create_topic () failed!(%d)\r\n", __LINE__);

	if (verbose)
		printf ("DDS Topic (%s) created.\r\n(%d)\r\n", player_type_str [t], __LINE__);

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
				participant, name, topic [t], expression, &parameters);
		if (!ftopic [t])
			fatal ("Couldn't create content-filtered topic!(%d)\r\n", __LINE__);
	}
	else
		strncpy (name, player_type_str [t], sizeof (name));

	/* Create Topic Description. */
	topic_desc [t] = DDS_DomainParticipant_lookup_topicdescription (participant, name);
	if (!topic_desc [t]) {
		DDS_DomainParticipantFactory_delete_participant (participant);
		fatal ("Unable to create topic description for topic!(%d)\r\n", __LINE__);
	}
}

static void player_activate (PLAYER *s)
{
	DDS_DataWriterQos 	wr_qos;
	PLAYER_WRITER		*wp = &writers [s->type];
	PlayerType_t		data;

	if (!wp->w) {
		if (!publisher) {	/* Create a publisher. */
			publisher = DDS_DomainParticipant_create_publisher (participant, NULL, NULL, 0);
			if (!publisher)
				fatal ("DDS_DomainParticipant_create_publisher () failed!(%d)\r\n", __LINE__);

			if (verbose)
				printf ("DDS Publisher created.(%d)\r\n", __LINE__);
		}
		if (!topic [s->type])
			player_topic_create (s->type, 0, 0, 0, 0, 0);

		/* Setup writer QoS parameters. */
		DDS_Publisher_get_default_datawriter_qos (publisher, &wr_qos);
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
		wp->w = DDS_Publisher_create_datawriter (publisher, topic [s->type], &wr_qos, NULL, 0);
		if (!wp->w) {
			fatal ("Unable to create writer(%d)\r\n", __LINE__);
			DDS_DomainParticipantFactory_delete_participant (participant);
		}
		if (verbose)
			printf ("DDS Writer (%s) created.(%d)\r\n", player_type_str [s->type], __LINE__);
	}
	strncpy (data.color, s->color, sizeof (data.color));
	s->h = DDS_DataWriter_register_instance (wp->w, &data);
	player_timeout ((uintptr_t) s);
}

void draw_player (PLAYER_TYPE st,
		 Color_t    color,
		 unsigned   x,
		 unsigned   y,
		 DrawType_t dt)
{
	unsigned	cx, cy;
	Color_t		bg = (white_bg) ? TC_White : TC_Black;
	static char	player_type_ch [] = "HR";

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
	tty_printf ("%c", player_type_ch [st]);
	tty_attr_reset ();
	tty_gotoxy (CX_HOME, 1);
}

void player_alive_timeout (uintptr_t user)
{
	PLAYER	*sp = (PLAYER *) user;

	if (cur_screen == S_Main)
		tty_gotoxy (1, 23);

#ifdef TRACE_DIED
	printf ("Player died: %s (%s)!      (%d)\r\n", player_type_str [sp->type], sp->color, __LINE__);
#endif
	if (cur_screen == S_Main)
		draw_player (sp->type, sp->c, sp->cur_x, sp->cur_y, DT_Dead);
}

void player_new (PLAYER_TYPE t, PlayerType_t *s, DDS_HANDLE h)
{
	PLAYER	*sp;
	char	buf [20];

	sp = (PLAYER *) malloc (sizeof (PLAYER));
	if (!sp) {
		printf ("Out of memory!(%d)\r\n", __LINE__);
		return;
	}
	strncpy (sp->color, s->color, sizeof (sp->color));
	sp->c = tty_color_type (s->color);
	sp->type = t;
	sp->cur_x = s->x;
	sp->cur_y = s->y;
	/*These are not published*/
	/*sp->delta_x = s->delta_x;*/
	/*sp->delta_y = s->delta_y;*/
	sp->gender = s->gender;
	sp->power = s->power;
	sp->h = h;
	sp->next = readers [t].instances;
	readers [t].instances = sp;
	snprintf (buf, sizeof (buf), "S:%s(%s)", player_type_str [t], s->color);
	sp->timer = DDS_Timer_create (buf);
	if (cur_screen == S_Main)
		draw_player (sp->type, sp->c, sp->cur_x, sp->cur_y, DT_Blink);
	else if (cur_screen == S_Debug && trace)
		player_trace ('R', h, t, s, PM_NEW);

	DDS_Timer_start (sp->timer, 5000, (uintptr_t) sp, player_alive_timeout);
}

PLAYER *player_lookup (PLAYER_TYPE t, DDS_HANDLE h)
{
	PLAYER	*sp;

	for (sp = readers [t].instances; sp; sp = sp->next)
		if (sp->h == h)
			return (sp);

	return (NULL);
}

void player_update (PLAYER *sp, PlayerType_t *np)
{
	DrawType_t	dt;

	if (cur_screen == S_Main)
		draw_player (sp->type, sp->c, sp->cur_x, sp->cur_y, DT_Erase);
	sp->cur_x = np->x;
	sp->cur_y = np->y;
	sp->delta_x++;
	if (cur_screen == S_Main) {
		dt = (sp->delta_x < 10) ? DT_Blink : DT_Normal;
		draw_player (sp->type, sp->c, sp->cur_x, sp->cur_y, dt);
	}
	else if (cur_screen == S_Debug && trace)
		player_trace ('R', sp->h, sp->type, np, PM_UPDATE);
	DDS_Timer_start (sp->timer, 5000, (uintptr_t) sp, player_alive_timeout);
}

void player_delete (PLAYER *sp)
{
	PLAYER		*prev_sp, *xsp;
	PlayerType_t	data;

	/*remove writers*/
	for (prev_sp = NULL, xsp = writers [sp->type].instances;
	     xsp;
	     prev_sp = xsp, xsp = xsp->next)
		if (xsp == sp) {
			if (prev_sp)
				prev_sp->next = sp->next;
			else
				writers [sp->type].instances = sp->next;
			if (cur_screen == S_Main)
				draw_player (sp->type, sp->c, sp->cur_x, sp->cur_y, DT_Erase);
			else if (cur_screen == S_Debug && trace) {
				strncpy (data.color, sp->color, sizeof (data.color));
				player_trace ('R', sp->h, sp->type, &data, PM_DELETE);
			}
			DDS_Timer_stop (sp->timer);
			DDS_Timer_delete (sp->timer);
			free (sp);
			break;
		}
}

void player_no_writers (PLAYER *sp)
{
	PLAYER		*xsp;
	PlayerType_t	data;

	for (xsp = readers [sp->type].instances;
	     xsp;
	     xsp = xsp->next)
		if (xsp == sp) {
			if (cur_screen == S_Main)
				draw_player (sp->type, sp->c, sp->cur_x, sp->cur_y, DT_Dead);
			else if (cur_screen == S_Debug && trace) {
				strncpy (data.color, sp->color, sizeof (data.color));
				player_trace ('R', sp->h, sp->type, &data, PM_NO_WRITERS);
			}
			break;
		}
}

void player_read (PLAYER_TYPE t, DDS_DataReader dr)
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
	PlayerType_t		*sample;
	DDS_ReturnCode_t	error;
	PLAYER			*sp;
#ifdef XTYPES_USED
	DDS_DynamicData		dd;
	DDS_MemberId		id;
	PlayerType_t		player;
#endif

	/*printf ("player_read(%s): got notification!\r\n", player_type_str [t]);*/
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
				printf ("Unable to read samples: error = %s!(%d)\r\n", DDS_error (error), __LINE__);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			info = DDS_SEQ_ITEM (rx_info, 0);
#ifdef XTYPES_USED
			if (info->valid_data && dyn_data) {
				dd = DDS_SEQ_ITEM (drx_sample, 0);
				if (!dd)
					fatal ("Empty dynamic sample!(%d)\r\n", __LINE__);

				id = DDS_DynamicData_get_member_id_by_name (dd, "color");
				error = DDS_DynamicData_get_string_value (dd, player.color, id);
				if (error)
					fatal ("Can't get data member(color) (%s)!(%d)", DDS_error (error), __LINE__);

				id = DDS_DynamicData_get_member_id_by_name (dd, "x");
				error = DDS_DynamicData_get_int32_value (dd, &player.x, id);
				if (error)
					fatal ("Can't get data member(x) (%s)! (%d)\r\n", DDS_error (error), __LINE__);

				id = DDS_DynamicData_get_member_id_by_name (dd, "y");
				error = DDS_DynamicData_get_int32_value (dd, &player.y, id);
				if (error)
					fatal ("Can't get data member(y) (%s)!(%d)\r\n", DDS_error (error), __LINE__);

				sample = &player;
			}
			else
#endif
				if (info->valid_data)
					sample = DDS_SEQ_ITEM (rx_sample, 0);
			if (info->instance_state == DDS_ALIVE_INSTANCE_STATE) {
				if (info->view_state == DDS_NEW_VIEW_STATE)
					player_new (t, sample, info->instance_handle);
				else {
					sp = player_lookup (t, info->instance_handle);
					if (sp)
						player_update (sp, sample);
				}
			}
			else {
				sp = player_lookup (t, info->instance_handle);
				if (sp) {
					if (info->instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
						player_delete (sp);
					else
						player_no_writers (sp);
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

void read_hunter (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	ARG_NOT_USED (l)

	player_read (ST_HUNTER, dr);
}

void read_rabbit (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	ARG_NOT_USED (l)

	player_read (ST_RABBIT, dr);
}

void reader_activate (PLAYER_TYPE t,
		      int        filter,
		      unsigned   x1,
		      unsigned   x2,
		      unsigned   y1,
		      unsigned   y2)
{
	DDS_DataReaderQos	rd_qos;

	/* Create a topic */
	if (!topic [t])
		player_topic_create (t, filter, x1, x2, y1, y2);

	if (!subscriber) {
		subscriber = DDS_DomainParticipant_create_subscriber (participant, NULL, NULL, 0); 
		if (!subscriber)
			fatal ("DDS_DomainParticipant_create_subscriber () returned an error!(%d)\r\n", __LINE__);

		if (verbose)
			printf ("DDS Subscriber created.(%d)\r\n", __LINE__);
	}

	DDS_Subscriber_get_default_datareader_qos (subscriber, &rd_qos);
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
	readers [t].r = DDS_Subscriber_create_datareader (subscriber, topic_desc [t], &rd_qos, r_listeners [t], DDS_DATA_AVAILABLE_STATUS);
	if (!readers [t].r)
		fatal ("DDS_DomainParticipant_create_datareader () returned an error!(%d)\r\n", __LINE__);

	if (verbose)
		printf ("DDS Reader (%s) created.(%d)\r\n", player_type_str [t], __LINE__);
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

void players_cleanup (void)
{
	PLAYER_TYPE	t;
	PLAYER		*s;

	for (t = 0; t < NPLAYERTYPES; t++) {
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

int player_type_menu (PLAYER_TYPE *type)
{
	char	ch;

	tty_attr_reset ();
	tty_gotoxy (4, 6);
	tty_printf ("Player type: ");
	tty_gotoxy (8, 8);
	print_option ('H', "unter");
	tty_gotoxy (8, 9);
	print_option ('R', "abbit");
	tty_gotoxy (8, 10);
	tty_printf ("or <esc> to return");
	tty_gotoxy (10, 12);
	tty_printf ("Choice? ");
	for (;;) {
		ch = tty_getch ();
		switch (ch) {
			case 'h':
			case 'H':
				*type = ST_HUNTER;
				break;
			case 'r':
			case 'R':
				*type = ST_RABBIT;
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
	tty_printf ("Player type = %s", player_type_str [*type]);
	tty_erase_eos ();
	return (0);
}

int color_menu (Color_t *color)
{
	char	ch;

	tty_attr_reset ();
	tty_gotoxy (4, 7);
	tty_printf ("Player color:");
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
	tty_printf ("Player color = %s", player_color_str [*color]);
	tty_erase_eos ();
	tty_printf ("\r\n\r\n");
	return (0);
}

void publish_menu (void)
{
	PLAYER		*sp;
	PLAYER_TYPE	type;
	Color_t		color;

	cur_screen = S_Publisher;
	tty_gotoxy (36, 1);
	tty_erase_eos ();
	tty_gotoxy (4, 4);
	tty_attr_set (TA_Underline | TA_Bright);
	tty_printf ("Publish");
	if (player_type_menu (&type))
		return;

	if (color_menu (&color))
		return;

	if ((sp = player_add (type, player_color_str [color])) == NULL)
		return;

	player_activate (sp);
}

void subscribe_menu (void)
{
	PLAYER_TYPE	type;
	unsigned	x1=0, x2=0, y1=0, y2=0;

	cur_screen = S_Subscriber;
	tty_gotoxy (36, 1);
	tty_erase_eos ();
	tty_gotoxy (4, 4);
	tty_attr_set (TA_Underline | TA_Bright);
	tty_printf ("Subscribe");
	if (player_type_menu (&type))
		return;

	if ((subscriptions & (1 << type)) != 0) {
		tty_printf ("Already subscribed to this player!");
		return;
	}
	
	subscriptions |= 1 << type;
	reader_activate (type, 0, x1, x2, y1, y2);
}

void draw_main_screen (void)
{
	PLAYER_WRITER		*wp;
	PLAYER			*sp;
	PLAYER_TYPE		t;
	unsigned		y;

	tty_attr_reset ();
	tty_erase_screen ();
	tty_attr_set (TA_Bright | TA_Reverse);
	tty_printf ("Players demo (c) 2011, Technicolor");

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
	for (t = ST_HUNTER; t <= ST_RABBIT; t++)
		if ((subscriptions & (1 << t)) != 0) {
			tty_gotoxy (4, y++);
			tty_printf ("%s", player_type_str [t]);
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
	for (t = ST_HUNTER; t <= ST_RABBIT; t++)
		if ((wp = &writers [t]) != NULL)
			for (sp = wp->instances; sp; sp = sp->next) {
				sp->row = y++;
				tty_gotoxy (4, sp->row);
				tty_printf ("%s", player_type_str [t]);
				tty_gotoxy (13, sp->row);
				tty_printf ("%s", sp->color);
				tty_gotoxy (21, sp->row);
				tty_printf ("%3ld %3ld %3d %1d", sp->delta_x, sp->delta_y, sp->power, sp->gender);
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
	printf ("Players command shell -- type 'h' or '?' for help.\r\n");
}
#endif

static void rabbit_mate(PLAYER *rabbit1, PLAYER *rabbit2){
	/*Create a new rabbit*/
	PLAYER 		*babyRabbit;
	Color_t		color;
	if (rabbit1->gender != rabbit2->gender)
		for (color = TC_Red; color <= TC_White; color++) 
			if ((babyRabbit = player_add(ST_RABBIT, player_color_str [color])) != NULL){
				player_activate(babyRabbit);
				break;
			}
}

int main (int argc, const char *argv [])
{
	DDS_PoolConstraints	reqs;
	PLAYER_WRITER		*wp;
	PLAYER			*sp;
	PLAYER_TYPE		t;
	char			ch;
	int			error;
#ifdef DDS_DEBUG
	char			command [132];
#endif

	do_switches (argc, argv);

	/*true random number*/
	srand(time(NULL));
		
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
	DDS_entity_name ("Technicolor Hunters");
#ifdef DDS_DEBUG
	DDS_Debug_abort_enable (&aborting);
	DDS_Debug_control_enable (&pause_traffic, &nsteps, NULL);
	DDS_Debug_menu_enable (&menu_screen);
#endif
#ifdef TRACE_DISC
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	/* Create a domain participant. */
	participant = DDS_DomainParticipantFactory_create_participant (
						domain_id, NULL, NULL, 0);
	if (!participant)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");

#ifdef TRACE_DISC
	rtps_dtrace_set (0);
#endif
	if (verbose)
		printf ("DDS Domain Participant created.\r\n");

	/* Register the message topic type. */
	error = register_PlayerType_type (participant);
	if (error) {
		DDS_DomainParticipantFactory_delete_participant (participant);
		fatal ("DDS_DomainParticipant_register_type ('PlayerType') failed (%s)!", DDS_error (error));
	}
	if (verbose)
		printf ("DDS Topic type ('%s') registered.\r\n", "PlayerType");

	for (t = ST_HUNTER; t <= ST_RABBIT; t++) {
		if ((subscriptions & (1 << t)) != 0)
			reader_activate (t, 0, 0, 0, 0, 0);
		if ((wp = &writers [t]) != NULL)
			for (sp = wp->instances; sp; sp = sp->next)
				player_activate (sp);
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
			tty_gets (sizeof (command), command, 0, 1);
			DDS_Debug_command (command);
			if (aborting && (!cli_mode || menu_screen)) {
				aborting = 0;
				debug = 0;
				draw_main_screen ();
			}
		}
#endif
	}
	players_cleanup ();
	unregister_PlayerType_type (participant);

	error = DDS_DomainParticipant_delete_contained_entities (participant);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Entities deleted\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (participant);
	if (error)
		fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Participant deleted\r\n");

	return (0);
}

