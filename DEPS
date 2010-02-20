# -*- python -*-

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# NOTES
#
# This file must exist both in trunk and trunk/src, since some users sync
# trunk and some sync src.  (In the end, src will win, since src-internal will
# be in a different repo.)
#
# Source packages should go into
#    _third_party_base + "PACKAGENAME/files"
# so that we can put our build wrapper and/or patches for each package into
#    _third_party_base + "PACKAGENAME"
# (DEPS must point to empty subdirectories)
#
# No trailing backslash when specifying SVN paths.  That confuses gclient and
# causes it to re-download the files each time.

use_relative_paths=True
use_relative_urls2=True

# Base is prefixed with "src/" if this is trunk/DEPS, or "" if this
# is trunk/src/DEPS.
_third_party_base = "src/third_party/"
_platform_base = "src/platform/"
_overlays_base = "src/overlays/"

vars = {
  "chromium_revision": "36775",
}

deps = {
    # cros
    _platform_base + "cros": "/cros.git",

    # login_manager
    _platform_base + "login_manager": "/login_manager.git",

    # pam_google
    _platform_base + "pam_google": "/pam_google.git",

    # autotest
    _third_party_base + "autotest/files": "/autotest.git",

    # IBus framework
    _third_party_base + "ibus/files": "/ibus.git",

    # IBus input method for Traditional Chinese
    _third_party_base + "ibus-chewing/files": "/ibus-chewing.git",

    # IBus input method for Simplified Chinese
    _third_party_base + "ibus-pinyin/files": "/ibus-pinyin.git",

    # IBus input method for Japanese
    _third_party_base + "ibus-anthy/files": "/ibus-anthy.git",

    # IBus input method for Korean
    _third_party_base + "ibus-hangul/files": "/ibus-hangul.git",

    # IBus input method for many languages (e.g. Thai)
    _third_party_base + "ibus-m17n/files": "/ibus-m17n.git",

    # google-breakpad
    _third_party_base + "google-breakpad/files":
        "http://google-breakpad.googlecode.com/svn/trunk@400",

    # pam-dev
    _third_party_base + "pam-dev": "/pam-dev.git",

    # shflags 1.0.3
    _third_party_base + "shflags/files":
        "http://shflags.googlecode.com/svn/tags/1.0.3@137",

    # shunit2 2.1.5
    _third_party_base + "shunit2/files":
        "http://shunit2.googlecode.com/svn/tags/source/2.1.5@294",

    # tpm-emulator
    _third_party_base + "tpm-emulator": "/tpm-emulator.git",

    # trousers
    _third_party_base + "trousers": "/trousers.git",

    # chrome-base
    _third_party_base + "chrome/files/base":
        "http://src.chromium.org/svn/trunk/src/base@" +
        Var("chromium_revision"),
    _third_party_base + "chrome/files/build":
        "http://src.chromium.org/svn/trunk/src/build@" +
        Var("chromium_revision"),

    # kernel
    _third_party_base + "kernel/files": "/kernel.git",

    # ARM kernel
    _third_party_base + "kernel-qualcomm": "/kernel.git@origin/qualcomm-2.6.31.12",

    # flimflam
    _third_party_base + "flimflam": "/flimflam.git",

    # gpt
    _third_party_base + "gpt": "/gpt.git",

    # xf86-video-msm from codeaurora.org
    _third_party_base + "xf86-video-msm": "/xf86-video-msm.git",

    # upstart
    _third_party_base + "upstart/files": "/upstart.git",

    # dhcpcd
    _third_party_base + "dhcpcd/dhcpcd": "/dhcpcd.git",

    # dhcpcd-dbus
    _third_party_base + "dhcpcd/dhcpcd-dbus": "/dhcpcd-dbus.git",

    # u-boot
    _third_party_base + "u-boot/files": "/u-boot.git",

    # vectormath from o3d
    _third_party_base + "vectormath":
        "http://o3d.googlecode.com/svn/trunk/googleclient/third_party/vectormath@166",

    # wpa_supplicant + hostap 
    _third_party_base + "wpa_supplicant/hostap.git": "/hostap.git",

    # portage
    _third_party_base + "portage": "/portage.git",

    # chromiumos-overlay
    _third_party_base + "chromiumos-overlay": "/chromiumos-overlay.git",

    # laptop mode tools
    _third_party_base + "laptop-mode-tools": "/laptop-mode-tools.git",

    # board overlays, please keep sorted alphabetically
    _overlays_base + "overlay-arm-generic": "/overlay-arm-generic.git",
    _overlays_base + "overlay-beagleboard": "/overlay-beagleboard.git",
    _overlays_base + "overlay-st1q":        "/overlay-st1q.git",
    _overlays_base + "overlay-x86-generic": "/overlay-x86-generic.git",
}
