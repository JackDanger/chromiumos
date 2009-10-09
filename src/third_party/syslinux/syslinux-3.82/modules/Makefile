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
## Non-COM32 simple Syslinux modules
##

ifndef SYSLINUX_OUT
$(error no syslinux out)
endif
SUBMAKE_OUT = $(SYSLINUX_OUT)/modules
export SUBMAKE_OUT
$(shell mkdir -p $(SUBMAKE_OUT))

topdir = ..
include $(topdir)/MCONFIG.embedded

INCLUDES   = -I$(com32)/include

BINS = $(SUBMAKE_OUT)/pxechain.com $(SUBMAKE_OUT)/gfxboot.com $(SUBMAKE_OUT)/poweroff.com

all: $(BINS)

.PRECIOUS: %.o

.PRECIOUS: %.elf
$(SUBMAKE_OUT)/%.elf: c32entry.o $(SUBMAKE_OUT)/%.o $(LIB)
	$(LD) -Ttext 0x101000 -e _start -o $@ $^

$(SUBMAKE_OUT)/%.c32: $(SUBMAKE_OUT)/%.elf
	$(OBJCOPY) -O binary $< $@

$(SUBMAKE_OUT)/%.com: %.asm
	ls -l gfxboot.map || true
	( $(NASM) -M -DDEPEND -o $@ $< ; echo '' ) > $(SUBMAKE_OUT)/.$(shell basename $@).d ; true
	ls -l gfxboot.map || true
	(cd $(SUBMAKE_OUT) && $(NASM) $(NASMOPT) -f bin -o $@ -l $(SUBMAKE_OUT)/$*.lst $(CURDIR)/$< )
	ls -l gfxboot.map || true

$(LIB): $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

$(SUBMAKE_OUT)/%.lss:	$(SUBMAKE_OUT)/%.ppm.gz $(PPMTOLSS16)
	$(GZIPPROG) -cd $< | \
		$(PPMTOLSS16) \#000000=0 \#d0d0d0=7 \#f6f6f6=15	\
		> $@

$(SUBMAKE_OUT)/%.ppm.gz: %.png
	$(PNGTOPNM) $< | gzip -9 > $@

tidy dist:
	rm -f *.o *.a *.lst *.elf *.map .*.d

clean: tidy

spotless: clean
	rm -f $(BINS)

-include $(SUBMAKE_OUT)/.*.d
