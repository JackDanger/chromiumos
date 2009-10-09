ifndef SYSLINUX_OUT
$(error no syslinux out)
endif
SUBMAKE_OUT = $(SYSLINUX_OUT)/mtools
export SUBMAKE_OUT
$(shell mkdir -p $(SUBMAKE_OUT))

topdir = ..
include $(topdir)/MCONFIG

OPTFLAGS = -g -Os
INCLUDES = -I. -I.. -I../libfat -I../libinstaller
CFLAGS	 = -W -Wall -D_FILE_OFFSET_BITS=64 $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = -s

SRCS     = syslinux.c \
	   ../libinstaller/syslxmod.c \
	   ../libinstaller/bootsect_bin.c \
	   ../libinstaller/ldlinux_bin.c \
	   $(wildcard ../libfat/*.c)
OBJS	 = $(patsubst %.c,$(SUBMAKE_OUT)/%.o,$(notdir $(SRCS)))

.SUFFIXES: .c .o .i .s .S

VPATH = .:../libfat:../libinstaller:$(SUBMAKE_OUT):$(SUBMAKE_OUT)/../libfat:$(SUBMAKE_OUT)/../libinstaller

all: installer

tidy dist:
	-rm -f *.o *.i *.s *.a .*.d *.tmp

clean: tidy
	-rm -f syslinux

spotless: clean
	-rm -f *~

installer: $(SUBMAKE_OUT)/syslinux

$(SUBMAKE_OUT)/syslinux: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(SUBMAKE_OUT)/%.o: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -c -o $@ $<
$(SUBMAKE_OUT)/%.i: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -E -o $@ $<
$(SUBMAKE_OUT)/%.s: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -S -o $@ $<

-include .*.d *.tmp
