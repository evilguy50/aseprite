// Aseprite
// Copyright (C) 2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/font_path.h"

#include "base/fs.h"

#include <queue>

namespace app {

std::vector<std::string> g_cache;

void get_font_dirs(std::vector<std::string>& fontDirs)
{
  if (!g_cache.empty()) {
    fontDirs = g_cache;
    return;
  }

  std::queue<std::string> q;
  q.push("~/.fonts");
  q.push("/usr/local/share/fonts");
  q.push("/usr/share/fonts");

  while (!q.empty()) {
    std::string fontDir = q.front();
    q.pop();

    fontDirs.push_back(fontDir);

    for (const auto& file : base::list_files(fontDir)) {
      std::string fullpath = base::join_path(fontDir, file);
      if (base::is_directory(fullpath))
        q.push(fontDir); // Add subdirectory in the queue
    }
  }

  g_cache = fontDirs;
}

} // namespace app
