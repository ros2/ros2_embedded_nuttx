############################################################################
# Makefile
#
#   Copyright (C) 2012 Gregory Nutt. All rights reserved.
#   Author: Gregory Nutt <gnutt@nuttx.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name NuttX nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################

# This is a top-level "kludge" Makefile that just includes the correct
# Makefile.  If you already know the Makefile that you want, you can skip
# this nonsense using:
#
#   make -f Makefile.unix, OR
#   make -f Makefile.win
#

-include .config
ifeq ($(CONFIG_WINDOWS_NATIVE),y)
include Makefile.win
else
include Makefile.unix
endif

## Other functions
OPENOCD=../tools/openocd/bin/openocd
QEMU_STM32 ?= ../../RTOS/qemu_stm32/arm-softmmu/qemu-system-arm
CROSS_COMPILE=arm-none-eabi-
STLINK=../stlink/

qemu: nuttx.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 -kernel nuttx.bin

qemudbg: nuttx.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel nuttx.bin

gdb-qemu: nuttx.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel nuttx

qemuauto: nuttx.bin
	bash emulate.sh nuttx.bin

emu: nuttx.bin
	bash emulate.sh nuttx.bin

program: nuttx.bin
	-$(OPENOCD) -f board/stm32f4discovery.cfg -c "init" -c "reset halt" -c "flash write_image erase nuttx.bin 0x08000000 bin" -c "verify_image nuttx.bin 0x8000000; reset run; exit"

dump_flash:
	$(OPENOCD) -f ../tools/olimex-arm-usb-tiny-h.cfg -f ../tools/stm32.cfg -c "init; halt; dump_image nuttx.bin.dump 0x8000000 0x1000; reset run; shutdown"

gdb_server:
	${STLINK}st-util -V
#	st-util -V
		
gdb: nuttx.bin
	$(CROSS_COMPILE)gdb -tui nuttx -ex 'target remote localhost:4242'

reset:
	$(OPENOCD) -f ../tools/olimex-arm-usb-tiny-h.cfg -f ../tools/stm32.cfg -c "init; halt; reset run; shutdown"
