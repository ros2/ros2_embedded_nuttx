/************************************************************************************
 * arch/arm/src/arm/setjmp.S
 *
 *   Copyright (C) 2014 Víctor Mayoral Vilches. All rights reserved.
 *   Author: Víctor Mayoral Vilches <victor@osrfoundation.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ************************************************************************************/

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <nuttx/config.h>

/************************************************************************************
 * Definitions
 ************************************************************************************/

# define BX(x) mov      pc, x

/************************************************************************************
 * Name: setjmp
 *
 * Description:
 *    save stack context for nonlocal goto
 *
 ************************************************************************************/
#ifndef	__thumb__
#
# "Pure ARM" version
#
# The jmp_buf is assumed to contain the following, in order:
#		r4
#		r5
#		r6
#		r7
#		r8
#		r9
#		r10
#		fp
#		sp
#		lr
#
	.text
	.balign 4
	.globl setjmp
	.type setjmp, #function
setjmp:
	stmia	r0, {r4, r5, r6, r7, r8, r9, r10, fp, sp, lr}
	mov	r0, #0
	BX(lr)
	.size setjmp,.-setjmp
	.text
	.balign 4
	.globl longjmp
	.type longjmp, #function
longjmp:
	ldmia	r0, {r4, r5, r6, r7, r8, r9, r10, fp, sp, lr}
	mov	r0, r1
	bne	1f
	mov	r0, #1
1:	BX(lr)
	.size longjmp,.-longjmp

#else /* __thumb__ */
#
# Thumb version
#
# The jmp_buf is assumed to contain the following, in order:
#		lr
#		r4
#		r5
#		r6
#		r7
#		r8
#		r9
#		r10
#		fp
#		sp
#
	.text
	.balign 4
	.globl setjmp
	.type setjmp, #function
	.thumb_func
setjmp:
	mov	r2, r0
	mov	r3, lr
	stmia	r0!, {r3, r4, r5, r6, r7}
	mov	r3, r8
	mov	r4, r9
	mov	r5, r10
	mov	r6, fp
	mov	r7, sp
	stmia	r0!, {r3, r4, r5, r6, r7}
	/* Do not trash r4 .. r7 */
	ldmia	r2!, {r3, r4, r5, r6, r7}
	mov	r0, #0
	BX(lr)
	.size setjmp,.-setjmp
	.text
	.balign 4
	.globl longjmp
	.type longjmp, #function
	.thumb_func
longjmp:
	mov	r2, r0
	add	r0, #5*4
	ldmia	r0!, {r3, r4, r5, r6, r7}
	mov	r8, r3
	mov	r9, r4
	mov	r10, r5
	mov	fp, r6
	mov	sp, r7
	ldmia	r2!, {r3, r4, r5, r6, r7}
	mov	r0, r1
	bne	1f
	mov	r0, #1
1:	BX(r3)
	.size longjmp,.-longjmp
#endif /* __thumb__ */