# Makefile for slim - generic Linux
#
# Edit the following section to adjust the options
# to fit into your operating system / distribution
#######################################################
CXX=/usr/bin/g++
CC=/usr/bin/gcc
BREAKPAD_DIR=../../google-breakpad/files/src/client/linux
CFLAGS=-Wall -I. -I/usr/include/freetype2 -I/usr/include/freetype2/config -I/usr/include/libpng12 -I/usr/include -I$(BREAKPAD_DIR)
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lXft -lX11 -lpng12 -lm -lXmu -lpng -ljpeg
CUSTOM=-DHAVE_SHADOW
ifdef USE_PAM
LDFLAGS+= -lpam
CUSTOM+= -DUSE_PAM
endif
PREFIX=/usr
CFGDIR=/etc
MANDIR=/usr/share/man
DESTDIR=
LOGDIR=/var/log/login_manager_crashes
#######################################################

NAME=slim
VERSION=1.3.1
TESTNAME=$(NAME)_unittests

DEFINES=-DPACKAGE=\"$(NAME)\" -DVERSION=\"$(VERSION)\" \
		-DPKGDATADIR=\"$(PREFIX)/share/slim\" -DSYSCONFDIR=\"$(CFGDIR)\"

OBJECTS=jpeg.o png.o main.o image.o numlock.o cfg.o switchuser.o app.o panel.o minidump_callback.o
ifdef USE_PAM
OBJECTS+=PAM.o
endif

TEST_OBJECTS=minidump_callback_unittest.o minidump_callback.o slim_testrunner.o

all: slim

slim: $(OBJECTS) breakpad
	$(CXX) $(LDFLAGS) $(OBJECTS) $(BREAKPAD_DIR)/libbreakpad.a -o $(NAME)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(DEFINES) $(CUSTOM) -c $< -o $@

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(CUSTOM) -c $< -o $@

install: slim install-theme
	install -D -m 755 slim $(DESTDIR)$(PREFIX)/bin/slim
	install -D -m 644 slim.1 $(DESTDIR)$(MANDIR)/man1/slim.1
	mkdir -p $(DESTDIR)$(LOGDIR)
	test -e $(DESTDIR)$(CFGDIR)/slim.conf || \
		install -D -m 644 slim.conf $(DESTDIR)$(CFGDIR)/slim.conf

clean:
	@rm -f slim *.o

dist:
	@rm -rf $(NAME)-$(VERSION)
	@mkdir $(NAME)-$(VERSION)
	@cp -r *.cpp *.h *.c Makefile Makefile.* COPYING ChangeLog INSTALL README TODO \
		xinitrc.sample slim.1 THEMES.README themes slim.conf $(NAME)-$(VERSION)
	@rm -rf $(NAME)-$(VERSION)/themes/.svn	$(NAME)-$(VERSION)/themes/default/.svn
	@tar cvzf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION)
	@rm -rf $(NAME)-$(VERSION)


install-theme:
	install -D -m 644 themes/default/slim.theme \
		 $(DESTDIR)$(PREFIX)/share/slim/themes/default/slim.theme
	install -D -m 644 themes/default/panel.png \
		 $(DESTDIR)$(PREFIX)/share/slim/themes/default/panel.png
	install -D -m 644 themes/default/background.jpg \
		 $(DESTDIR)$(PREFIX)/share/slim/themes/default/background.jpg

breakpad: $(BREAKPAD_DIR)/Makefile
	cd $(BREAKPAD_DIR) && make libbreakpad.a

unittests: $(TEST_OBJECTS)
	$(CXX) $(LDFLAGS) -lgtest $(TEST_OBJECTS) -o $(TESTNAME)

check: unittests
	./$(TESTNAME)
