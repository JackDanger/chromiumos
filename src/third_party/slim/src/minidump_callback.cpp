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

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <string>

const char kMinidumpDir[] = "/var/log/login_manager_crashes/";

bool GetMTime(const std::string& filename, time_t *mtime_out) {
  struct stat filedata;
  if (!stat(filename.c_str(), &filedata) && S_ISREG(filedata.st_mode)) {
    *mtime_out = filedata.st_mtime;
    return true;
  }
  return false;
}

bool FilterCallback(void *context) {
  DIR *directory;
  struct dirent *entry;
  time_t deadline = time(NULL) - 12 * 60 * 60 /* 12h in seconds */;
  bool file_too_new = false;

  std::string crash_path(reinterpret_cast<const char *>(context));
  directory = opendir(crash_path.c_str());
  if (directory != NULL) {
    time_t file_mtime;
    while (entry = readdir(directory)) {
      if (GetMTime(crash_path + entry->d_name, &file_mtime) &&
          file_mtime > deadline) {
        file_too_new = true;
        break;
      }
    }
    closedir(directory);
  }

  return !file_too_new;
}

bool MinidumpCallback(const char *dump_path,
                      const char *minidump_id,
                      void *context,
                      bool succeeded) {

  return succeeded;
}
