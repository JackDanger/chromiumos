/* SLiM - Simple Login Manager
   Copyright (C) 1997, 1998 Per Liden
   Copyright (C) 2004-06 Simone Rota <sip@varlock.com>
   Copyright (C) 2004-06 Johannes Winkelmann <jw@tks6.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include "minidump_callback.h"

#include <gtest/gtest.h>
#include <time.h>
#include <string>

class MinidumpCallbackTest : public testing::Test { };

TEST(MinidumpCallbackTest, GetMTimeTest) {
  time_t mytime;
  EXPECT_TRUE(GetMTime("./minidump_callback_unittest.cpp", &mytime));
}

TEST(MinidumpCallbackTest, FilterCallback) {
  EXPECT_FALSE(FilterCallback(const_cast<char*>("./")));
}
