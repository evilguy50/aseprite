// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef __ASE_CONFIG_H
#error You cannot use config.h two times
#endif

#define __ASE_CONFIG_H

// In MSVC
#ifdef _MSC_VER
  // Avoid warnings about insecure standard C++ functions
  #define _CRT_SECURE_NO_WARNINGS

  // Disable warning C4355 in MSVC: 'this' used in base member initializer list
  #pragma warning(disable:4355)
#endif

// General information
#define PACKAGE "Aseprite"
#define VERSION "1.1.2-dev"

#ifdef CUSTOM_WEBSITE_URL
#define WEBSITE                 CUSTOM_WEBSITE_URL // To test web server
#else
#define WEBSITE                 "http://www.aseprite.org/"
#endif
#define WEBSITE_DOWNLOAD        WEBSITE "download/"
#define WEBSITE_CONTRIBUTORS    WEBSITE "contributors/"
#define WEBSITE_NEWS_RSS        "http://blog.aseprite.org/rss"
#define UPDATE_URL              WEBSITE "update/?xml=1"
#define COPYRIGHT               "Copyright (C) 2001-2015 David Capello"

#define LOG                     verbose_log
#ifdef _DEBUG
#define DLOG                    LOG
#else
#define DLOG(...)               ((void)0)
#endif

// verbose_log() is defined in src/app/log.cpp and used through LOG macro
void verbose_log(const char* format, ...);

#include "base/base.h"
#include "base/debug.h"
