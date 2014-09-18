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

/* tty.c -- Implements some simple VT100 Terminal (TTY) access functions. */

#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <termios.h>
#endif
#include <stdarg.h>
#include <stdint.h>
#include <poll.h>
#include "libx.h"
#include "dds/dds_aux.h"
#include "tty.h"


#if defined(NUTTX_RTOS)
#ifndef STDIN_FILENO
#  define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#  define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#  define STDERR_FILENO 2
#endif
#endif

#ifdef _WIN32
static int	new_console;
DWORD		old_tty_mode;
#else
struct termios	old_tty_mode;
#endif

static const char attr_chars [] = "12457";
static unsigned cur_attr;
#ifdef _WIN32
static unsigned tty_start_x;
static unsigned tty_start_y;
static unsigned tty_max_x;
static unsigned tty_max_y;
static unsigned tty_saved_x;
static unsigned tty_saved_y;
static unsigned tty_attribs;
#endif

HANDLE tty_stdin;
HANDLE tty_stdout;
unsigned tty_width;
unsigned tty_height;
unsigned tty_cursor_x;
unsigned tty_cursor_y;
#ifdef TTY_MOUSE_INFO
unsigned tty_mouse_x;
unsigned tty_mouse_y;
#endif

static void stty_restore (void)
{
#ifdef _WIN32
	if (new_console)
		FreeConsole ();

	SetConsoleMode (tty_stdin, old_tty_mode);
#else
#ifndef TTY_NORMAL
	if (tcsetattr (STDIN_FILENO, TCSANOW, &old_tty_mode) < 0)
		perror ("stty_raw::tcsetattr");

	tcflush (STDIN_FILENO, TCIOFLUSH);
#endif
#endif
}

static void stty_raw (void)
{
#ifndef _WIN32
#ifndef TTY_NORMAL
	struct termios tty_mode;
#endif
#endif
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO	info;
/*	unsigned			i; */

	if (AllocConsole ())
		new_console = 1;
	
	tty_stdin = GetStdHandle (STD_INPUT_HANDLE); 
	if (tty_stdin == INVALID_HANDLE_VALUE) {
		printf ("stty_raw: can't get Standard Input!\r\n");
		return;
	}
	tty_stdout = GetStdHandle (STD_OUTPUT_HANDLE); 
	if (tty_stdout == INVALID_HANDLE_VALUE) {
		printf ("stty_raw: can't get Standard Output!\r\n");
		return;
	}
	if (!GetConsoleScreenBufferInfo (tty_stdout, &info)) {
		printf ("stty_raw: can't get Console buffer info!\r\n");
		return;
	}
#if 0
	printf ("Cursor: X=%u, Y=%u\r\n", info.dwCursorPosition.X, info.dwCursorPosition.Y);
	printf ("Max. window size: Width=%u, Height=%u\r\n", info.dwMaximumWindowSize.X, info.dwMaximumWindowSize.Y);
	printf ("Size: Width=%u, Height=%u\r\n", info.dwSize.X, info.dwSize.Y);
	printf ("Window: Bottom=%u, Left=%u, Right=%u, Top=%u\r\n", info.srWindow.Bottom, info.srWindow.Left, info.srWindow.Right, info.srWindow.Top);
	printf ("Attributes: 0x%04x\r\n", info.wAttributes);

	for (i = 0; i < 256; i++) {
		if ((i & 31) == 0)
			printf ("\r\n%3u: ", i);
		printf ("%c", i);
	}
#endif
	if (!GetConsoleMode (tty_stdin, &old_tty_mode)) {
		printf ("stty_raw: can't get Console mode!\r\n");
		return;
	}
	/* Enable the window [and mouse] input events. */
	if (!SetConsoleMode (tty_stdin, ENABLE_PROCESSED_INPUT
					| ENABLE_PROCESSED_OUTPUT
#ifdef TTY_FOCUS_INFO
					| ENABLE_WINDOW_INPUT
#endif
#ifdef TTY_MOUSE_INFO
					| ENABLE_MOUSE_INPUT
#endif
								)) {
		printf ("stty_raw: can't set Console mode!\r\n");
		return;
	}
 
#else
#ifndef TTY_NORMAL
	if (tcgetattr (STDIN_FILENO, &old_tty_mode) < 0)
		perror ("stty_raw::tcgetattr");

	tty_mode = old_tty_mode;
	tty_mode.c_iflag = 0;
	tty_mode.c_oflag = 0;
	tty_mode.c_cflag = CS8 | CREAD | CLOCAL;
	tty_mode.c_lflag = ISIG;
#if !defined (__SVR4) && !defined (__APPLE__)
	tty_mode.c_line = 0;
#endif
	tty_mode.c_cc [VMIN] = 1;
	tty_mode.c_cc [VTIME] = 0;

	if (tcsetattr (STDIN_FILENO, TCSANOW, &tty_mode) < 0)
		perror ("stty_raw::tcsetattr");

	tcflush (STDIN_FILENO, TCIOFLUSH);
#endif
	tty_stdin = STDIN_FILENO;
#endif
}

static void exit_handler (void)
{
        stty_restore ();
}

static void install_atexit_handler (void)
{
	static int exithandler_installed = 0;

	if (!exithandler_installed) {
		atexit (exit_handler);
		exithandler_installed = 1;
	}
}

/* tty_read -- Read a number of characters from the tty. */

int tty_read (char *buf, size_t size)
{
	unsigned	n;
#ifdef _WIN32
	unsigned	i, x;
	DWORD		nevents;
	INPUT_RECORD	input;

	n = 0;
	if (!GetNumberOfConsoleInputEvents (tty_stdin, &nevents)) {
		printf ("tty_read: GetNumberOfConsoleInputEvents() returned error %d.\r\n", GetLastError ());
		return (-1);
	}
	for (i = 0; i < nevents; i++) {
		if (!ReadConsoleInput (tty_stdin, &input, 1, &x)) {
			printf ("tty_read: ReadConsoleInput() returned error %d.\r\n", GetLastError ());
			return (-1);
		}
		switch (input.EventType) {
			case KEY_EVENT: /* Keyboard input */
				if (input.Event.KeyEvent.bKeyDown) {
					/*printf ("<press>");*/
					*buf++ = input.Event.KeyEvent.uChar.AsciiChar;
					if (++n >= size)
						return (n);
				}
				/*else
					printf ("<release>");*/
				break;
			case MOUSE_EVENT: /* mouse input */
#ifdef TTY_MOUSE_INFO
#ifndef MOUSE_HWHEELED
#define MOUSE_HWHEELED 0x0008
#endif
				switch (input.Event.MouseEvent.dwEventFlags) {
					case 0: /* Button pressed. */
						printf ("<mouse_buttons:0x%04x>", 
							input.Event.MouseEvent.dwButtonState);
						break;
					case DOUBLE_CLICK:
						printf ("<mouse_dclick>");
						break;
					case MOUSE_HWHEELED:
						printf ("<mouse_hwheel:%s>",
							(input.Event.MouseEvent.dwButtonState & 0x80000000) ? "left" : "right");
						break;
					case MOUSE_MOVED:
						tty_mouse_x = input.Event.MouseEvent.dwMousePosition.X;
						tty_mouse_y = input.Event.MouseEvent.dwMousePosition.Y;
						printf ("<mouse:X=%u,Y=%u>", tty_mouse_x, tty_mouse_y);
						break;
					case MOUSE_WHEELED:
						printf ("<mouse_vwheel:%s>", 
							(input.Event.MouseEvent.dwButtonState & 0x80000000) ? "up" : "down");
						break;
					default:
						printf ("<mouse_unknown: %d>",
							input.Event.MouseEvent.dwEventFlags);
						break;
				}
				break; 
#endif
			case WINDOW_BUFFER_SIZE_EVENT: /* Screenn buffer resizing. */
				tty_width = input.Event.WindowBufferSizeEvent.dwSize.X;
				tty_height = input.Event.WindowBufferSizeEvent.dwSize.Y;
				/*printf ("<window:%ux%u>", tty_width, tty_height);*/
				break;
			case FOCUS_EVENT: /* disregard focus events */
#ifdef TTY_FOCUS_INFO
				printf ("<focus:%d>", input.Event.FocusEvent.bSetFocus);
#endif
				break;
			case MENU_EVENT: /* disregard menu events */
				printf ("<menu>");
				break;
			default:
				printf ("tty_read: Invalid input event type %d.\r\n", input.EventType);
				break;
		} 
	}
#else
	n = read (tty_stdin, buf, size);
#endif
	return (n);
}

static char sbuf [128];

void tty_printf (const char *fmt, ...)
{
	va_list	arg;
#ifdef _WIN32
	unsigned n;
#endif
	va_start (arg, fmt);
	vsnprintf (sbuf, sizeof (sbuf), fmt, arg);
	va_end (arg);
#ifdef _WIN32
/*	printf ("%s", sbuf);*/
	if (!WriteConsoleA (tty_stdout, sbuf, strlen (sbuf), &n, NULL))
		printf ("tty_printf: WriteConsole() returned error %d", GetLastError ());
#else
	printf ("%s", sbuf);
	fflush (stdout);
#endif
}

static void attr_update (int reset, unsigned attr)
{
	int		sep = 0;
	unsigned	i, m;

	if (reset) {
#ifdef _WIN32
		tty_attribs = 0x0007;
#else
		printf ("\x1b[0m");
		fflush (stdout);
#endif
	}
#ifdef _WIN32
/*	tty_attribs &= ~(FOREGROUND_INTENSITY |
		         BACKGROUND_INTENSITY |
			 COMMON_LVB_UNDERSCORE |
			 COMMON_LVB_REVERSE_VIDEO);*/
	for (i = 0, m = 1; i <= 4; i++, m <<= 1)
		if ((attr & m) != 0)
			switch (m) {
				case TA_Bright:
					tty_attribs |= FOREGROUND_INTENSITY;
					break;
				case TA_Dim:
					tty_attribs |= BACKGROUND_INTENSITY;
					break;
				case TA_Underline:
					tty_attribs |= COMMON_LVB_UNDERSCORE;
					break;
				/*case TA_Blink:
					tty_attribs |= 0x2000;
					break;*/
				case TA_Reverse: {
					unsigned fg, bg;

					/* Ugly hack - bit doesn't seem to work. */

					fg = (tty_attribs & 0xf0) >> 4;
					bg = (tty_attribs & 0x0f) << 4;
					tty_attribs = (tty_attribs & ~0xff) | fg | bg;
					/*tty_attribs |= COMMON_LVB_REVERSE_VIDEO;*/
					}
					break;
				default:
					break;
			}
	/*printf ("A:[0x%x]", tty_attribs);*/
	if (!SetConsoleTextAttribute (tty_stdout, tty_attribs))
		printf ("SetConsoleTextAttribute() returned error: %d", GetLastError());
#else
	if (!attr)
		return;

	printf ("\x1b[");
	for (i = 0, m = 1; i <= 4; i++, m <<= 1)
		if ((attr & m) != 0) {
			if (sep)
				printf (";");
			printf ("%c", attr_chars [i]);
			sep = 1;
		}
	printf ("m");
	fflush (stdout);
#endif
}

void tty_attr_reset (void)
{
	cur_attr = 0;
	attr_update (1, 0);
}

void tty_attr_set (unsigned attr)
{
	cur_attr = attr;
	attr_update (1, attr);
}

void tty_attr_on (unsigned attr)
{
	cur_attr |= attr;
	attr_update (0, attr);
}

void tty_attr_off (unsigned attr)
{
	cur_attr &= ~attr;
	attr_update (1, cur_attr);
}

void tty_color (Color_t fg, Color_t bg)
{
#ifdef _WIN32
	static const unsigned cfg [] = {
/* Black */	0,
/* Red */	FOREGROUND_RED,
/* Green */	FOREGROUND_GREEN,
/* Yellow */	FOREGROUND_RED | FOREGROUND_GREEN,
/* Blue */	FOREGROUND_BLUE,
/* Magenta */	FOREGROUND_BLUE | FOREGROUND_RED,
/* Cyan */	FOREGROUND_BLUE | FOREGROUND_GREEN,
/* White */	FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED
	};
	static const unsigned cbg [] = {
/* Black */	0,
/* Red */	BACKGROUND_RED,
/* Green */	BACKGROUND_GREEN,
/* Yellow */	BACKGROUND_RED | BACKGROUND_GREEN,
/* Blue */	BACKGROUND_BLUE,
/* Magenta */	BACKGROUND_BLUE | BACKGROUND_RED,
/* Cyan */	BACKGROUND_BLUE | BACKGROUND_GREEN,
/* White */	BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED
	};

	tty_attribs &= ~(FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | 
			 BACKGROUND_BLUE | BACKGROUND_RED | BACKGROUND_GREEN);
	tty_attribs |= cfg [fg];
	tty_attribs |= cbg [bg];
	/*printf ("[C:0x%x]", tty_attribs);*/
	if (!SetConsoleTextAttribute (tty_stdout, tty_attribs))
		printf ("SetConsoleTextAttribute() returned error: %d", GetLastError());
#else
	printf ("\x1b[3%c;4%cm", '0' + fg, '0' + bg);
	fflush (stdout);
#endif
}

const char *tty_color_names [] = {
	"Black",
	"Red",
	"Green",
	"Yellow",
	"Blue",
	"Magenta",
	"Cyan",
	"White"
};

Color_t tty_color_type (const char *s)
{
	Color_t	c;

	for (c = TC_Black; c <= TC_White; c++)
		if (!astrcmp (tty_color_names [c], s))
			return (c);

	return (TC_Black);
}

void tty_init (void)
{
	stty_raw ();
	install_atexit_handler ();
	/*printf ("\x1b""c");*/
	tty_attr_reset ();
}

/* tty_gotoxy -- Go to the given X and Y position. */

void tty_gotoxy (unsigned x, unsigned y)
{
#ifdef _WIN32
	COORD	pos;

#if 1
	pos.X = tty_start_x + x - 1;
	pos.Y = tty_start_y + y - 1;
	SetConsoleCursorPosition (tty_stdout, pos);
#else
	SetCursorPos (x + tty_anchor_x - 1, y + tty_anchor_y - 1);
#endif
#else
	printf ("\x1b[%d;%dH", y, x);
	fflush (stdout);
#endif
}

#ifdef _WIN32

static void getxy (unsigned *x, unsigned *y)
{
	CONSOLE_SCREEN_BUFFER_INFO	info;

	GetConsoleScreenBufferInfo (tty_stdout, &info);
	*x = info.dwCursorPosition.X;
	*y = info.dwCursorPosition.Y;
}

static void setxy (unsigned x, unsigned y)
{
	COORD	pos;

	pos.X = x;
	pos.Y = y;
	SetConsoleCursorPosition (tty_stdout, pos);
}

static void setregion (unsigned x,
		       unsigned y,
		       char ch,
		       unsigned attr,
		       unsigned nx,
		       unsigned ny)
{
	CHAR_INFO	buffer [80];
	unsigned	i;
	COORD		buf_size;
	COORD		buf_ofs;
	SMALL_RECT	region;

	for (i = 0; i < nx; i++) {
		buffer [i].Char.AsciiChar = ch;
		buffer [i].Attributes = attr;
	}
	buf_size.X = nx;
	buf_size.Y = 1;
	buf_ofs.X = 0;
	buf_ofs.Y = 0;
	region.Left = x;
	region.Top = y;
	region.Bottom = y;
	region.Right = x + nx - 1;
	for (i = 0; i < ny; i++) {
		WriteConsoleOutputA (tty_stdout, buffer, buf_size, buf_ofs, &region);
		region.Top++;
		region.Bottom++;
	}
}

#endif

/* tty_erase_screen -- Clear the TTY. */

void tty_erase_screen (void)
{
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO	info;

	if (!GetConsoleScreenBufferInfo (tty_stdout, &info)) {
		printf ("tty_erase_screen: can't get Console buffer info!\r\n");
		return;
	}
	if (info.srWindow.Top + 25 > info.dwSize.Y)
		tty_start_y = info.dwSize.Y - 25;
	else
		tty_start_y = info.srWindow.Top;
	tty_start_x = 0;
	tty_max_x = 79;
	tty_max_y = info.srWindow.Bottom;
#if 0
	printf ("Cursor: X=%u, Y=%u\r\n", info.dwCursorPosition.X, info.dwCursorPosition.Y);
	printf ("Max. window size: Width=%u, Height=%u\r\n", info.dwMaximumWindowSize.X, info.dwMaximumWindowSize.Y);
	printf ("Size: Width=%u, Height=%u\r\n", info.dwSize.X, info.dwSize.Y);
	printf ("Window: Bottom=%u, Left=%u, Right=%u, Top=%u\r\n", info.srWindow.Bottom, info.srWindow.Left, info.srWindow.Right, info.srWindow.Top);
	printf ("Attributes: 0x%04x\r\n", info.wAttributes);
#endif
	setregion (tty_start_x, tty_start_y,
		   ' ', tty_attribs,
		   tty_max_x - tty_start_x + 1, tty_max_y - tty_start_y + 1);
	setxy (tty_start_x, tty_start_y);
#else
	printf ("\x1b[2J");
	tty_gotoxy (1, 1);
	fflush (stdout);
#endif
}

/* tty_erase_eol -- Erase to the end of the current line. */

void tty_erase_eol (void)
{
#ifdef _WIN32
	unsigned	x, y;

	getxy (&x, &y);
	setregion (x, y, ' ', tty_attribs, tty_max_x - x + 1, 1);
#else
	printf ("\x1b[K");
	fflush (stdout);
#endif
}

/* tty_erase_sol -- Erase to the start of the current line. */

void tty_erase_sol (void)
{
#ifdef _WIN32
	unsigned	x, y;

	getxy (&x, &y);
	setregion (tty_start_x, y, ' ', tty_attribs, x + 1, 1);
#else
	printf ("\x1b[1K");
	fflush (stdout);
#endif
}

/* tty_erase_line -- Erase complete line. */

void tty_erase_line (void)
{
#ifdef _WIN32
	unsigned	x, y;

	getxy (&x, &y);
	setregion (tty_start_x, y, ' ', tty_attribs, tty_max_x - tty_start_x + 1, 1);
#else
	printf ("\x1b[2K");
	fflush (stdout);
#endif
}

/* tty_erase_eos -- Erase to the end of the screen. */

void tty_erase_eos (void)
{
#ifdef _WIN32
	unsigned	x, y;

	getxy (&x, &y);
	tty_erase_eol ();
	if (y < tty_max_y)
		setregion (tty_start_x, y + 1,
			   ' ', tty_attribs,
			   tty_max_x - tty_start_x + 1, tty_max_y - y);
#else
	printf ("\x1b[J");
	fflush (stdout);
#endif
}

/* tty_erase_sos -- Erase to the start of the screen. */

void tty_erase_sos (void)
{
#ifdef _WIN32
	unsigned	x, y;

	getxy (&x, &y);
	tty_erase_sol ();
	if (y > tty_start_y)
		setregion (tty_start_x, tty_start_y,
			   ' ', tty_attribs,
			   tty_max_x - tty_start_x + 1, y - tty_start_y);
#else
	printf ("\x1b[1J");
	fflush (stdout);
#endif
}

static char normal_corners [] = "++++";
static char normal_borders [] = "-||-";
#ifdef _WIN32
static char line_corners [] = { 218, 191, 192, 217 };
static char line_borders [] = { 196, 179, 179, 196 };
#else
static char line_corners [] = { 108, 107, 109, 106 };
static char line_borders [] = { 113, 120, 120, 113 };
#endif
const char *tty_corners = normal_corners;
const char *tty_borders = normal_borders;

/* tty_line_drawing -- Switch to line-drawing mode. */

void tty_line_drawing (int on)
{
	if (on) {
#ifndef _WIN32
		printf ("\x1b(0");
#endif
		tty_corners = line_corners;
		tty_borders = line_borders;
	}
	else {
#ifndef _WIN32
		printf ("\x1b(B");
#endif
		tty_corners = normal_corners;
		tty_borders = normal_borders;
	}
}

/* tty_save_cursor -- Save the current cursor position. */

void tty_save_cursor (void)
{
#ifdef _WIN32
	getxy (&tty_saved_x, &tty_saved_y);
#else
	printf ("\x1b[s");
	fflush (stdout);
#endif
}

/* tty_restore_cursor -- Restore the cursor position. */

void tty_restore_cursor (void)
{
#ifdef _WIN32
	setxy (tty_saved_x, tty_saved_y);
#else
	printf ("\x1b[u");
	fflush (stdout);
#endif
}

/* tty_save_cursor_attr - Save the current cursor position and attributes. */

void tty_save_cursor_attr (void)
{
#ifdef _WIN32
#else
	printf ("\x1b""7");
	fflush (stdout);
#endif
}

/* tty_restore_cursor_attr -- Restore the cursor position. */

void tty_restore_cursor_attr (void)
{
#ifdef _WIN32
#else
	printf ("\x1b""8");
	fflush (stdout);
#endif
}

#define	MAX_QUEUE	128

static char tty_queue [MAX_QUEUE];
static unsigned rp, wp;
static int full, empty = 1;

void tty_input (HANDLE fd, short events, void *udata)
{
	char		ch;
	unsigned	n;

	ARG_NOT_USED (fd)
	ARG_NOT_USED (events)
	ARG_NOT_USED (udata)

	DDS_continue ();

	n = tty_read (&ch, 1);
	if (!n)
		return;

	if (full) {
		printf ("\a");
		return;
	}
	tty_queue [wp++] = ch;
	if (wp >= MAX_QUEUE)
		wp = 0;
	empty = 0;
	if (wp == rp)
		full = 1;
}

void tty_attach (void)
{
}

int tty_getch (void)
{
	char	ch;

	while (empty)
		DDS_wait (100);

	ch = tty_queue [rp++];
	if (rp >= MAX_QUEUE)
		rp = 0;
	full = 0;
	if (rp == wp)
		empty = 1;
	return (ch);
}

#define	ESC	'\x1b'
#define	BS	8
#define	CR	13
#define	LF	10
#define	DEL	127

int tty_gets (size_t nchars, char buf [], int number, int echo)
{
	unsigned	ofs = 0;
	char		ch;

	for (;;) {
		ch = tty_getch ();
		/*printf ("{%d}", ch);*/
		fflush (stdout);
		if (ch == ESC)
			return (1);

		else if (ch == BS || ch == DEL)
			if (ofs) {
				ofs--;
				if (echo) {
					printf ("%c %c", BS, BS);
					fflush (stdout);
				}
			}
			else
				printf ("\a");

		else if (ch == CR || ch == LF) {
			if (echo) {
				printf ("\r\n");
				fflush (stdout);
			}
			buf [ofs] = '\0';
			return (0);
		}
		else if (number && (ch < '0' || ch > '9'))
			printf ("\a");
		else if (ch < ' ' || ch > '~')
			printf ("\a");
		else {
			if (ofs < nchars - 1) {
				buf [ofs++] = ch;
				if (echo) {
					if (echo == '*')
						printf ("*");
					else
						printf ("%c", ch);
					fflush (stdout);
				}
			}
			else
				printf ("\a");
		}
	}
}

