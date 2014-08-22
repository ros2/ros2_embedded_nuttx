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

/* tty.h -- Simple VT100 TTY support functions. */

#ifndef __tty_h_
#define	__tty_h_

#include "sock.h"

extern HANDLE tty_stdin;
extern unsigned tty_width, tty_height;

void tty_init (void);

/* Initialize the terminal.  Sets Raw mode and installs an exit handler to
   restore the mode on exit. */

int tty_read (char *buf, size_t buf_size);

/* Read a number of characters from the tty. */

#ifndef __GNUC__
#define	__attribute__(x)	/*NOTHING*/
#endif

void tty_printf (const char *format, ...)
	__attribute__((format(printf, 1, 2)));

/* Formatted output to TTY function. */

void tty_erase_screen (void);

/* Clear the screen. */

void tty_erase_line (void);

/* Erase complete line. */

void tty_erase_eol (void);

/* Erase to the end of the line. */

void tty_erase_sol (void);

/* Erase to the start of the line. */

void tty_erase_eos (void);

/* Erase till the end of the screen. */

void tty_erase_sos (void);

/* Erase till the start of the screen. */

void tty_gotoxy (unsigned x, unsigned y);

/* Move the cursor to the given X and Y position on the screen. */

/* Attribute definitions: */
typedef enum {
	TA_Bright = 1,
	TA_Dim = 2,
	TA_Underline = 4,
	TA_Blink = 8,
	TA_Reverse = 16
} Attrib_t;

void tty_attr_reset ();

void tty_attr_set (unsigned attr);

void tty_attr_on (unsigned attr);

void tty_attr_off (unsigned attr);

typedef enum {
	TC_Black,
	TC_Red,
	TC_Green,
	TC_Yellow,
	TC_Blue,
	TC_Magenta,
	TC_Cyan,
	TC_White
} Color_t;

extern const char *tty_color_names [];

void tty_color (Color_t fg, Color_t bg);

/* Set the color. */

Color_t tty_color_type (const char *s);

/* Return the color type from the color name. */

void tty_line_drawing (int on);

/* Set/reset line-drawing mode. */

typedef enum {
	TC_UpperLeft,
	TC_UpperRight,
	TC_LowerLeft,
	TC_LowerRight
} Corner_t;

extern const char *tty_corners;

typedef enum {
	TB_Top,
	TB_Left,
	TB_Right,
	TB_Bottom
} Border_t;

extern const char *tty_borders;


void tty_save_cursor (void);

/* Save the current cursor position. */

void tty_restore_cursor (void);

/* Restore the cursor position. */

void tty_save_cursor_attr (void);

/* Save the current cursor position and attributes. */

void tty_restore_cursor_attr (void);

/* Restore the cursor position and attributes. */


void tty_input (HANDLE fd, short events, void *udata);

/* Utility function to queue characters in an input buffer. */

int tty_getch (void);

/* Attempts to read a single character from the input buffer.  Blocks until
   a character is available. */

int tty_gets (size_t nchars, char buf [], int number, int echo);

/* Attempts to read a complete line from the input buffer.  Blocks until an
   end-of-line is entered.  If the number argument is set, only numeric
   characters are accepted.  If the echo argument is set to '*' then every
   character will be echoed with '*', else if echo is non-0, all characters
   will be echoed normally. The function returns a 0-terminated string. */

#endif /* !__tty_h_ */

