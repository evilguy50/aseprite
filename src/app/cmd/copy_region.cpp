// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/cmd/copy_region.h"

#include "doc/image.h"

#include <algorithm>

namespace app {
namespace cmd {

CopyRegion::CopyRegion(Image* dst, Image* src,
  const gfx::Region& region, int dst_dx, int dst_dy)
  : WithImage(dst)
{
  // Save region pixels
  for (const auto& rc : region) {
    gfx::Clip clip(
      rc.x+dst_dx, rc.y+dst_dy,
      rc.x, rc.y, rc.w, rc.h);
    if (!clip.clip(
          dst->width(), dst->height(),
          src->width(), src->height()))
      continue;

    m_region.createUnion(m_region, gfx::Region(clip.dstBounds()));

    for (int y=0; y<clip.size.h; ++y)  {
      m_stream.write(
        (const char*)src->getPixelAddress(clip.src.x, clip.src.y+y),
        src->getRowStrideSize(clip.size.w));
    }
  }
}

void CopyRegion::onExecute()
{
  swap();
}

void CopyRegion::onUndo()
{
  swap();
}

void CopyRegion::onRedo()
{
  swap();
}

void CopyRegion::swap()
{
  Image* image = this->image();

  // Save current image region in "tmp" stream
  std::stringstream tmp;
  for (const auto& rc : m_region)
    for (int y=0; y<rc.h; ++y)
      tmp.write(
        (const char*)image->getPixelAddress(rc.x, rc.y+y),
        image->getRowStrideSize(rc.w));

  // Restore m_stream into the image
  for (const auto& rc : m_region) {
    for (int y=0; y<rc.h; ++y) {
      m_stream.read(
        (char*)image->getPixelAddress(rc.x, rc.y+y),
        image->getRowStrideSize(rc.w));
    }
  }

  // TODO use m_stream.swap(tmp) when clang and gcc support it
  m_stream.str(tmp.str());
  m_stream.clear();

  image->incrementVersion();
}

} // namespace cmd
} // namespace app
