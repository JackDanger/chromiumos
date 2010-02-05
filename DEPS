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

vars = {
  "chromium_revision": "36775",
}

deps = {
    # chromiumos-build
    "tools/chromiumos-build": "/chromiumos-build.git",

    # cros
    _platform_base + "cros": "/cros.git",

    # login_manager
    _platform_base + "login_manager": "/login_manager.git",

    # pam_google
    _platform_base + "pam_google": "/pam_google.git",

    # text_input
    _platform_base + "text_input": "/text_input.git",

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

    # multilingual support libraries
    _third_party_base + "m17n-lib": "/m17n-lib.git",

    # gflags 1.1
    _third_party_base + "gflags/files":
        "http://google-gflags.googlecode.com/svn/trunk@31",

    # google-breakpad
    _third_party_base + "google-breakpad/files":
        "http://google-breakpad.googlecode.com/svn/trunk@400",

    # gtest 1.3.0
    _third_party_base + "gtest/files":
        "http://googletest.googlecode.com/svn/trunk@327",

    # googlemock 1.4
    _third_party_base + "gmock/files":
        "http://googlemock.googlecode.com/svn/trunk@221",

    # gtk+2.0 - branch at 2.18.3, need to fix our clone somehow.
    _third_party_base + "gtk+2.0": "/gtkplus2.0.git",

    # hostap
    _third_party_base + "wpa_supplicant/hostap.git": "/hostap.git",

    # pam-dev
    _third_party_base + "pam-dev": "/pam-dev.git",

    # shflags 1.0.3
    _third_party_base + "shflags/files":
        "http://shflags.googlecode.com/svn/tags/1.0.3@137",

    # shunit2 2.1.5
    _third_party_base + "shunit2/files":
        "http://shunit2.googlecode.com/svn/tags/source/2.1.5@294",

    # syslinux
    _third_party_base + "syslinux/files": "/syslinux.git",

    # tpm-emulator
    _third_party_base + "tpm-emulator/files":
        "http://svn.berlios.de/svnroot/repos/tpm-emulator/trunk@341",

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

    # flimflam
    _third_party_base + "flimflam": "/flimflam.git",

    # gpt
    _third_party_base + "gpt": "/gpt.git",

    # cairo - branch at 1.8.8, need to fix our clone somehow.
    _third_party_base + "cairo": "/cairo.git",

    # gconf - branch at 2.28.0, need to fix our clone somehow.
    _third_party_base + "gconf": "/gconf.git",

    # curl
    _third_party_base + "curl": "/curl.git",

    # openssh
    _third_party_base + "openssh": "/openssh.git",

    # openssl
    _third_party_base + "openssl": "/openssl.git",

    # tzdata
    _third_party_base + "tzdata": "/tzdata.git",

    # xf86-video-msm from codeaurora.org
    _third_party_base + "xf86-video-msm": "/xf86-video-msm.git",

    # pango (i18n text rendering)
    _third_party_base + "pango": "/pango.git",

    # upstart
    _third_party_base + "upstart/files": "/upstart.git",

    # xserver-xorg-core
    _third_party_base + "xserver-xorg-core": "/xserver-xorg-core.git",

    # dhcp3
    _third_party_base + "dhcp3": "/dhcp3.git",

    # rsyslog
    _third_party_base + "rsyslog": "/rsyslog.git",

    # vim
    _third_party_base + "vim": "/vim.git",

    # dhcpcd
    _third_party_base + "dhcpcd/dhcpcd": "/dhcpcd.git",

    # dhcpcd-dbus
    _third_party_base + "dhcpcd/dhcpcd-dbus": "/dhcpcd-dbus.git",

    # anthy - japanese input
    _third_party_base + "anthy": "/anthy.git",

    # alsa-lib
    _third_party_base + "alsa-lib": "/alsa-lib.git",

    # u-boot
    _third_party_base + "u-boot/files": "/u-boot.git",

    # portage
    _third_party_base + "portage": "/portage.git",

    # chromiumos-overlay
    _third_party_base + "chromiumos-overlay": "/chromiumos-overlay.git",
}
