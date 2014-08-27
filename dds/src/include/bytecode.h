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

/* bytecode.h -- Defines the bytecodes that are generated from Query and Filter
		 SQL statements as well the interface to the bytecode
		 interpreter that can be used to parse the statements. */

#ifndef __bytecode_h_
#define	__bytecode_h_

#include "dds/dds_error.h"
#include "db.h"
#include "uqos.h"
#include "typecode.h"

#define	BC_OK		DDS_RETCODE_OK
#define	BC_ERR_STKOVFL	DDS_RETCODE_ERROR
#define	BC_ERR_INVADDR	DDS_RETCODE_ERROR
#define	BC_ERR_INVDATA	DDS_RETCODE_ERROR
#define	BC_ERR_INVOP	DDS_RETCODE_ERROR
#define	BC_ERR_INVPAR	DDS_RETCODE_ERROR
#define	BC_ERR_INFLOOP	DDS_RETCODE_ERROR
#define	BC_ERR_NFOUND	DDS_RETCODE_ALREADY_DELETED
#define	BC_ERR_UNIMPL	DDS_RETCODE_UNSUPPORTED

/*      Name	Code	   Definition                          Argument
        ----    ----       ----------                          -------- */

/* Load constant (embedded): */
#define	O_LCI	0	/* Small constant embedded integer (-64..63). */

/* Load constant (follows opcode): */
#define O_LCBU	0x80	/* Unsigned byte (8-bit)               [1]. */
#define O_LCBS	0x81	/* Signed byte (8-bit)                 [1]. */
#define O_LCSU	0x82	/* Unsigned short (16-bit)             [2]. */
#define O_LCSS	0x83	/* Signed short (16-bit)               [2]. */
#define O_LCWU	0x84	/* Unsigned integer (32-bit)           [4]. */
#define O_LCWS	0x85	/* Signed integer (32-bit)             [4]. */
#define	O_LCL	0x86	/* Long (64-bit)                       [8]. */
#define	O_LCF	0x87	/* Float (32-bit)                      [4]. */
#define	O_LCD	0x88	/* Double (64-bit)                     [8]. */
#define	O_LCO	0x89	/* Long Double (128-bit)               [16]. */
#define	O_LCC	O_LCBU	/* Character (8-bit)                   [1]. */
#define	O_LCU	O_LCWU	/* Wide/Unicode character (32-bit)     [4]. */
#define	O_LCS	0x8a	/* String pointer                      [str]. */

/* Load payload data: */
#define	O_LDBU	0x90	/* Unsigned byte (8-bit)               [ofs]. */
#define	O_LDBS	0x91	/* Signed byte (8-bit)                 [ofs]. */
#define	O_LDSU	0x92	/* Unsigned short (16-bit)             [ofs]. */
#define	O_LDSS	0x93	/* Signed short (16-bit)               [ofs]. */
#define	O_LDWU	0x94	/* Unsigned word (32-bit)              [ofs]. */
#define	O_LDWS	0x95	/* Signed word (32-bit)                [ofs]. */
#define	O_LDL	0x96	/* Long (64-bit)                       [ofs]. */
#define	O_LDF	0x97	/* Float (32-bit)                      [ofs]. */
#define	O_LDD	0x98	/* Double (64-bit)                     [ofs]. */
#define	O_LDO	0x99	/* Long Double (128-bit)               [ofs]. */
#define	O_LDC	O_LDBU	/* Character (8-bit)                   [ofs]. */
#define	O_LDU	O_LDWU	/* Wide/Unicode character (32-bit)     [ofs]. */
#define	O_LDS	0x9a	/* String pointer                      [ofs]. */

/* Load Parameter data: */
#define	O_LPBU	0xa0	/* Unsigned byte (8-bit)               [ofs]. */
#define	O_LPBS	0xa1	/* Signed byte (8-bit)                 [ofs]. */
#define	O_LPSU	0xa2	/* Unsigned short (16-bit)             [ofs]. */
#define	O_LPSS	0xa3	/* Signed short (16-bit)               [ofs]. */
#define	O_LPWU	0xa4	/* Unsigned word (32-bit)              [ofs]. */
#define	O_LPWS	0xa5	/* Signed word (32-bit)                [ofs]. */
#define	O_LPL	0xa6	/* Long (64-bit)                       [ofs]. */
#define	O_LPF	0xa7	/* Float (32-bit)                      [ofs]. */
#define	O_LPD	0xa8	/* Double (64-bit)                     [ofs]. */
#define	O_LPO	0xa9	/* Long Double (128-bit)               [ofs]. */
#define	O_LPC	O_LPBU	/* Character (8-bit)                   [ofs]. */
#define	O_LPU	O_LPWU	/* Wide/Unicode character (32-bit)     [ofs]. */
#define	O_LPS	0xaa	/* String pointer                      [ofs]. */

/* Comparisons (removes arguments from stack): */
#define	O_CMPWU	0xb0	/* Unsigned word compare tos-1 & tos. */
#define	O_CMPWS	0xb1	/* Signed word compare tos-1 & tos. */
#define	O_CMPLU	0xb2	/* Unsigned long compare tos-1 & tos. */
#define	O_CMPLS	0xb3	/* Signed long compare tos-1 & tos. */
#define	O_CMPD	0xb4	/* Double compare tos-1 & tos. */
#define	O_CMPO	0xb5	/* Long double compare tos-1 & tos. */
#define	O_CMPS	0xb6	/* String compare tos-1 & tos. */
#define	O_BTWWU	0xb7	/* Unsigned word check if tos-2 in [tos-1..tos]. */
#define	O_BTWWS	0xb8	/* Signed word check if tos-2 in [tos-1..tos]. */
#define	O_BTWLU	0xb9	/* Unsigned long check if tos-2 in [tos-1..tos]. */
#define	O_BTWLS	0xba	/* Signed long check if tos-2 in [tos-1..tos]. */
#define	O_BTWD	0xbb	/* Double check if tos-2 in [tos-1..tos]. */
#define	O_BTWO	0xbc	/* Long Double check if tos-2 in [tos-1..tos]. */
#define	O_LIKE	0xbd	/* String match tos-1 and tos. */

/* Branches: */
#define	O_BEQ	0xc0	/* Branch if equal                     [loc]. */
#define	O_BNE	0xc1	/* Branch if not equal                 [loc]. */
#define	O_BGT	0xc2	/* Branch if greater than              [loc]. */
#define	O_BLE	0xc3	/* Branch if less than or equal        [loc]. */
#define	O_BLT	0xc4	/* Branch if less than                 [loc]. */
#define	O_BGE	0xc5	/* Branch if greater than or equal     [loc]. */
#define	O_BT	0xc8	/* Branch if True                      [loc]. */
#define	O_BF	0xc9	/* Branch if False                     [loc]. */

/* Conversions: */
#define	O_WU2L	0xd0	/* Convert unsigned 32-bit to 64-bit. */
#define	O_WS2L	0xd1	/* Convert signed 32-bit to 64-bit. */
#define	O_WU2D	0xd2	/* Convert unsigned 32-bit to double. */
#define	O_WS2D	0xd3	/* Convert signed 32-bit to double. */
#define	O_WU2O	0xd4	/* Convert unsigned 32-bit to long double. */
#define	O_WS2O	0xd5	/* Convert signed 32-bit to long double. */
#define	O_LU2D	0xd6	/* Convert unsigned 64-bit to double. */
#define	O_LS2D	0xd7	/* Convert signed 64-bit to double. */
#define	O_LU2O	0xd8	/* Convert unsigned 64-bit to long double. */
#define	O_LS2O	0xd9	/* Convert signed 64-bit to long double. */
#define	O_D2O	0xda	/* Convert double to long double. */
#define	O_S2W	0xdb	/* Convert string to 32-bit. */
#define	O_S2L	0xdc	/* Convert string to 64-bit. */
#define	O_S2D	0xdd	/* Convert string to double. */
#define	O_S2O	0xde	/* Convert string to long double. */
#define	O_C2S	0xdf	/* Convert char to string. */

/* Various: */
#define	O_CREF	0xe0	/* Container reference                 [ofs]. */
#define	O_DISC	0xe1	/* Discriminant reference              [4]. */
#define	O_TREF	0xe2	/* Topic reference                     [str]. */
#define	O_FCR	0xe3	/* Field Container reference           [ofs]. */
#define	O_FOFS	0xe4	/* Field offset                        [ofs]. */
#define	O_DS0	0xe5	/* Select first data sample. */
#define	O_DS1	0xe6	/* Select second data sample. */

/* Returns: */
#define	O_RET	0xf0	/* Return top of stack to caller. */
#define	O_RETC	0xf1	/* Return comparison result to caller. */
#define	O_RETT	0xf2	/* Return True. */
#define	O_RETF	0xf3	/* Return False. */
#define	O_CRFF	0xf4	/* If False: return False. */
#define	O_CRFT	0xf5	/* If False: return True. */
#define	O_CRTF	0xf6	/* If True: return False. */
#define	O_CRTT	0xf7	/* If True: return True. */
#define	O_CRNE	0xf8	/* If not == 0, return result. */

#define	O_NOP	0xff	/* No operation. */

typedef struct bc_program_st {
	unsigned char	*start;
	unsigned char	*buffer;
	size_t		length;
	unsigned	npars;
} BCProgram;

int bc_init (void);

/* Initialize the bytecode interpreter. */

int bc_interpret (const BCProgram     *program,
		  const Strings_t     *pars,
		  void                *cache,
		  DBW                 *data,
		  DBW                 *data2,
		  int                 marshalled,
		  const TypeSupport_t *ts,
		  int                 *result);

/* Interpret a bytecode program (progcode, proglen), with the given parameter
   list (pars) over a single data sample (data, marshalled) of the specified
   type (ts), or over two data samples (data and data2).
   The cache argument is a reference to a simple pointer that will be kept by
   the caller for caching of often used data.  Use bc_cache_init() before the
   first call to bc_interpret(), and flush the cache with bc_cache_flush())
   whenever either the program changes or the parameters are updated.
   If successful, 0 is returned and *result will be set to the value of the
   return code of the bytecode program (via O_RET*). */

void bc_cache_init (void *cache);

/* Initialize a cache argument. */

void bc_cache_flush (void *cache);

/* Flush the cache contents.  This function should be called whenever the filter
   needs to be cleaned up. */

void bc_cache_reset (void *cache);

/* Reset the bytecode cache contents.  Should be called when the bytecode program
   or its arguments are updated. */

void bc_dump (unsigned indent, const BCProgram *program);

/* Dump, i.e. disassemble a bytecode program. */

#endif /* !__bytecode_h_ */

