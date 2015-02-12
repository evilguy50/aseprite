// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/document_location.h"

#include "doc/cel.h"
#include "doc/layer.h"
#include "doc/sprite.h"

namespace app {

using namespace doc;

LayerIndex DocumentLocation::layerIndex() const
{
  return (m_sprite && m_layer ?
          m_sprite->layerToIndex(m_layer): LayerIndex());
}

void DocumentLocation::layerIndex(LayerIndex layerIndex)
{
  ASSERT(m_sprite != NULL);
  m_layer = m_sprite->indexToLayer(layerIndex);
}

Palette* DocumentLocation::palette()
{
  return (m_sprite ? m_sprite->palette(m_frame): NULL);
}

const Cel* DocumentLocation::cel() const
{
  if (m_layer)
    return m_layer->cel(m_frame);
  else
    return NULL;
}

Cel* DocumentLocation::cel()
{
  if (m_layer)
    return m_layer->cel(m_frame);
  else
    return NULL;
}

Image* DocumentLocation::image(int* x, int* y, int* opacity) const
{
  Image* image = NULL;

  if (m_sprite) {
    if (const Cel* cel = this->cel()) {
      image = cel->image();
      if (x) *x = cel->x();
      if (y) *y = cel->y();
      if (opacity) *opacity = MID(0, cel->opacity(), 255);
    }
  }

  return image;
}

Palette* DocumentLocation::palette() const
{
  return (m_sprite ? m_sprite->palette(m_frame): NULL);
}

} // namespace app
