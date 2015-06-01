// SHE library
// Copyright (C) 2012-2015  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "she/skia/skia_display.h"

#include "she/system.h"

namespace she {

SkiaDisplay::SkiaDisplay(EventQueue* queue, int width, int height, int scale)
  : m_window(instance()->eventQueue(), this)
  , m_surface(new SkiaSurface)
  , m_customSurface(false)
  , m_nativeCursor(kArrowCursor)
{
  m_surface->create(width, height);
  m_window.setScale(scale);
  m_window.setVisible(true);
  m_recreated = false;
}

void SkiaDisplay::setSkiaSurface(SkiaSurface* surface)
{
  m_surface->dispose();
  m_surface = surface;
  m_customSurface = true;
}

void SkiaDisplay::resize(const gfx::Size& size)
{
  if (m_customSurface)
    return;

  m_surface->dispose();
  m_surface = new SkiaSurface;
  m_surface->create(size.w, size.h);
  m_recreated = true;
}

void SkiaDisplay::dispose()
{
  delete this;
}

int SkiaDisplay::width() const
{
  return m_window.clientSize().w;
}

int SkiaDisplay::height() const
{
  return m_window.clientSize().h;
}

int SkiaDisplay::originalWidth() const
{
  return m_window.restoredSize().w;
}

int SkiaDisplay::originalHeight() const
{
  return m_window.restoredSize().h;
}

void SkiaDisplay::setScale(int scale)
{
  m_window.setScale(scale);
}

int SkiaDisplay::scale() const
{
  return m_window.scale();
}

NonDisposableSurface* SkiaDisplay::getSurface()
{
  return static_cast<NonDisposableSurface*>(m_surface);
}

// Flips all graphics in the surface to the real display.  Returns
// false if the flip couldn't be done because the display was
// resized.
bool SkiaDisplay::flip()
{
  if (m_recreated) {
    m_recreated = false;
    return false;
  }

  m_window.updateWindow();
  return true;
}

void SkiaDisplay::maximize()
{
  m_window.maximize();
}

bool SkiaDisplay::isMaximized() const
{
  return m_window.isMaximized();
}

void SkiaDisplay::setTitleBar(const std::string& title)
{
  m_window.setText(title);
}

NativeCursor SkiaDisplay::nativeMouseCursor()
{
  return m_nativeCursor;
}

bool SkiaDisplay::setNativeMouseCursor(NativeCursor cursor)
{
  m_nativeCursor = cursor;
  m_window.setNativeMouseCursor(cursor);
  return true;
}

void SkiaDisplay::setMousePosition(const gfx::Point& position)
{
  m_window.setMousePosition(position);
}

void SkiaDisplay::captureMouse()
{
  m_window.captureMouse();
}

void SkiaDisplay::releaseMouse()
{
  m_window.releaseMouse();
}

DisplayHandle SkiaDisplay::nativeHandle()
{
  return (DisplayHandle)m_window.handle();
}

} // namespace she
