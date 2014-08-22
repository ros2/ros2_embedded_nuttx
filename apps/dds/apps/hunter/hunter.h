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

#ifndef __hunter_h_
#define __hunter_h_

/*BEGIN INCLUDES*/

#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#endif
#include "tty.h"
#include "dds/dds_aux.h"
#ifdef XTYPES_USED
#include "dds/dds_dreader.h"
#endif

/*END INCLUDES*/
/*BEGIN DEFINES*/

/*Hunter line of sight*/
#define HLOS 40
/*Hunter kill line of sight*/
#define HKLOS 6
/*Rabbit line of sight*/
#define RLOS 15
/*rabbit mating distance*/
#define RMD 5
/*rabbit speed*/
#define RSPEED 5

/*#define TRACE_DISC	** Define to trace discovery endpoints. */
/*#define TRACE_DATA	** Define to trace data endpoints. */
/*#define TRACE_DIED	** Define this to print players that died. */

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

#define	INC_ARG() if (!*cp) { i++; cp = argv [i]; }

#define	NPLAYERTYPES	2
#define	NMSGS	4
#define	CX_HOME	74

/*END DEFINES*/
/*BEGIN ENUMS*/

typedef enum {
	ST_HUNTER,
	ST_RABBIT
} PLAYER_TYPE;

typedef enum {
	S_Init,
	S_Main,
	S_Publisher,
	S_Subscriber,
	S_Debug
} Screen_t;

typedef enum {
	PM_NEW,
	PM_UPDATE,
	PM_DELETE,
	PM_NO_WRITERS
} PLAYER_MODE;

typedef enum {
	DT_Erase,
	DT_Blink,
	DT_Normal,
	DT_Dead
} DrawType_t;

/*END ENUMS*/
/*BEGIN STRUCTS*/

typedef struct player_st PLAYER;
struct player_st {
	PLAYER		*next;
	PLAYER_TYPE	type;
	char		color [128];
	Color_t		c;
	long		cur_x;
	long		cur_y;
	long		delta_x;
	long		delta_y;
	unsigned	delay;
	int		power;
	int		gender;
	DDS_HANDLE	h;
	unsigned	row;
	DDS_Timer	timer;
};


typedef struct player_writer_st {
	DDS_DataWriter	w;
	PLAYER		*instances;
} PLAYER_WRITER;

typedef struct player_reader_st {
	DDS_DataReader	r;
	PLAYER		*instances;
} PLAYER_READER;

typedef struct player_type_st {
	char		color [128];
	int		x;
	int		y;
	int		gender;
	int		power;
} PlayerType_t;

/*END STRUCT*/
/*BEGIN FUNCTION PROTOTYPE*/

int color_menu (Color_t *color);
int do_switches (int argc, const char **argv);
void draw (unsigned x, unsigned y, char c);
void draw_background (void);
void draw_debug_screen (void);
void draw_main_screen (void);
void draw_player (PLAYER_TYPE st, Color_t    color, unsigned   x, unsigned   y, DrawType_t dt);
void draw_rectangle (unsigned sx, unsigned sy, unsigned ex, unsigned ey, int dashed);
int get_num (const char **cpp, unsigned *num, unsigned min, unsigned max);
int get_str (const char **cpp, const char **name);
void init_background (void);
PLAYER *player_add (PLAYER_TYPE t, const char *color);
void player_alive_timeout (uintptr_t user);
void player_delete (PLAYER *sp);
PLAYER *player_lookup (PLAYER_TYPE t, DDS_HANDLE h);
void player_new (PLAYER_TYPE t, PlayerType_t *s, DDS_HANDLE h);
void player_no_writers (PLAYER *sp);
void player_read (PLAYER_TYPE t, DDS_DataReader dr);
PLAYER_TYPE player_type (const char *cp);
int player_type_menu (PLAYER_TYPE *type);
void player_update (PLAYER *sp, PlayerType_t *np);
void players_cleanup (void);
void print_option (char c, char *s);
void publish_menu (void);
void read_hunter (DDS_DataReaderListener *l, DDS_DataReader dr);
void read_rabbit (DDS_DataReaderListener *l, DDS_DataReader dr);
void reader_activate (PLAYER_TYPE t, int filter, unsigned x1, unsigned x2, unsigned y1, unsigned y2);
DDS_ReturnCode_t register_PlayerType_type (DDS_DomainParticipant participant);
void subscribe_menu (void);
void unregister_PlayerType_type (DDS_DomainParticipant participant);
void usage (void);

/*END FUNCTION PROTOTYPE*/

#endif
