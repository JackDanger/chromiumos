#!/bin/bash

# Each package should list its hard-coded UIDs/GIDs here. We are starting at
# 200 and going up
DBUS_UID=200
DBUS_GID=200
NTP_UID=201
NTP_GID=201

# Call with these params:
# username (e.g. "messagebus")
# UID (e.g. 200)
# GID (e.g. 200)
# full name (e.g. "")
# home dir (e.g. "/home/foo" or "/var/run/dbus")
# shell (e.g. "/bin/sh" or "/bin/false")
add_user() {
  echo "${1}:x:${2}:${3}:${4}:${5}:${6}" | \
    dd of="${ROOT}/etc/passwd" conv=notrunc oflag=append
}

# Call with these params:
# groupname (e.g. "messagebus")
# GID (e.g. 200)
add_group() {
  echo "${1}:x:${2}:" | dd of="${ROOT}/etc/group" conv=notrunc oflag=append
}
