// SHE library
// Copyright (C) 2012-2015  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "she/skia/skia_window_osx.h"

#include "she/event.h"
#include "she/event_queue.h"
#include "she/osx/window.h"
#include "gfx/size.h"

#if SK_SUPPORT_GPU

  #include "GrContext.h"
  #include "she/gl/gl_context_cgl.h"
  #include "she/skia/gl_context_skia.h"

#endif

namespace she {

class SkiaWindow::Impl : public OSXWindowImpl {
public:
  bool closing;
  int scale;
  OSXWindow* window;
#if SK_SUPPORT_GPU
  GLContextSkia<GLContextCGL> gl;
#endif

  Impl()
#if SK_SUPPORT_GPU
    : gl(nullptr)
#endif
  {
    closing = false;
    scale = 1;
    window = [[OSXWindow alloc] initWithImpl:this];
  }

  // OSXWindowImpl impl

  void onClose() override {
    closing = true;
  }
};

SkiaWindow::SkiaWindow(EventQueue* queue, SkiaDisplay* display)
  : m_impl(nullptr)
{
  dispatch_sync(
    dispatch_get_main_queue(),
    ^{
      m_impl = new SkiaWindow::Impl;
    });
}

SkiaWindow::~SkiaWindow()
{
  delete m_impl;
}

int SkiaWindow::scale() const
{
  return m_impl->scale;
}

void SkiaWindow::setScale(int scale)
{
  m_impl->scale = scale;
}

void SkiaWindow::setVisible(bool visible)
{
  if (m_impl->closing)
    return;

  dispatch_sync(dispatch_get_main_queue(), ^{
    if (visible) {
      [m_impl->window makeKeyAndOrderFront:nil];
    }
    else {
      [m_impl->window close];
    }
  });
}

void SkiaWindow::maximize()
{
}

bool SkiaWindow::isMaximized() const
{
  return false;
}

gfx::Size SkiaWindow::clientSize() const
{
  if (m_impl->closing)
    return gfx::Size(0, 0);

  return m_impl->window.clientSize;
}

gfx::Size SkiaWindow::restoredSize() const
{
  if (m_impl->closing)
    return gfx::Size(0, 0);

  return m_impl->window.restoredSize;
}

void SkiaWindow::setTitle(const std::string& title)
{
  if (m_impl->closing)
    return;

  dispatch_sync(dispatch_get_main_queue(), ^{
    [m_impl->window setTitle:[NSString stringWithUTF8String:title.c_str()]];
  });
}

void SkiaWindow::captureMouse()
{
}

void SkiaWindow::releaseMouse()
{
}

void SkiaWindow::setMousePosition(const gfx::Point& position)
{
}

void SkiaWindow::setNativeMouseCursor(NativeCursor cursor)
{
}

void SkiaWindow::updateWindow(const gfx::Rect& bounds)
{
}

void* SkiaWindow::handle()
{
  return (void*)m_impl->window;
}

} // namespace she
