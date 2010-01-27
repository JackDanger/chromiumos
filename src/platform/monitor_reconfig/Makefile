# Makefile for monitor_reconfigure
#######################################################
CXX ?= /usr/bin/g++
CC ?= /usr/bin/gcc
CPPFLAGS += -I../..
CFLAGS += -Wall -g -O2
CXXFLAGS=$(CFLAGS)
LDFLAGS += -lX11 -lXrandr
#######################################################

NAME=monitor_reconfigure
VERSION=0.1

CPPFLAGS+=-DPACKAGE=\"$(NAME)\" -DVERSION=\"$(VERSION)\"

OBJECTS=monitor_reconfigure_main.o

all: monitor_reconfigure

monitor_reconfigure: $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

install: monitor_reconfigure
	install -D -m 755 monitor_reconfigure $(DESTDIR)/usr/sbin/monitor_reconfigure	

clean:
	@rm -f monitor_reconfigure *.o
