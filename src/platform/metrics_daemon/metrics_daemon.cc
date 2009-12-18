// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/metrics_daemon/metrics_daemon.h"

#include <glib-object.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "marshal_void__string_boxed.h"
}

#include <base/logging.h>

#define SAFE_MESSAGE(e) ((e && e->message) ? e->message : "unknown error")
#define READ_WRITE_ALL_FILE_FLAGS \
  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

const char* MetricsDaemon::kMetricsFilePath = "/tmp/.chromeos-metrics";

MetricsDaemon::NetworkState
MetricsDaemon::network_states_[MetricsDaemon::kNumberNetworkStates] = {
#define STATE(name, capname) { #name, "Connman" # capname },
#include "network_states.h"
};

void MetricsDaemon::Run(bool run_as_daemon, bool testing) {
  Init(testing);
  if (!run_as_daemon || daemon(0, 0) == 0) {
    Loop();
  }
}

void MetricsDaemon::Init(bool testing) {
  testing_ = testing;
  network_state_id_ = kUnknownNetworkStateId;

  // Opens the file to communicate with Chrome, and keep it open.
  metrics_fd_ = open(kMetricsFilePath, O_WRONLY | O_APPEND | O_CREAT,
                     READ_WRITE_ALL_FILE_FLAGS);
  PLOG_IF(FATAL, metrics_fd_ < 0) << kMetricsFilePath;
  // Needs to chmod because open flags are anded with umask.
  int result = fchmod(metrics_fd_, READ_WRITE_ALL_FILE_FLAGS);
  PLOG_IF(FATAL, result < 0) << kMetricsFilePath << ": chmod";

  ::g_thread_init(NULL);
  ::g_type_init();
  ::dbus_g_thread_init();

  ::GError* error = NULL;
  ::DBusGConnection* dbc = ::dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  // Note that LOG(FATAL) terminates the process; otherwise we'd have to worry
  // about leaking |error|.
  LOG_IF(FATAL, dbc == NULL) <<
    "cannot connect to dbus: " << SAFE_MESSAGE(error);

  ::DBusGProxy* net_proxy = ::dbus_g_proxy_new_for_name(
      dbc, "org.moblin.connman", "/", "org.moblin.connman.Metrics");
  LOG_IF(FATAL, net_proxy == NULL) << "no dbus proxy for network";

#if 0
  // Unclear how soon one can call dbus_g_type_get_map().  Doing it before the
  // call to dbus_g_bus_get() results in a (non-fatal) assertion failure.
  // GetProperties returns a hash table.
  hashtable_gtype = ::dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
                                          G_TYPE_VALUE);
#endif

  dbus_g_object_register_marshaller(marshal_VOID__STRING_BOXED,
                                    G_TYPE_NONE,
                                    G_TYPE_STRING,
                                    G_TYPE_VALUE,
                                    G_TYPE_INVALID);  
  ::dbus_g_proxy_add_signal(net_proxy, "ConnectionStateChanged",
                            G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
  ::dbus_g_proxy_connect_signal(net_proxy, "ConnectionStateChanged",
                                G_CALLBACK(&StaticNetSignalHandler),
                                this, NULL);
}

void MetricsDaemon::Loop() {
  ::GMainLoop* loop = ::g_main_loop_new(NULL, false);
  ::g_main_loop_run(loop);
}

void MetricsDaemon::StaticNetSignalHandler(::DBusGProxy* proxy,
                                           const char* property,
                                           const ::GValue* value,
                                           void *data) {
  (static_cast<MetricsDaemon*>(data))->NetSignalHandler(proxy, property, value);
}

void MetricsDaemon::NetSignalHandler(::DBusGProxy* proxy,
                                     const char* property,
                                     const ::GValue* value) {
 
  if (strcmp("ConnectionState", property) != 0) {
    return;
  }

  const char* newstate = static_cast<const char*>(g_value_get_string(value));
  LogNetworkStateChange(newstate);
}

void MetricsDaemon::LogNetworkStateChange(const char* newstate) {
  NetworkStateId new_id = GetNetworkStateId(newstate);
  if (new_id == kUnknownNetworkStateId) {
    LOG(WARNING) << "unknown network connection state " << newstate;
    return;
  }
  NetworkStateId old_id = network_state_id_;
  if (new_id == old_id) {  // valid new state and no change
    return;
  }
  struct timeval now;
  if (gettimeofday(&now, NULL) != 0) {
    PLOG(WARNING) << "gettimeofday";
  }
  if (old_id != kUnknownNetworkStateId) {
    struct timeval diff;
    timersub(&now, &network_state_start_, &diff);
    // 
    int diff_ms = diff.tv_usec / 1000 + diff.tv_sec * 1000;
    // Saturates rather than overflowing.  We expect this to be statistically
    // insignificant, since INT_MAX milliseconds is 24.8 days.
    if (diff.tv_sec >= INT_MAX / 1000) {
      diff_ms = INT_MAX;
    }
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%d", diff_ms);
    if (testing_) {
      TestPublishMetric(network_states_[old_id].stat_name, buffer);
    } else {
      ChromePublishMetric(network_states_[old_id].stat_name, buffer);
    }
  }
  network_state_id_ = new_id;
  network_state_start_ = now;
}

MetricsDaemon::NetworkStateId
MetricsDaemon::GetNetworkStateId(const char* state_name) {
  for (int i = 0; i < kNumberNetworkStates; i++) {
    if (strcmp(state_name, network_states_[i].name) == 0) {
      return static_cast<NetworkStateId>(i);
    }
  }
  return static_cast<NetworkStateId>(-1);
}

// This code needs to be in a library, because there are (or will be) other
// users.
void MetricsDaemon::ChromePublishMetric(const char* name, const char* value) {
  int name_length;
  int value_length;
  char message[kMetricsMessageMaxLength];
  uint32_t message_length;

  if (flock(metrics_fd_, LOCK_EX) < 0) {
    // I am leaving commented-out error calls here (and later) for future
    // librarification, but it's not clear we want to log these as this service
    // is not essential, and there is the risk of spewing.
    // error("flock", path, errno);
    return;
  }

  // Message is: LENGTH (4 bytes), NAME, VALUE.
  // LENGTH includes the initial 4 bytes.

  name_length = strlen(name);
  value_length = strlen(value);
  message_length = name_length + value_length + 2 + sizeof(message_length);

  if (message_length > sizeof(message)) {
    // some error here
    return;
  }

  *reinterpret_cast<uint32_t*>(&message) = message_length; // same endianness

  strcpy(message + 4, name);
  strcpy(message + 4 + name_length + 1, value);

  if (write(metrics_fd_, message, message_length) !=
      static_cast<int>(message_length)) {
    // error("write", path, errno);
    return;
  }

  if (flock(metrics_fd_, LOCK_UN) < 0) {
    // error("unlock", path, errno);
    return;
  }
}

void MetricsDaemon::TestPublishMetric(const char* name, const char* value) {
  LOG(INFO) << "received metric: " << name << " " << value;
}
