CXX=gcc
LDFLAGS=-ldl
CXXFLAGS=-Wall -Werror

# TODO: Clean this up once we figure out a scalable and automated 
# way of building tests that depend on other modules of the source tree.
BIN=pam_loader_ia32
OBJS=pam_loader.o

all:	$(BIN)

$(BIN):	$(OBJS)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(INCLUDE_DIRS) $(OBJS) -o $(BIN)
.c.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

clean:	
	rm -rf *.o $(BIN)
