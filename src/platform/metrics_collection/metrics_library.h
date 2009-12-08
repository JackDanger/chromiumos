/*
 * metrics_library.h
 *
 *  Created on: Dec 1, 2009
 *      Author: sosa
 */

#ifndef METRICS_LIBRARY_H_
#define METRICS_LIBRARY_H_

#include <stdio.h>
#include <string>

extern const char kChromePath[];
extern const char kAutotestPath[];
extern const int kBufferSize;

// TODO(sosa@chromium.org): Add testing for send methods

// Library used to send metrics both autotest and chrome
class MetricsLibrary {
public:
  // Sends to Chrome via a file.  If |file_descriptor| is valid, uses that
  // file descriptor, otherwise, opens a new one and closes it
  static void SendToChrome(std::string name, std::string value, int file_descriptor);
  // Sends to Autotest via a file.  If |file| is non-null, uses that file o/w
  // SendToAutotest opens its own file, writes, and closes it
  static void SendToAutotest(std::string name, std::string value, FILE* file);
  // Prints message to stderr
  static void PrintError(const char *message, const char *file, int code);
};

#endif /* METRICS_LIBRARY_H_ */
