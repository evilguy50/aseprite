// Aseprite UI Library
// Copyright (C) 2001-2013, 2015  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ui/splitter.h"

#include "ui/load_layout_event.h"
#include "ui/manager.h"
#include "ui/message.h"
#include "ui/preferred_size_event.h"
#include "ui/resize_event.h"
#include "ui/save_layout_event.h"
#include "ui/system.h"
#include "ui/theme.h"

#include <sstream>

namespace ui {

using namespace gfx;

Splitter::Splitter(Type type, int align)
  : Widget(kSplitterWidget)
  , m_type(type)
  , m_pos(50)
{
  setAlign(align);
  initTheme();
}

void Splitter::setPosition(double pos)
{
  m_pos = pos;
  limitPos();

  invalidate();
}

bool Splitter::onProcessMessage(Message* msg)
{
  switch (msg->type()) {

    case kMouseDownMessage:
      if (isEnabled()) {
        Widget* c1, *c2;
        int x1, y1, x2, y2;
        int bar, click_bar;
        gfx::Point mousePos = static_cast<MouseMessage*>(msg)->position();

        bar = click_bar = 0;

        UI_FOREACH_WIDGET_WITH_END(getChildren(), it, end) {
          if (it+1 != end) {
            c1 = *it;
            c2 = *(it+1);

            ++bar;

            if (this->getAlign() & JI_HORIZONTAL) {
              x1 = c1->getBounds().x2();
              y1 = getBounds().y;
              x2 = c2->getBounds().x;
              y2 = getBounds().y2();
            }
            else {
              x1 = getBounds().x;
              y1 = c1->getBounds().y2();
              x2 = getBounds().x2();
              y2 = c2->getBounds().y;
            }

            if ((mousePos.x >= x1) && (mousePos.x < x2) &&
                (mousePos.y >= y1) && (mousePos.y < y2))
              click_bar = bar;
          }
        }

        if (!click_bar)
          break;

        captureMouse();

        // Continue with motion message...
      }
      else
        break;

    case kMouseMoveMessage:
      if (hasCapture()) {
        gfx::Point mousePos = static_cast<MouseMessage*>(msg)->position();

        if (getAlign() & JI_HORIZONTAL) {
          switch (m_type) {
            case ByPercentage:
              m_pos = 100.0 * (mousePos.x - getBounds().x) / getBounds().w;
              break;
            case ByPixel:
              m_pos = mousePos.x - getBounds().x;
              break;
          }
        }
        else {
          switch (m_type) {
            case ByPercentage:
              m_pos = 100.0 * (mousePos.y - getBounds().y) / getBounds().h;
              break;
            case ByPixel:
              m_pos = mousePos.y - getBounds().y;
              break;
          }
        }

        limitPos();
        layout();
        return true;
      }
      break;

    case kMouseUpMessage:
      if (hasCapture()) {
        releaseMouse();
        return true;
      }
      break;

    case kSetCursorMessage:
      if (isEnabled() && (!getManager()->getCapture() || hasCapture())) {
        gfx::Point mousePos = static_cast<MouseMessage*>(msg)->position();
        Widget* c1, *c2;
        int x1, y1, x2, y2;
        bool change_cursor = false;

        UI_FOREACH_WIDGET_WITH_END(getChildren(), it, end) {
          if (it+1 != end) {
            c1 = *it;
            c2 = *(it+1);

            if (this->getAlign() & JI_HORIZONTAL) {
              x1 = c1->getBounds().x2();
              y1 = getBounds().y;
              x2 = c2->getBounds().x;
              y2 = getBounds().y2();
            }
            else {
              x1 = getBounds().x;
              y1 = c1->getBounds().y2();
              x2 = getBounds().x2();
              y2 = c2->getBounds().y;
            }

            if ((mousePos.x >= x1) && (mousePos.x < x2) &&
                (mousePos.y >= y1) && (mousePos.y < y2)) {
              change_cursor = true;
              break;
            }
          }
        }

        if (change_cursor) {
          if (getAlign() & JI_HORIZONTAL)
            set_mouse_cursor(kSizeWECursor);
          else
            set_mouse_cursor(kSizeNSCursor);
          return true;
        }
      }
      break;

  }

  return Widget::onProcessMessage(msg);
}

void Splitter::onResize(ResizeEvent& ev)
{
#define LAYOUT_TWO_CHILDREN(x, y, w, h, l, t, r, b)                     \
  {                                                                     \
    avail = rc.w - this->child_spacing;                                 \
                                                                        \
    pos.x = rc.x;                                                       \
    pos.y = rc.y;                                                       \
    switch (m_type) {                                                   \
      case ByPercentage:                                                \
        pos.w = int(avail*m_pos/100);                                   \
        break;                                                          \
      case ByPixel:                                                     \
        pos.w = int(m_pos);                                             \
        break;                                                          \
    }                                                                   \
                                                                        \
    /* TODO uncomment this to make a restricted splitter */             \
    /* pos.w = MID(reqSize1.w, pos.w, avail-reqSize2.w); */             \
    pos.h = rc.h;                                                       \
                                                                        \
    child1->setBounds(pos);                                             \
    gfx::Rect child1Pos = child1->getBounds();                          \
                                                                        \
    pos.x = child1Pos.x + child1Pos.w + this->child_spacing;            \
    pos.y = rc.y;                                                       \
    pos.w = avail - child1Pos.w;                                        \
    pos.h = rc.h;                                                       \
                                                                        \
    child2->setBounds(pos);                                             \
  }

  gfx::Rect rc(ev.getBounds());
  gfx::Rect pos(0, 0, 0, 0);
  int avail;

  setBoundsQuietly(rc);
  limitPos();

  Widget* child1 = panel1();
  Widget* child2 = panel2();

  if (child1 && child2) {
    if (getAlign() & JI_HORIZONTAL) {
      LAYOUT_TWO_CHILDREN(x, y, w, h, l, t, r, b);
    }
    else {
      LAYOUT_TWO_CHILDREN(y, x, h, w, t, l, b, r);
    }
  }
  else if (child1)
    child1->setBounds(rc);
  else if (child2)
    child2->setBounds(rc);
}

void Splitter::onPaint(PaintEvent& ev)
{
  getTheme()->paintSplitter(ev);
}

void Splitter::onPreferredSize(PreferredSizeEvent& ev)
{
#define GET_CHILD_SIZE(w, h)                    \
  do {                                          \
    w = MAX(w, reqSize.w);                      \
    h = MAX(h, reqSize.h);                      \
  } while(0)

#define FINAL_SIZE(w)                                     \
  do {                                                    \
    w *= visibleChildren;                                 \
    w += this->child_spacing * (visibleChildren-1);       \
  } while(0)

  int visibleChildren;
  Size reqSize;

  visibleChildren = 0;
  for (Widget* child : getChildren()) {
    if (child->isVisible())
      visibleChildren++;
  }

  int w, h;
  w = h = 0;

  for (Widget* child : getChildren()) {
    if (!child->isVisible())
      continue;

    reqSize = child->getPreferredSize();

    if (this->getAlign() & JI_HORIZONTAL)
      GET_CHILD_SIZE(w, h);
    else
      GET_CHILD_SIZE(h, w);
  }

  if (visibleChildren > 0) {
    if (this->getAlign() & JI_HORIZONTAL)
      FINAL_SIZE(w);
    else
      FINAL_SIZE(h);
  }

  w += this->border_width.l + this->border_width.r;
  h += this->border_width.t + this->border_width.b;

  ev.setPreferredSize(Size(w, h));
}

void Splitter::onLoadLayout(LoadLayoutEvent& ev)
{
  ev.stream() >> m_pos;
  if (m_pos < 0) m_pos = 0;
  if (m_type == ByPixel)
    m_pos *= guiscale();

  // Do for all children
  for (Widget* child : getChildren())
    child->loadLayout();
}

void Splitter::onSaveLayout(SaveLayoutEvent& ev)
{
  double pos = (m_type == ByPixel ? m_pos / guiscale(): m_pos);
  ev.stream() << pos;

  // Do for all children
  for (Widget* child : getChildren())
    child->saveLayout();
}

Widget* Splitter::panel1() const
{
  const WidgetsList& list = getChildren();
  if (list.size() >= 1 && list[0]->isVisible())
    return list[0];
  else
    return nullptr;
}

Widget* Splitter::panel2() const
{
  const WidgetsList& list = getChildren();
  if (list.size() >= 2 && list[1]->isVisible())
    return list[1];
  else
    return nullptr;
}

void Splitter::limitPos()
{
  if (getAlign() & JI_HORIZONTAL) {
    switch (m_type) {
      case ByPercentage:
        m_pos = MID(0, m_pos, 100);
        break;
      case ByPixel:
        m_pos = MID(0, m_pos, getBounds().w);
        break;
    }
  }
  else {
    switch (m_type) {
      case ByPercentage:
        m_pos = MID(0, m_pos, 100);
        break;
      case ByPixel:
        m_pos = MID(0, m_pos, getBounds().h);
        break;
    }
  }
}

} // namespace ui
