## -----------------------------------------------------------------------
##
##   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# SYSLINUX utilities
#

ifndef SYSLINUX_OUT
$(error no syslinux out)
endif
SUBMAKE_OUT = $(SYSLINUX_OUT)/utils
export SUBMAKE_OUT
$(shell mkdir -p $(SUBMAKE_OUT))

topdir = ..
include $(topdir)/MCONFIG

CFLAGS   = -W -Wall -Os -fomit-frame-pointer -D_FILE_OFFSET_BITS=64
LDFLAGS  = -O2 -s

TARGETS	 = $(SUBMAKE_OUT)/mkdiskimage $(SUBMAKE_OUT)/isohybrid $(SUBMAKE_OUT)/gethostip
ASIS     = $(SUBMAKE_OUT)/keytab-lilo $(SUBMAKE_OUT)/lss16toppm $(SUBMAKE_OUT)/md5pass $(SUBMAKE_OUT)/ppmtolss16 $(SUBMAKE_OUT)/sha1pass $(SUBMAKE_OUT)/syslinux2ansi

ISOHDPFX = $(SUBMAKE_OUT)/../mbr/isohdpfx.bin $(SUBMAKE_OUT)/../mbr/isohdpfx_f.bin $(SUBMAKE_OUT)/../mbr/isohdpfx_c.bin \
	   $(SUBMAKE_OUT)/../mbr/isohdppx.bin $(SUBMAKE_OUT)/../mbr/isohdppx_f.bin $(SUBMAKE_OUT)/../mbr/isohdppx_c.bin

all: $(TARGETS)

$(SUBMAKE_OUT)/%.o: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -c -o $@ $<

$(SUBMAKE_OUT)/mkdiskimage: mkdiskimage.in $(SUBMAKE_OUT)/../mbr/mbr.bin bin2hex.pl
	$(PERL) bin2hex.pl < ../mbr/mbr.bin | cat mkdiskimage.in - > $@
	chmod a+x $@

$(SUBMAKE_OUT)/isohybrid: isohybrid.in $(ISOHDPFX) bin2hex.pl
	cp -f isohybrid.in $@
	for f in $(ISOHDPFX) ; do $(PERL) bin2hex.pl < $$f >> $@ ; done
	chmod a+x $@

$(SUBMAKE_OUT)/gethostip: $(SUBMAKE_OUT)/gethostip.o
	$(CC) $(LDFLAGS) -o $@ $^

tidy dist:
	rm -f *.o .*.d

clean: tidy
	rm -f $(TARGETS)

spotless: clean

installer: all

install: installer
	mkdir -m 755 -p $(INSTALLROOT)$(BINDIR)
	install -m 755 $(TARGETS) $(ASIS) $(INSTALLROOT)$(BINDIR)

-include .*.d
