// Aseprite UI Library
// Copyright (C) 2001-2013, 2015  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

/* Based on code from GTK+ 2.1.2 (gtk+/gtk/gtkhbox.c) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gfx/size.h"
#include "ui/box.h"
#include "ui/message.h"
#include "ui/preferred_size_event.h"
#include "ui/resize_event.h"
#include "ui/theme.h"

namespace ui {

using namespace gfx;

Box::Box(int align)
  : Widget(kBoxWidget)
{
  setAlign(align);
  initTheme();
}

void Box::onPreferredSize(PreferredSizeEvent& ev)
{
#define GET_CHILD_SIZE(w, h)                    \
  {                                             \
    if (getAlign() & HOMOGENEOUS)               \
      w = MAX(w, reqSize.w);                    \
    else                                        \
      w += reqSize.w;                           \
                                                \
    h = MAX(h, reqSize.h);                      \
  }

#define FINAL_SIZE(w)                                   \
  {                                                     \
    if (getAlign() & HOMOGENEOUS)                       \
      w *= nvis_children;                               \
                                                        \
    w += child_spacing * (nvis_children-1);             \
  }

  int w, h, nvis_children;

  nvis_children = 0;
  UI_FOREACH_WIDGET(getChildren(), it) {
    Widget* child = *it;
    if (!(child->flags & HIDDEN))
      nvis_children++;
  }

  w = h = 0;

  UI_FOREACH_WIDGET(getChildren(), it) {
    Widget* child = *it;

    if (child->flags & HIDDEN)
      continue;

    Size reqSize = child->getPreferredSize();

    if (this->getAlign() & HORIZONTAL) {
      GET_CHILD_SIZE(w, h);
    }
    else {
      GET_CHILD_SIZE(h, w);
    }
  }

  if (nvis_children > 0) {
    if (this->getAlign() & HORIZONTAL) {
      FINAL_SIZE(w);
    }
    else {
      FINAL_SIZE(h);
    }
  }

  w += border_width.l + border_width.r;
  h += border_width.t + border_width.b;

  ev.setPreferredSize(Size(w, h));
}

void Box::onResize(ResizeEvent& ev)
{
#define FIXUP(x, y, w, h, l, t, r, b)                                   \
  {                                                                     \
    if (nvis_children > 0) {                                            \
      if (getAlign() & HOMOGENEOUS) {                                   \
        width = (getBounds().w                                          \
                 - this->border_width.l                                 \
                 - this->border_width.r                                 \
                 - this->child_spacing * (nvis_children - 1));          \
        extra = width / nvis_children;                                  \
      }                                                                 \
      else if (nexpand_children > 0) {                                  \
        width = getBounds().w - reqSize.w;                              \
        extra = width / nexpand_children;                               \
      }                                                                 \
      else {                                                            \
        width = 0;                                                      \
        extra = 0;                                                      \
      }                                                                 \
                                                                        \
      x = getBounds().x + this->border_width.l;                         \
      y = getBounds().y + this->border_width.t;                         \
      h = MAX(1, getBounds().h                                          \
                 - this->border_width.t                                 \
                 - this->border_width.b);                               \
                                                                        \
      UI_FOREACH_WIDGET(getChildren(), it) {                            \
        child = *it;                                                    \
                                                                        \
        if (!(child->flags & HIDDEN)) {                                 \
          if (this->getAlign() & HOMOGENEOUS) {                         \
            if (nvis_children == 1)                                     \
              child_width = width;                                      \
            else                                                        \
              child_width = extra;                                      \
                                                                        \
            --nvis_children;                                            \
            width -= extra;                                             \
          }                                                             \
          else {                                                        \
            reqSize = child->getPreferredSize();                        \
                                                                        \
            child_width = reqSize.w;                                    \
                                                                        \
            if (child->isExpansive()) {                                 \
              if (nexpand_children == 1)                                \
                child_width += width;                                   \
              else                                                      \
                child_width += extra;                                   \
                                                                        \
              --nexpand_children;                                       \
              width -= extra;                                           \
            }                                                           \
          }                                                             \
                                                                        \
          w = MAX(1, child_width);                                      \
                                                                        \
          gfx::Rect cpos;                                               \
          if (getAlign() & HORIZONTAL)                                  \
            cpos = gfx::Rect(x, y, w, h);                               \
          else                                                          \
            cpos = gfx::Rect(y, x, h, w);                               \
                                                                        \
          child->setBounds(cpos);                                       \
                                                                        \
          x += child_width + this->child_spacing;                       \
        }                                                               \
      }                                                                 \
    }                                                                   \
  }

  Widget* child;
  int nvis_children = 0;
  int nexpand_children = 0;
  int child_width;
  int width;
  int extra;
  int x, y, w, h;

  setBoundsQuietly(ev.getBounds());

  UI_FOREACH_WIDGET(getChildren(), it) {
    child = *it;

    if (!(child->flags & HIDDEN)) {
      nvis_children++;
      if (child->isExpansive())
        nexpand_children++;
    }
  }

  Size reqSize = getPreferredSize();

  if (this->getAlign() & HORIZONTAL) {
    FIXUP(x, y, w, h, l, t, r, b);
  }
  else {
    FIXUP(y, x, h, w, t, l, b, r);
  }
}

void Box::onPaint(PaintEvent& ev)
{
  getTheme()->paintBox(ev);
}

} // namespace ui
