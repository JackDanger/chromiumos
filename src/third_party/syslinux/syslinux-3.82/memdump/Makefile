## -----------------------------------------------------------------------
##
##   Copyright 2001-2008 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## memory dump utility
##

ifndef SYSLINUX_OUT
$(error no syslinux out)
endif
SUBMAKE_OUT = $(SYSLINUX_OUT)/memdump
export SUBMAKE_OUT
$(shell mkdir -p $(SUBMAKE_OUT))

topdir = ..
include $(topdir)/MCONFIG.embedded

OPTFLAGS = 
INCLUDES = -include code16.h -I.
LDFLAGS	 = -T com16.ld

SRCS     = main.c serial.c ymsend.c
OBJS	 = $(SUBMAKE_OUT)/crt0.o $(patsubst %.c,$(SUBMAKE_OUT)/%.o,$(notdir $(SRCS)))
LIBOBJS	 = $(SUBMAKE_OUT)/conio.o $(SUBMAKE_OUT)/memcpy.o $(SUBMAKE_OUT)/memset.o $(SUBMAKE_OUT)/skipatou.o $(SUBMAKE_OUT)/strtoul.o \
	   $(SUBMAKE_OUT)/argv.o $(SUBMAKE_OUT)/printf.o $(SUBMAKE_OUT)/__divdi3.o $(SUBMAKE_OUT)/__udivmoddi4.o

.SUFFIXES: .c .o .i .s .S .elf .com

TARGETS = $(SUBMAKE_OUT)/memdump.com

all: $(TARGETS)

tidy dist:
	-rm -f *.o *.i *.s *.a .*.d *.tmp *.elf

clean: tidy

spotless: clean
	-rm -f *~ $(TARGETS)

installer:

$(SUBMAKE_OUT)/memdump.elf: $(OBJS) $(SUBMAKE_OUT)/libcom.a
	$(LD) $(LDFLAGS) -o $@ $^

$(SUBMAKE_OUT)/libcom.a: $(LIBOBJS)
	-rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

$(SUBMAKE_OUT)/memdump.com: $(SUBMAKE_OUT)/memdump.elf
	$(OBJCOPY) -O binary $< $@

$(SUBMAKE_OUT)/%.o: %.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -c -o $@ $<
$(SUBMAKE_OUT)/%.i: %.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -E -o $@ $<
$(SUBMAKE_OUT)/%.s: %.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -S -o $@ $<
$(SUBMAKE_OUT)/%.o: %.S
	$(CC) $(MAKEDEPS) $(SFLAGS) -c -o $@ $<
$(SUBMAKE_OUT)/%.s: %.S
	$(CC) $(MAKEDEPS) $(SFLAGS) -E -o $@ $<

-include .*.d *.tmp
