# $Id: Makefile,v 1.29 2008/12/11 12:18:17 ecd Exp $
#

X49GP_DEBUG = \
	-DDEBUG_X49GP_MODULES \
	-DDEBUG_S3C2410_SRAM \
	-DDEBUG_S3C2410_MEMC \
	-DDEBUG_S3C2410_INTC \
	-DDEBUG_S3C2410_POWER \
	-DDEBUG_S3C2410_LCD \
	-DDEBUG_S3C2410_UART \
	-DDEBUG_S3C2410_TIMER \
	-DDEBUG_S3C2410_USBDEV \
	-DDEBUG_S3C2410_WATCHDOG \
	-DDEBUG_S3C2410_IO_PORT \
	-DDEBUG_S3C2410_RTC \
	-DDEBUG_S3C2410_ADC \
	-DDEBUG_S3C2410_SDI \
	-DDEBUG_S3C2410_SPI \
	-DDEBUG_X49GP_SYSCALL \
	-DDEBUG_X49GP_FLASH_READ \
	-DDEBUG_X49GP_FLASH_WRITE \
	-UDEBUG_X49GP_SYSRAM_READ \
	-UDEBUG_X49GP_SYSRAM_WRITE \
	-UDEBUG_X49GP_ERAM_READ \
	-UDEBUG_X49GP_ERAM_WRITE \
	-UDEBUG_X49GP_IRAM_READ \
	-UDEBUG_X49GP_IRAM_WRITE \
	-DDEBUG_X49GP_TIMER_IDLE \
	-DDEBUG_X49GP_ARM_IDLE \
	-DDEBUG_X49GP_ENABLE_IRQ \
	-DDEBUG_X49GP_UI

DEBUG = -g # -pg

#2.09
FIRMWARE = 4950_92.bin
#2.10-7
#FIRMWARE = hp49g-u.bin
#HPGCC3 (copy HPGCC3 ROM first)
#FIRMWARE = 49_hpgcc.bin
#2.15
#FIRMWARE = 4950_215.bin

BOOT49GP = boot-49g+.bin
BOOT50G = boot-50g.bin
SERIAL49GP = DE00000001
SERIAL50G = DEA0000001
COPYRIGHT = Kinposhcopyright

QEMU_DEFINES = -DTARGET_ARM -DX49GP \
	-D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE \
	-fno-strict-aliasing

# Use this to debug
# DEFINES = $(X49GP_DEBUG) $(QEMU_DEFINES)
# Use this for speed
DEFINES = $(QEMU_DEFINES)

ifdef QEMU_OLD
QEMUSRC = qemu/prepare.sh \
	$(wildcard qemu/patches/*.patch) \
	$(wildcard qemu/patches/*.diff)

QEMU=qemu/qemu
else
QEMUSRC =
QEMU=qemu/qemu-git
endif

QEMUMAKE = $(shell if [ "`uname -s`" = "Linux" -a "`uname -m`" = "sun4u" ]; then echo "sparc32 $(MAKE)"; else echo "$(MAKE)"; fi)

ifdef QEMU_OLD
QEMU_DIR=$(QEMU)
QEMU_DEFINES+=-DQEMU_OLD
X49GP_LDFLAGS = -L$(QEMU)/arm-softmmu
X49GP_LIBS = -lqemu -lz
else
QEMU_DIR=qemu/qemu-git
QEMU_DIR_BUILD=$(QEMU_DIR)/arm-softmmu
QEMU_DEFINES+=-DNEED_CPU_H
X49GP_LDFLAGS=
X49GP_LIBS= $(QEMU_DIR_BUILD)/exec.o $(QEMU_DIR_BUILD)/translate-all.o $(QEMU_DIR_BUILD)/cpu-exec.o $(QEMU_DIR_BUILD)/translate.o $(QEMU_DIR_BUILD)/fpu/softfloat.o $(QEMU_DIR_BUILD)/op_helper.o $(QEMU_DIR_BUILD)/helper.o $(QEMU_DIR_BUILD)/disas.o $(QEMU_DIR_BUILD)/i386-dis.o $(QEMU_DIR_BUILD)/arm-dis.o $(QEMU_DIR_BUILD)/tcg/tcg.o $(QEMU_DIR_BUILD)/iwmmxt_helper.o $(QEMU_DIR_BUILD)/neon_helper.o
endif
QEMU_INCDIR=$(QEMU_DIR)
QEMU_INC=-I$(QEMU_INCDIR)/target-arm -I$(QEMU_INCDIR) -I$(QEMU_INCDIR)/fpu -I$(QEMU_INCDIR)/arm-softmmu

X49GP_INCLUDES = -Iinclude -Ibitmaps $(QEMU_INC)

INCLUDES = $(GDB_INCLUDES) $(X49GP_INCLUDES)

ifdef QEMU_OLD
CC = $(shell if [ "`uname -s`" = "Darwin" ]; then echo "gcc"; else echo "gcc-3.4"; fi)
else
CC = gcc
endif
LD = $(CC)
AR = ar
RANLIB = ranlib

CC += $(shell if [ "`uname -m`" = "sparc64" -o "`uname -m`" = "sun4u" ]; then echo "-mcpu=ultrasparc -m32"; fi)

COCOA_LIBS=$(shell if [ "`uname -s`" = "Darwin" ]; then echo "-F/System/Library/Frameworks -framework Cocoa -framework IOKit"; fi)

CFLAGS = -O2 -Wall $(DEBUG) $(INCLUDES) $(DEFINES)
LDFLAGS = $(DEBUG) $(X49GP_LDFLAGS) $(GDB_LDFLAGS)
LDLIBS = $(X49GP_LIBS) $(GDB_LIBS) $(COCOA_LIBS)

MAKEDEPEND = $(CC) -MM

CFLAGS += $(shell pkg-config --cflags gtk+-2.0)
LDLIBS += $(shell pkg-config --libs gtk+-2.0) -lz -lm

ifdef QEMU_OLD
export MAKE MAKEDEPEND CC LD AR RANLIB CFLAGS LDFLAGS
endif

LIBS = $(QEMU)

SRCS = main.c module.c flash.c sram.c s3c2410.c \
	s3c2410_sram.c \
	s3c2410_memc.c \
	s3c2410_intc.c \
	s3c2410_power.c \
	s3c2410_lcd.c \
	s3c2410_nand.c \
	s3c2410_uart.c \
	s3c2410_timer.c \
	s3c2410_usbdev.c \
	s3c2410_watchdog.c \
	s3c2410_io_port.c \
	s3c2410_rtc.c \
	s3c2410_adc.c \
	s3c2410_spi.c \
	s3c2410_sdi.c \
	s3c2410_arm.c \
	ui.c timer.c tiny_font.c symbol.c \
	gdbstub.c block.c

OBJS = $(SRCS:.c=.o)

ifdef QEMU_OLD
VVFATOBJS = $(QEMU)/arm-softmmu/block-vvfat.o \
	$(QEMU)/arm-softmmu/block-qcow.o \
	$(QEMU)/arm-softmmu/block-raw.o
else
# TEMPO hack
VVFATOBJS = block-vvfat.o \
	block-qcow.o \
	block-raw.o
endif

ifdef QEMU_OLD
VVFATOBJS += $(QEMU)/arm-softmmu/cutils.o
else
VVFATOBJS += $(QEMU_DIR)/cutils.o
endif

TARGET = x49gp

all: do-it-all

ifeq (.depend,$(wildcard .depend))
include .depend
do-it-all: $(QEMU) $(TARGET) flash-49g+ flash-50g sram s3c2410-sram
else
do-it-all: depend-and-build
endif

ifdef QEMU_OLD
$(TARGET): $(OBJS) $(VVFATOBJS) $(QEMU)/arm-softmmu/libqemu.a
	$(CC) -Wl,--no-as-needed $(LDFLAGS) -o $@ $(OBJS) $(VVFATOBJS) $(LDLIBS)
else
$(TARGET): $(OBJS) $(VVFATOBJS) $(X49GP_LIBS)
	$(CC) -Wl,--no-as-needed $(LDFLAGS) -o $@ $(OBJS) $(VVFATOBJS) $(LDLIBS)
endif

flash-49g+: $(BOOT49GP) flash-noboot
	@cp flash-noboot $@
	@/bin/echo -n "$@: Copy boot loader \"$(BOOT49GP)\" to "; echo "0"
	@dd if=$(BOOT49GP) of=$@ bs=16 seek=0 conv=notrunc 2>/dev/null
	@/bin/echo -n "$@: Set serial number \"$(SERIAL49GP)\" at "; expr 16 \* 1023
	@/bin/echo -n "$(SERIAL49GP)" >serialno
	@dd if=serialno of=$@ bs=16 seek=1023 conv=notrunc 2>/dev/null
	@rm -f serialno

flash-50g: $(BOOT50G) flash-noboot
	@cp flash-noboot $@
	@/bin/echo -n "$@: Copy boot loader \"$(BOOT50G)\" to "; echo "0"
	@dd if=$(BOOT50G) of=$@ bs=16 seek=0 conv=notrunc 2>/dev/null
	@/bin/echo -n "$@: Set serial number \"$(SERIAL50G)\" at "; expr 16 \* 1023
	@/bin/echo -n "$(SERIAL50G)" >serialno
	@dd if=serialno of=$@ bs=16 seek=1023 conv=notrunc 2>/dev/null
	@rm -f serialno

flash-noboot: $(FIRMWARE)
	@/bin/echo -n "$@: Fill ff: 0 "
	@/bin/echo -ne "\377" >one
	@COUNT=0; \
	SIZE=1; \
	while [ $${COUNT} -le 20 ]; do \
		cat one one >one2; \
		mv one2 one; \
		COUNT=`expr $${COUNT} + 1`; \
		SIZE=`expr $${SIZE} + $${SIZE}`; \
		/bin/echo -n "."; \
	done; \
	echo " $${SIZE}"
	@mv one $@
	@/bin/echo -n "$@: Mark blocks:"
	@OFFSET=524544; \
	for block in 02 03 04 05 06 07 10 11 12 13 14 15; do \
		/bin/echo -ne "\360\0$${block}\000\000\000" >header; \
		dd if=header of=$@ bs=1 seek=$${OFFSET} conv=notrunc 2>/dev/null; \
		/bin/echo -n " $${OFFSET}"; \
		OFFSET=`expr $${OFFSET} + 131072`; \
	done; \
	echo ""
	@/bin/echo -n "$@: Copy firmware \"$(FIRMWARE)\" to "; expr 16 \* 1024
	@dd if=hp49g-u.bin of=$@ bs=16 seek=1024 conv=notrunc 2>/dev/null
	@dd if=$(FIRMWARE) of=$@ bs=16 seek=1024 conv=notrunc 2>/dev/null
	@/bin/echo -n "$@: Set copyright \"$(COPYRIGHT)\" at "; expr 16 \* 1024
	@/bin/echo -n "$(COPYRIGHT)" >copyright
	@dd if=copyright of=$@ bs=16 seek=1024 conv=notrunc 2>/dev/null
	@rm -f one one2 header copyright

sram:
	dd if=/dev/zero of=$@ bs=1024 count=512

s3c2410-sram:
	dd if=/dev/zero of=$@ bs=1024 count=4

sdcard:
ifeq ($(shell uname),Darwin)
	rm -f sdcard.dmg
	hdiutil create sdcard -megabytes 64 -fs MS-DOS -volname x49gp
else
	/sbin/mkdosfs -v -C -S 512 -f 2 -F 16 -r 512 -R 2 -n "x49gp" $@ 65536
endif

config:
	./newconfig

sim: dummy
	$(MAKE) -C $@

ifdef QEMU_OLD
$(QEMU): $(QEMU)/config-host.h dummy
	$(QEMUMAKE) -C $@

$(QEMU)/config-host.h: $(QEMUSRC)
	cd qemu; ./prepare.sh
	make -C . all

$(QEMU)/arm-softmmu/%.o: $(QEMU)/%.c
	$(QEMUMAKE) BASE_CFLAGS=-DX49GP -C $(QEMU)/arm-softmmu $(shell basename $@)
else
$(QEMU)/config-host.h: $(QEMUSRC)
	( cd $(QEMU); \
	./configure-small --extra-cflags=-DX49GP; \
	$(QEMUMAKE) -f Makefile-small )

$(QEMU)/arm-softmmu/%.o: $(QEMU)/%.c
	$(QEMUMAKE) -C $(QEMU) -f Makefile-small
endif

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

ifdef QEMU_OLD
clean-libs:
	if [ -n "$(LIBS)" ]; then \
		for d in $(LIBS); do \
			$(MAKE) -C $$d clean; \
		done \
	fi

clean: clean-libs
	rm -f *.o core *~ .depend

distclean: clean
	rm -rf $(QEMU)
	rm -f $(TARGET) flash-49g+ flash-50g flash-noboot sram s3c2410-sram
	rm -f config

depend-and-build: depend
	$(MAKE) -C . all

depend-libs: $(QEMU)/config-host.h
	if [ -n "$(LIBS)" ]; then \
		for d in $(LIBS); do \
			if [ "$$d" != "$(QEMU)" ]; then \
				$(MAKE) -C $$d depend; \
			fi \
		done \
	fi

depend: depend-libs
	$(MAKEDEPEND) $(CFLAGS) $(SRCS) >.depend
else
clean-qemu:
	$(MAKE) -C $(QEMU) -f Makefile-small clean

clean: clean-qemu
	rm -f *.o core *~ .depend

distclean: clean
	$(MAKE) -C $(QEMU) -f Makefile-small distclean
	rm -f $(TARGET) flash-49g+ flash-50g flash-noboot sram s3c2410-sram
	rm -f config

depend-libs: $(QEMU)/config-host.h

depend-and-build: depend
	$(MAKE) -C . all

depend: depend-libs
	$(MAKEDEPEND) $(CFLAGS) $(SRCS) >.depend
endif

dummy:
