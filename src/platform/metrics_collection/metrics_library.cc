/*
 * metrics_library.cc
 *
 *  Created on: Dec 1, 2009
 *      Author: sosa
 */

#include "platform/metrics_collection/metrics_library.h"
#include <errno.h>
#include <sys/file.h>
#include <string.h>
#include <stdio.h>

const char kAutotestPath[] = "/tmp/.chromeos-metrics-autotest";
const char kChromePath[] = "/tmp/.chromeos-metrics";
const int kBufferSize = 4096;

using namespace std;

// TODO(sosa@chromium.org) - use Chromium logger instead of stderr
void MetricsLibrary::PrintError(const char *message, const char *file,
                               int code) {
  const char *kProgramName = "metrics_library";
  if (code == 0) {
    fprintf(stderr, "%s: %s\n", kProgramName, message);
  } else if (file == NULL) {
    fprintf(stderr, "%s: ", kProgramName);
    perror(message);
  } else {
    fprintf(stderr, "%s: %s: ", kProgramName, file);
    perror(message);
  }
}

void MetricsLibrary::SendToAutotest(string name, string value, FILE
                                   *autotest_file) {
  bool file_opened = false;
  char write_buffer[kBufferSize];
  if (!autotest_file) {
    autotest_file = fopen(kAutotestPath, "a+");
    if (autotest_file == NULL) {
      PrintError("fopen", kAutotestPath, errno);
      return;
    }
    file_opened = true;
  }
  snprintf(write_buffer, sizeof(write_buffer), "%s=%s\n",
           name.c_str(), value.c_str());
  fputs(write_buffer, autotest_file);
  if (file_opened) {
    fclose(autotest_file);
  }
}

void MetricsLibrary::SendToChrome(string name, string value,
                                 int chrome_file_descriptor) {
  // True if callee opened file, o/w false
  bool opened_file = false;
  // Check to see if the file descriptor passed in is valid
  if (chrome_file_descriptor < 0) {
    // If not, get a new file descriptor
    chrome_file_descriptor = open(kChromePath,
                                  O_WRONLY | O_APPEND | O_CREAT, 0666);
    opened_file = true;
    // If we failed to open it, return
    if (chrome_file_descriptor < 0) {
      PrintError("open", kChromePath, errno);
      return;
    }
  }
  // Only grab a lock if we opened the file
  if (opened_file) {
    // Grab an exclusive lock to protect Chrome from truncating underneath us
    if (flock(chrome_file_descriptor, LOCK_EX) < 0) {
      PrintError("flock", kChromePath, errno);
      close(chrome_file_descriptor);
      return;
    }
  }
  // Message format is: LENGTH (binary), NAME, VALUE
  char message[kBufferSize];
  char *curr_ptr = message;
  int32_t message_length =
      name.length() + value.length() + 2 + sizeof(message_length);
  if (message_length > sizeof(message))
    PrintError("name/value too long", NULL, 0);
  // Make sure buffer is blanked
  memset(message, 0, sizeof(message));
  memcpy(curr_ptr, &message_length, sizeof(message_length));
  curr_ptr += sizeof(message_length);
  strncpy(curr_ptr, name.c_str(), name.length());
  curr_ptr += name.length() + 1;
  strncpy(curr_ptr, value.c_str(), value.length());
  if (write(chrome_file_descriptor, message, message_length) != message_length)
    PrintError("write", kChromePath, errno);
  // Release the file lock and close file if callee opened file
  if (opened_file) {
    if (flock(chrome_file_descriptor, LOCK_UN) < 0)
      PrintError("unlock", kChromePath, errno);
    close(chrome_file_descriptor);
  }
}
