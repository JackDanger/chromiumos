# Makefile for metrics client and library
#######################################################
CXX=/usr/bin/g++
CC=/usr/bin/gcc
CFLAGS=-Wall -I/usr/include -I../../
CXXFLAGS=$(CFLAGS)
#######################################################

CLIENT=metrics_client
LIBRARY=metrics_library
VERSION=0.1

DEFINES=-DPACKAGE=\"$(NAME)\" -DVERSION=\"$(VERSION)\"

CLIENT_OBJECTS=metrics_client.o

LIBRARY_OBJECTS=metrics_library.o
	
all: $(CLIENT) $(LIBRARY)

metrics_client: $(CLIENT_OBJECTS) $(LIBRARY)
	$(CXX) $(LDFLAGS) $(CLIENT_OBJECTS) libmetrics.a -o $(CLIENT)
	
metrics_library: $(LIBRARY_OBJECTS)
	ar rcs libmetrics.a $(LIBRARY_OBJECTS) 	
	
.cc.o:
	$(CXX) $(CXXFLAGS) $(DEFINES) -c $< -o $@

install:
	install $(CLIENT) $(DESTDIR)/usr/bin

clean:
	@rm -f $(CLIENT) *.o *.a
