// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/metrics_collection/metrics_client.h"
#include <errno.h>
#include <sys/file.h>
#include <string.h>
#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include "platform/metrics_collection/metrics_library.h"

using namespace std;

void MetricsClient::ParseFile(FILE *input) {
  // Open both Chrome and autotest files
  bool autotest_file_opened = true;
  bool chrome_file_opened = true;
  FILE *autotest_file = fopen(kAutotestPath, "a+");
  if (autotest_file == NULL) {
    MetricsLibrary::PrintError("fopen", kAutotestPath, errno);
    autotest_file_opened = false;
  }
  int chrome_file_descriptor = open(kChromePath,
                                    O_WRONLY | O_APPEND | O_CREAT, 0666);
  if (chrome_file_descriptor < 0) {
    MetricsLibrary::PrintError("open", kChromePath, errno);
    chrome_file_opened = false;
  } else if (flock(chrome_file_descriptor, LOCK_EX) < 0) {
    MetricsLibrary::PrintError("flock", kChromePath, errno);
      chrome_file_opened = false;
  }

  // Parse input file
  string name, value, flag;
  char read_buffer[kBufferSize];
  while (fgets(read_buffer, sizeof(read_buffer), input) != NULL) {
    char *token;  // current token
    char whitespace[] = " \n";  // whitespace identifier for tokenizing
    bool parse_error = false;  // parse error on current line
    for (int i = 0; (i < 3) && (!parse_error); i++) {
      switch (i) {
        case 0:
          token = strtok(read_buffer, whitespace);
          if (token != NULL)  name = token;
          else  parse_error = true;
          break;
        case 1:
          token = strtok(NULL, whitespace);
          if (token != NULL)  value = token;
          else  parse_error = true;
          break;
        case 2:
          token = strtok(NULL, whitespace);
          if (token != NULL)  flag = token;
          else  parse_error = true;
          break;
      }
    }
    if (parse_error) {
        // Skip line and continue processing
        MetricsLibrary::PrintError("Invalid format in input stream", "", 0);
        continue;
    }
    // Check the flag to see whether to send to chrome or autotest
    if ((flag.compare("c") == 0) && chrome_file_opened) {
      MetricsLibrary::SendToChrome(name, value, chrome_file_descriptor);
    } else if ((flag.compare("a") == 0) && autotest_file_opened) {
      MetricsLibrary::SendToAutotest(name, value, autotest_file);
    } else {
      // Default to both
      if (chrome_file_opened)
        MetricsLibrary::SendToChrome(name, value, chrome_file_descriptor);
      if (autotest_file_opened)
        MetricsLibrary::SendToAutotest(name, value, autotest_file);
    }
  }
  if (autotest_file_opened)
    fclose(autotest_file);
  if (chrome_file_opened) {
    if (flock(chrome_file_descriptor, LOCK_UN) < 0)
      MetricsLibrary::PrintError("unlock", kChromePath, errno);
    close(chrome_file_descriptor);
  }
}

// Usage:  metrics_client -[abc] metric_name metric_value
int main(int argc, char** argv) {
  bool send_to_autotest = true;
  bool send_to_chrome = true;
  bool use_stdin = false;
  int flag;
  int metric_name_index = 1;
  int metric_value_index = 2;

  if (argc == 1) {
    use_stdin = true;
  } else {
    // We have flags
    if (argc > 3) {
      send_to_chrome = false;
      send_to_autotest = false;
    }
    // Parse arguments
    while ((flag = getopt(argc, argv, "abc")) != -1) {
      switch (flag) {
        case 'a':
          send_to_autotest = true;
          break;
        case 'b':
          send_to_chrome = true;
          send_to_autotest = true;
          break;
        case 'c':
          send_to_chrome = true;
          break;
        default:
          cerr << "*** usage:  metrics_client -[ac] name value" << endl;
          exit(1);
          break;
      }
    }
    metric_name_index = optind;
    metric_value_index = optind + 1;
  }
  // Metrics value should be the last argument passed
  if ((metric_value_index + 1) != argc) {
      cerr << "*** usage:  metrics_client -[ac] name value" << endl;
      exit(1);
  }
  // Send metrics
  if (use_stdin) {
    MetricsClient::ParseFile(stdin);
  } else {
    if (send_to_autotest) {
      MetricsLibrary::SendToAutotest(argv[metric_name_index],
                                     argv[metric_value_index], NULL);
    }
    if (send_to_chrome) {
      MetricsLibrary::SendToChrome(argv[metric_name_index],
                                   argv[metric_value_index], -1);
    }
  }
  return 0;
}
