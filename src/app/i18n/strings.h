// Aseprite
// Copyright (C) 2016-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_I18N_STRINGS_INCLUDED
#define APP_I18N_STRINGS_INCLUDED
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "strings.ini.h"

namespace app {

  // Singleton class to load and access "strings/en.ini" file.
  class Strings : public app::gen::Strings<app::Strings> {
  public:
    static Strings* instance();

    const std::string& translate(const char* id) const;

  private:
    Strings();

    mutable std::unordered_map<std::string, std::string> m_strings;
  };

} // namespace app

#endif
