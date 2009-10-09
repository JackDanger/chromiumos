ifndef SYSLINUX_OUT
$(error no syslinux out)
endif
SUBMAKE_OUT = $(SYSLINUX_OUT)/libinstaller
export SUBMAKE_OUT
$(shell mkdir -p $(SUBMAKE_OUT))

# _bin.c files required by both BTARGET and ITARGET installers
BINFILES = $(SUBMAKE_OUT)/bootsect_bin.c $(SUBMAKE_OUT)/ldlinux_bin.c \
	   $(SUBMAKE_OUT)/extlinux_bss_bin.c $(SUBMAKE_OUT)/extlinux_sys_bin.c \
	   $(SUBMAKE_OUT)/mbr_bin.c $(SUBMAKE_OUT)/gptmbr_bin.c

PERL	 = perl

all: $(BINFILES)

$(SUBMAKE_OUT)/bootsect_bin.c: $(SUBMAKE_OUT)/../core/ldlinux.bss bin2c.pl
	$(PERL) bin2c.pl syslinux_bootsect < $< > $@

$(SUBMAKE_OUT)/ldlinux_bin.c: $(SUBMAKE_OUT)/../core/ldlinux.sys bin2c.pl
	$(PERL) bin2c.pl syslinux_ldlinux < $< > $@

$(SUBMAKE_OUT)/extlinux_bss_bin.c: $(SUBMAKE_OUT)/../core/extlinux.bss bin2c.pl
	$(PERL) bin2c.pl extlinux_bootsect < $< > $@

$(SUBMAKE_OUT)/extlinux_sys_bin.c: $(SUBMAKE_OUT)/../core/extlinux.sys bin2c.pl
	$(PERL) bin2c.pl extlinux_image 512 < $< > $@

$(SUBMAKE_OUT)/mbr_bin.c: $(SUBMAKE_OUT)/../mbr/mbr.bin bin2c.pl
	$(PERL) bin2c.pl syslinux_mbr < $< > $@

$(SUBMAKE_OUT)/gptmbr_bin.c: $(SUBMAKE_OUT)/../mbr/gptmbr.bin bin2c.pl
	$(PERL) bin2c.pl syslinux_gptmbr < $< > $@

tidy:
	rm -f $(BINFILES)

clean: tidy

dist: tidy

spotless: clean
