// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRICSCLIENT_H_
#define METRICS_CLIENT_H_

#include <stdio.h>

// Client to both the Chrome User Metrics Server and Autotest test that
// takes performance and other user metrics from a running system

// Use -a flag for autotest, -c flag for chrome or anything else to send to both
class MetricsClient {
 public:
  // Parses a |input| for metrics to send to chrome and autotest
  static void ParseFile(FILE *input);
};

#endif /* METRICS_CLIENT_H_ */
