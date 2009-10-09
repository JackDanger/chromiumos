## -----------------------------------------------------------------------
##
##   Copyright 2007-2009 H. Peter Anvin - All Rights Reserved
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
# Makefile for MBR
#

ifndef SYSLINUX_OUT
$(error no syslinux out)
endif
SUBMAKE_OUT = $(SYSLINUX_OUT)/mbr
export SUBMAKE_OUT
$(shell mkdir -p $(SUBMAKE_OUT))

topdir = ..
include $(topdir)/MCONFIG.embedded

all:	$(SUBMAKE_OUT)/mbr.bin   $(SUBMAKE_OUT)/altmbr.bin   $(SUBMAKE_OUT)/gptmbr.bin   $(SUBMAKE_OUT)/isohdpfx.bin   $(SUBMAKE_OUT)/isohdppx.bin \
	$(SUBMAKE_OUT)/mbr_c.bin $(SUBMAKE_OUT)/altmbr_c.bin $(SUBMAKE_OUT)/gptmbr_c.bin $(SUBMAKE_OUT)/isohdpfx_c.bin $(SUBMAKE_OUT)/isohdppx_c.bin \
	$(SUBMAKE_OUT)/mbr_f.bin $(SUBMAKE_OUT)/altmbr_f.bin $(SUBMAKE_OUT)/gptmbr_f.bin $(SUBMAKE_OUT)/isohdpfx_f.bin $(SUBMAKE_OUT)/isohdppx_f.bin

$(SUBMAKE_OUT)/%.o: %.S
	$(CC) $(MAKEDEPS) $(SFLAGS) -Wa,-a=$(SUBMAKE_OUT)/$*.lst -c -o $@ $<

$(SUBMAKE_OUT)/%_c.o: %.S
	$(CC) $(MAKEDEPS) $(SFLAGS) -Wa,-a=$(SUBMAKE_OUT)/$*_c.lst -DCTRL_80 -c -o $@ $<

$(SUBMAKE_OUT)/%_f.o: %.S
	$(CC) $(MAKEDEPS) $(SFLAGS) -Wa,-a=$(SUBMAKE_OUT)/$*_f.lst -DFORCE_80 -c -o $@ $<

.PRECIOUS: %.elf
$(SUBMAKE_OUT)/%.elf: $(SUBMAKE_OUT)/%.o mbr.ld
	$(LD) $(LDFLAGS) -T mbr.ld -e _start -o $@ $<

$(SUBMAKE_OUT)/%.bin: $(SUBMAKE_OUT)/%.elf checksize.pl
	$(OBJCOPY) -O binary $< $@
	$(PERL) checksize.pl $@
	$(CHMOD) -x $@

$(SUBMAKE_OUT)/mbr_bin.c: $(SUBMAKE_OUT)/mbr.bin

tidy dist:
	rm -f *.o *.elf *.lst .*.d

clean: tidy

spotless: clean
	rm -f *.bin

-include .*.d
