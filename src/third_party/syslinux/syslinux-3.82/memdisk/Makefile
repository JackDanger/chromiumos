## -----------------------------------------------------------------------
##
##   Copyright 2001-2009 H. Peter Anvin - All Rights Reserved
##   Copyright 2009 Intel Corporation; author: H. Peter Anvin
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

ifndef SYSLINUX_OUT
$(error no syslinux out)
endif
SUBMAKE_OUT = $(SYSLINUX_OUT)/memdisk
export SUBMAKE_OUT
$(shell mkdir -p $(SUBMAKE_OUT))

topdir = ..
include $(topdir)/MCONFIG.embedded
$(info SUBMAKE_OUT b is $(SUBMAKE_OUT))
-include $(topdir)/version.mk

INCLUDES = -I$(topdir)/com32/include
CFLAGS  += -DDATE='"$(DATE)"'
LDFLAGS  = $(GCCOPT) -g
NASM     = nasm
NASMOPT  = -O9999
NFLAGS   = -dDATE='"$(DATE)"'
NINCLUDE = -I$(SUBMAKE_OUT)/

SRCS	 = $(wildcard *.asm *.c *.h)

# The DATE is set on the make command line when building binaries for
# official release.  Otherwise, substitute a hex string that is pretty much
# guaranteed to be unique to be unique from build to build.
ifndef HEXDATE
HEXDATE := $(shell $(PERL) ../now.pl $(SRCS))
endif
ifndef DATE
DATE    := $(shell sh ../gen-id.sh $(VERSION) $(HEXDATE))
endif

# Important: init.o16 must be first!!
OBJS16   = $(SUBMAKE_OUT)/init.o16 $(SUBMAKE_OUT)/init32.o
OBJS32   = $(SUBMAKE_OUT)/start32.o $(SUBMAKE_OUT)/setup.o $(SUBMAKE_OUT)/msetup.o $(SUBMAKE_OUT)/e820func.o $(SUBMAKE_OUT)/conio.o $(SUBMAKE_OUT)/memcpy.o $(SUBMAKE_OUT)/memset.o \
           $(SUBMAKE_OUT)/unzip.o $(SUBMAKE_OUT)/memdisk_chs.o $(SUBMAKE_OUT)/memdisk_edd.o

CSRC     = setup.c msetup.c e820func.c conio.c unzip.c
SSRC     = start32.S memcpy.S memset.S
NASMSRC  = memdisk_chs.asm memdisk_edd.asm memdisk16.asm

all: $(SUBMAKE_OUT)/memdisk # e820test

# tidy, clean removes everything except the final binary
tidy dist:
	rm -f *.o *.s *.tmp *.o16 *.s16 *.bin *.lst *.elf e820test .*.d

clean: tidy

# spotless also removes the product binary
spotless: clean
	rm -f memdisk .depend

$(SUBMAKE_OUT)/memdisk16.o: memdisk16.asm

# Cancel rule
$(SUBMAKE_OUT)/%.o: %.asm

$(SUBMAKE_OUT)/memdisk16.o: memdisk16.asm
	( $(NASM) -M -DDEPEND $(NFLAGS) $(NINCLUDE) -o $@ $< ; echo '' ) > $(SUBMAKE_OUT)/.$(shell basename $@).d ; true
	$(NASM) -f elf $(NASMOPT) $(NFLAGS) $(NINCLUDE) -o $@ -l $(SUBMAKE_OUT)/$*.lst $<

.PRECIOUS: $(SUBMAKE_OUT)/%.bin
$(SUBMAKE_OUT)/%.bin: %.asm
	( $(NASM) -M -DDEPEND $(NFLAGS) $(NINCLUDE) -o $@ $< ; echo '' ) > .$@.d ; true
	$(NASM) -f bin $(NASMOPT) $(NFLAGS) $(NINCLUDE) -o $@ -l $(SUBMAKE_OUT)/$*.lst $<

$(SUBMAKE_OUT)/memdisk_%.o: $(SUBMAKE_OUT)/memdisk_%.bin
	( cd $(SUBMAKE_OUT) && $(LD) -r -b binary -o $(shell basename $@) $(shell basename $< ) )

$(SUBMAKE_OUT)/memdisk16.elf: $(OBJS16)
	$(LD) -Ttext 0 -o $@ $^

$(SUBMAKE_OUT)/memdisk32.elf: memdisk.ld $(OBJS32)
	$(LD) -o $@ -T $^

$(SUBMAKE_OUT)/%.bin: $(SUBMAKE_OUT)/%.elf
	$(OBJCOPY) -O binary $< $@

$(SUBMAKE_OUT)/memdisk: $(SUBMAKE_OUT)/memdisk16.bin $(SUBMAKE_OUT)/memdisk32.bin postprocess.pl
	$(PERL) postprocess.pl $@ $(SUBMAKE_OUT)/memdisk16.bin $(SUBMAKE_OUT)/memdisk32.bin

$(SUBMAKE_OUT)/e820test: e820test.c e820func.c msetup.c
	$(CC) -m32 -g -W -Wall -DTEST -o $@ $^

# This file contains the version number, so add a dependency for it
setup.s: ../version

# Include dependencies file
-include $(SUBMAKE_OUT)/.*.d
