ifndef SYSLINUX_OUT
$(error no syslinux out)
endif
SUBMAKE_OUT = $(SYSLINUX_OUT)/codepage
export SUBMAKE_OUT
$(info submake out top: $(SUBMAKE_OUT))
$(shell mkdir -p $(SUBMAKE_OUT))
PERL		= perl
CPSRC		= $(wildcard *.txt)
GENFILES	= $(patsubst %.txt,$(SUBMAKE_OUT)/%.cp,$(CPSRC))

.SUFFIXES: .txt .cp

all: $(GENFILES)

# This generates codepage files where the display and filesystem
# codepages are both the same.
$(SUBMAKE_OUT)/%.cp: %.txt cptable.pl UnicodeData
	$(PERL) cptable.pl UnicodeData $< $< $@

tidy:
	rm -f *.cp *.bin

clean: tidy

dist: tidy

spotless: clean
