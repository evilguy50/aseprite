// Aseprite
// Copyright (C) 2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifndef APP_TOOLS_SYMMETRY_H_INCLUDED
#define APP_TOOLS_SYMMETRY_H_INCLUDED
#pragma once

#include "app/tools/stroke.h"

#include <vector>

namespace app {
  namespace tools {

    typedef std::vector<Stroke> Strokes;

    // This class controls user input.
    class Symmetry {
    public:
      virtual ~Symmetry() { }
      virtual void generateStrokes(const Stroke& stroke, Strokes& strokes) = 0;
    };

  } // namespace tools
} // namespace app

#endif
