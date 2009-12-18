# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CCC = g++
CC = gcc

INCLUDES = -I../.. -I../../third_party/chrome/files -I../../common
COPT = -g

CCONFIG = $(shell pkg-config --cflags dbus-1 glib-2.0 dbus-glib-1)
LDCONFIG = $(shell pkg-config --libs dbus-1 glib-2.0 gthread-2.0 dbus-glib-1)

CFLAGS = $(COPT) -fno-exceptions -Wall -Werror $(INCLUDES) $(CCONFIG)
LDFLAGS = -L../../third_party/chrome $(LDCONFIG) -lrt -lbase -lpthread -lgflags

COMMONSRC = metrics_daemon.cc
COMMONCSRC = marshal_void__string_boxed.c
SRC = main.cc

TESTSRC = metrics_daemon_unittest.cc
TESTLIB = -lgflags -lgtest

OBJ = $(SRC:.cc=.o)
COMMONOBJ = $(COMMONSRC:.cc=.o) $(COMMONCSRC:.c=.o)
TESTOBJ = $(TESTSRC:.cc=.o)

%.o : %.cc
	$(CCC) -c $(CFLAGS) $< -o $@

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.c : %.list
	glib-genmarshal --body --prefix=marshal $< > $@

%.h : %.list
	glib-genmarshal --header --prefix=marshal $< > $@

DAEMON = metrics_daemon
TEST = test_daemon

all: $(DAEMON) $(TEST)

metrics_daemon: $(OBJ) $(COMMONOBJ)
	$(CCC) -o metrics_daemon $(OBJ) $(COMMONOBJ) $(LDFLAGS)

test_daemon: $(TESTOBJ) $(COMMONOBJ)
	$(CCC) -o test_daemon $(TESTOBJ) $(COMMONOBJ) $(LDFLAGS) $(TESTLIB)

# dependencies in addition to those defined by the rules

metrics_daemon.o: metrics_daemon.h marshal_void__string_boxed.h network_states.h
metrics_daemon_unittest.o: metrics_daemon.h marshal_void__string_boxed.h \
                           network_states.h
marshal_void__string_boxed.o: marshal_void__string_boxed.h

.PRECIOUS: marshal_void__string_boxed.c  # keep around for debugging

install:
	mkdir -p $(DESTDIR)/usr/bin
	install $(DAEMON) $(DESTDIR)/usr/bin

clean:
	rm -f *.o marshal_void__string_boxed.[ch] \
	metrics_daemon test_daemon *~
