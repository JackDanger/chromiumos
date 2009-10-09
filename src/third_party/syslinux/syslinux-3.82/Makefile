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

ifndef BUILD_ROOT
BUILD_ROOT = ../../../build
$(shell mkdir -p $(BUILD_ROOT))
endif
SYSLINUX_OUT := $(BUILD_ROOT)/x86/obj/src/third_party/syslinux/syslinux-3.82
$(shell mkdir -p $(SYSLINUX_OUT))
SYSLINUX_OUT := $(shell readlink -f $(SYSLINUX_OUT))
export SYSLINUX_OUT  # export to sub-make processes

#
# Main Makefile for SYSLINUX
#
topdir = .
include $(topdir)/MCONFIG
-include $(topdir)/version.mk

#
# The BTARGET refers to objects that are derived from ldlinux.asm; we
# like to keep those uniform for debugging reasons; however, distributors
# want to recompile the installers (ITARGET).
#
# BOBJECTS and IOBJECTS are the same thing, except used for
# installation, so they include objects that may be in subdirectories
# with their own Makefiles.  Finally, there is a list of those
# directories.
#

# List of module objects that should be installed for all derivatives
MODULES = $(SYSLINUX_OUT)/memdisk/memdisk $(SYSLINUX_OUT)/memdump/memdump.com $(SYSLINUX_OUT)/modules/*.com

# syslinux.exe is BTARGET so as to not require everyone to have the
# mingw suite installed
BTARGET  = $(SYSLINUX_OUT)/version.gen $(SYSLINUX_OUT)/version.h $(SYSLINUX_OUT)/version.mk
BOBJECTS = $(BTARGET) \
	$(SYSLINUX_OUT)/mbr/mbr.bin $(SYSLINUX_OUT)/mbr/altmbr.bin $(SYSLINUX_OUT)/mbr/gptmbr.bin \
	$(SYSLINUX_OUT)/mbr/mbr_c.bin $(SYSLINUX_OUT)/mbr/altmbr_c.bin $(SYSLINUX_OUT)/mbr/gptmbr_c.bin \
	$(SYSLINUX_OUT)/mbr/mbr_f.bin $(SYSLINUX_OUT)/mbr/altmbr_f.bin $(SYSLINUX_OUT)/mbr/gptmbr_f.bin \
	$(SYSLINUX_OUT)/core/pxelinux.0 $(SYSLINUX_OUT)/core/isolinux.bin $(SYSLINUX_OUT)/core/isolinux-debug.bin \
	$(MODULES)

# BSUBDIRs build the on-target binary components.
# ISUBDIRs build the installer (host) components.
#
# Note: libinstaller is both a BSUBDIR and an ISUBDIR.  It contains
# files that depend only on the B phase, but may have to be regenerated
# for "make installer".
BSUBDIRS = codepage core memdisk modules mbr memdump \
	   libinstaller
ITARGET  =
IOBJECTS = $(ITARGET) \
	$(SYSLINUX_OUT)/utils/gethostip $(SYSLINUX_OUT)/utils/isohybrid $(SYSLINUX_OUT)/utils/mkdiskimage \
	$(SYSLINUX_OUT)/mtools/syslinux $(SYSLINUX_OUT)/linux/syslinux $(SYSLINUX_OUT)/extlinux/extlinux
ISUBDIRS = libinstaller mtools linux extlinux utils

# Things to install in /usr/bin
INSTALL_BIN   =	mtools/syslinux
# Things to install in /sbin
INSTALL_SBIN  = extlinux/extlinux
# Things to install in /usr/lib/syslinux
INSTALL_AUX   =	core/pxelinux.0 gpxe/gpxelinux.0 core/isolinux.bin \
		core/isolinux-debug.bin \
		dos/syslinux.com dos/copybs.com win32/syslinux.exe \
		mbr/*.bin $(MODULES)
INSTALL_AUX_OPT = win32/syslinux.exe

# These directories manage their own installables
INSTALLSUBDIRS = com32 utils

# Things to install in /boot/extlinux
EXTBOOTINSTALL = $(MODULES)

# Things to install in /tftpboot
NETINSTALLABLE = core/pxelinux.0 gpxe/gpxelinux.0 \
		 $(MODULES)
all:
	$(MAKE) all-local
	set -e ; for i in $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done
	-ls -l $(BOBJECTS) $(IOBJECTS)

all-local: $(BTARGET) $(ITARGET)

installer:
	$(MAKE) installer-local
	set -e ; for i in $(ISUBDIRS); do $(MAKE) -C $$i all ; done
	-ls -l $(BOBJECTS) $(IOBJECTS)

installer-local: $(ITARGET) $(BINFILES)

$(SYSLINUX_OUT)/version.gen: version version.pl
	$(PERL) version.pl $< $@ '%define < @'
$(SYSLINUX_OUT)/version.h: version version.pl
	$(PERL) version.pl $< $@ '#define < @'
$(SYSLINUX_OUT)/version.mk: version version.pl
	$(PERL) version.pl $< $@ '< := @'

local-install: installer
	mkdir -m 755 -p $(INSTALLROOT)$(BINDIR)
	install -m 755 -c $(INSTALL_BIN) $(INSTALLROOT)$(BINDIR)
	mkdir -m 755 -p $(INSTALLROOT)$(SBINDIR)
	install -m 755 -c $(INSTALL_SBIN) $(INSTALLROOT)$(SBINDIR)
	mkdir -m 755 -p $(INSTALLROOT)$(AUXDIR)
	install -m 644 -c $(INSTALL_AUX) $(INSTALLROOT)$(AUXDIR)
	-install -m 644 -c $(INSTALL_AUX_OPT) $(INSTALLROOT)$(AUXDIR)
	mkdir -m 755 -p $(INSTALLROOT)$(MANDIR)/man1
	install -m 644 -c man/*.1 $(INSTALLROOT)$(MANDIR)/man1
	: mkdir -m 755 -p $(INSTALLROOT)$(MANDIR)/man8
	: install -m 644 -c man/*.8 $(INSTALLROOT)$(MANDIR)/man8

install: local-install
	set -e ; for i in $(INSTALLSUBDIRS) ; do $(MAKE) -C $$i $@ ; done

netinstall: installer
	mkdir -p $(INSTALLROOT)$(TFTPBOOT)
	install -m 644 $(NETINSTALLABLE) $(INSTALLROOT)$(TFTPBOOT)

extbootinstall: installer
	mkdir -m 755 -p $(INSTALLROOT)$(EXTLINUXDIR)
	install -m 644 $(EXTBOOTINSTALL) $(INSTALLROOT)$(EXTLINUXDIR)

install-all: install netinstall extbootinstall

local-tidy:
	rm -f *.o *.elf *_bin.c stupid.* patch.offset
	rm -f *.lsr *.lst *.map *.sec *.tmp
	rm -f $(OBSOLETE)

tidy: local-tidy
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-clean:
	rm -f $(ITARGET)

clean: local-tidy local-clean
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-dist:
	find . \( -name '*~' -o -name '#*' -o -name core \
		-o -name '.*.d' -o -name .depend \) -type f -print0 \
	| xargs -0rt rm -f

dist: local-dist local-tidy
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-spotless:
	rm -f $(BTARGET) .depend *.so.*

spotless: local-clean local-dist local-spotless
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

# Shortcut to build linux/syslinux using klibc
klibc:
	$(MAKE) clean
	$(MAKE) CC=klcc ITARGET= ISUBDIRS='linux extlinux' BSUBDIRS=

# Hook to add private Makefile targets for the maintainer.
-include Makefile.private
