CXX=g++
LDFLAGS=-lpam
CXXFLAGS=-Wall -Werror

# TODO(gauravsh): Clean this up once we figure out a scalable and automated 
# way of building tests that depend on other modules of the source tree.
#
# Similarly, the pam_client.cc and pam_client.h used by the test are
# copied over from platform/login_manager/chromeos_login_manager since
# the original files use include paths relative to third_party/ in the
# Chromium source tree. Ideally, we would want to directly refer to
# the original files, with a patch applied to use the correct include
# paths for building the test.

SRCDIR=../../../../../
INCLUDE_DIRS=-I$(SRCDIR)/src/third_party/chrome/files 

AUTH_ENV_TEST_BIN=login_AuthenticationEnvVarsTest
AUTH_ENV_TEST_OBJS=login_AuthenticationEnvVarsTest.o pam_client.o

all:	$(AUTH_ENV_TEST_BIN)

$(AUTH_ENV_TEST_BIN):	$(AUTH_ENV_TEST_OBJS)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(INCLUDE_DIRS) $(AUTH_ENV_TEST_OBJS) \
	-o $(AUTH_ENV_TEST_BIN)
.cc.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

clean:	
	rm -rf *.o $(AUTH_ENV_TEST_BIN)
