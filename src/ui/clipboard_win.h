// Aseprite UI Library
// Copyright (C) 2001-2015  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "base/string.h"

#include <algorithm>
#include <windows.h>

namespace {

void get_system_clipboard_text(std::string& text)
{
  if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
    if (OpenClipboard(win_get_window())) {
      HGLOBAL hglobal = GetClipboardData(CF_UNICODETEXT);
      if (hglobal != NULL) {
        LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
        if (lpstr != NULL) {
          text = base::to_utf8(lpstr).c_str();
          GlobalUnlock(hglobal);
        }
      }
      CloseClipboard();
    }
  }
}

void set_system_clipboard_text(const std::string& text)
{
  if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
    if (OpenClipboard(win_get_window())) {
      EmptyClipboard();

      if (!text.empty()) {
        std::wstring wstr = base::from_utf8(text);
        int len = wstr.size();

        HGLOBAL hglobal = GlobalAlloc(GMEM_MOVEABLE |
                                      GMEM_ZEROINIT, sizeof(WCHAR)*(len+1));

        LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
        std::copy(wstr.begin(), wstr.end(), lpstr);
        GlobalUnlock(hglobal);

        SetClipboardData(CF_UNICODETEXT, hglobal);
      }
      CloseClipboard();
    }
  }
}

}
