## -----------------------------------------------------------------------
##
##   Copyright 1998-2009 H. Peter Anvin - All Rights Reserved
##   Copyright 2009 Intel Corporation; author: H. Peter Anvin
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Makefile for the SYSLINUX core
#

# No builtin rules
MAKEFLAGS += -r
MAKE      += -r

ifndef SYSLINUX_OUT
$(error no syslinux out)
endif
SUBMAKE_OUT = $(SYSLINUX_OUT)/core
export SUBMAKE_OUT
$(shell mkdir -p $(SUBMAKE_OUT))

topdir = ..
include $(topdir)/MCONFIG.embedded
-include $(topdir)/version.mk

OPTFLAGS = 
INCLUDES =

# This is very similar to cp437; technically it's for Norway and Denmark,
# but it's unlikely the characters that are different will be used in
# filenames by other users.
CODEPAGE = cp865

# The targets to build in this directory...
BTARGET  = $(SUBMAKE_OUT)/kwdhash.gen \
	   $(SUBMAKE_OUT)/ldlinux.bss $(SUBMAKE_OUT)/ldlinux.sys $(SUBMAKE_OUT)/ldlinux.bin \
	   $(SUBMAKE_OUT)/pxelinux.0 $(SUBMAKE_OUT)/isolinux.bin $(SUBMAKE_OUT)/isolinux-debug.bin \
	   $(SUBMAKE_OUT)/extlinux.bin $(SUBMAKE_OUT)/extlinux.bss $(SUBMAKE_OUT)/extlinux.sys

# All primary source files for the main syslinux files
NASMSRC	 = $(wildcard *.asm)
NASMHDR  = $(wildcard *.inc)
CSRC	 = $(wildcard *.c)
CHDR	 = $(wildcard *.h)
OTHERSRC = keywords
ALLSRC   = $(NASMSRC) $(NASMHDR) $(CSRC) $(CHDR) $(OTHERSRC)

# The DATE is set on the make command line when building binaries for
# official release.  Otherwise, substitute a hex string that is pretty much
# guaranteed to be unique to be unique from build to build.
ifndef HEXDATE
HEXDATE := $(shell $(PERL) ../now.pl $(SRCS))
endif
ifndef DATE
DATE    := $(shell sh ../gen-id.sh $(VERSION) $(HEXDATE))
endif

all: $(BTARGET)

$(SUBMAKE_OUT)/kwdhash.gen: keywords genhash.pl
	$(PERL) genhash.pl < keywords > $(SUBMAKE_OUT)/kwdhash.gen

.PRECIOUS: $(SUBMAKE_OUT)/%.elf

# Standard rule for {isolinux,isolinux-debug}.bin
$(SUBMAKE_OUT)/iso%.bin: $(SUBMAKE_OUT)/iso%.elf checksumiso.pl
	$(OBJCOPY) -O binary $< $@
	$(PERL) checksumiso.pl $@

# Standard rule for {ldlinux,pxelinux,extlinux}.bin
$(SUBMAKE_OUT)/%.bin: $(SUBMAKE_OUT)/%.elf
	$(OBJCOPY) -O binary $< $@

$(SUBMAKE_OUT)/%.o: %.asm $(SUBMAKE_OUT)/kwdhash.gen $(SUBMAKE_OUT)/../version.gen
	( $(NASM) -M -DDEPEND -I$(SUBMAKE_OUT)/ $(NINCLUDE) -o $@ $< ; echo '' ) > $@.d; true
	$(NASM) $(NASMOPT) -I$(SUBMAKE_OUT)/ -f elf -g -F stabs -DDATE_STR="'$(DATE)'" \
		-DHEXDATE="$(HEXDATE)" \
		-l $(@:.o=.lsr) -o $@ $<

$(SUBMAKE_OUT)/%.elf: $(SUBMAKE_OUT)/%.o syslinux.ld
	$(LD) $(LDFLAGS) -T syslinux.ld -M -o $@ $< > $(@:.elf=.map)
	$(OBJDUMP) -h $@ > $(@:.elf=.sec)
	$(PERL) lstadjust.pl $(@:.elf=.lsr) $(@:.elf=.sec) $(@:.elf=.lst)

$(SUBMAKE_OUT)/pxelinux.0: $(SUBMAKE_OUT)/pxelinux.bin
	cp -f $< $@

$(SUBMAKE_OUT)/ldlinux.bss: $(SUBMAKE_OUT)/ldlinux.bin
	dd if=$< of=$@ bs=512 count=1

$(SUBMAKE_OUT)/ldlinux.sys: $(SUBMAKE_OUT)/ldlinux.bin
	dd if=$< of=$@ bs=512 skip=1

$(SUBMAKE_OUT)/extlinux.bss: $(SUBMAKE_OUT)/extlinux.bin
	dd if=$< of=$@ bs=512 count=1

$(SUBMAKE_OUT)/extlinux.sys: $(SUBMAKE_OUT)/extlinux.bin
	dd if=$< of=$@ bs=512 skip=1

# NASM prior to 2.03 wouldn't auto-generate this dependency...
$(SUBMAKE_OUT)/ldlinux.o: $(SUBMAKE_OUT)/codepage.cp

$(SUBMAKE_OUT)/codepage.cp: $(SUBMAKE_OUT)/../codepage/$(CODEPAGE).cp
	cp -f $< $@

install: installer

install-lib: installer

install-all: install install-lib

netinstall: installer

tidy dist:
	rm -f codepage.cp *.o *.elf stupid.* patch.offset .depend .*.d
	rm -f *.lsr *.lst *.map *.sec
	rm -f $(OBSOLETE)

clean: tidy

spotless: clean
	rm -f $(BTARGET) *.bin *_bin.c

# Include dependencies file
-include .*.d
