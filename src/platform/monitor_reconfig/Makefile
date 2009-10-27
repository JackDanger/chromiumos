# Makefile for monitor_reconfigure
#######################################################
CXX=/usr/bin/g++
CC=/usr/bin/gcc
CFLAGS=-Wall -I/usr/include -I../../
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lX11 -lXrandr
#######################################################

NAME=monitor_reconfigure
VERSION=0.1

DEFINES=-DPACKAGE=\"$(NAME)\" -DVERSION=\"$(VERSION)\"		

OBJECTS=monitor_reconfigure_main.o

all: monitor_reconfigure

monitor_reconfigure: $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $(NAME)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(DEFINES) -c $< -o $@

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@

install: monitor_reconfigure
	install -D -m 755 monitor_reconfigure $(DESTDIR)/usr/sbin/monitor_reconfigure	

clean:
	@rm -f monitor_reconfigure *.o
