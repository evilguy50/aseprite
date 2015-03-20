// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/color.h"
#include "app/color_utils.h"
#include "app/modules/gui.h"
#include "app/modules/palettes.h"
#include "app/ui/palette_view.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/skin/style.h"
#include "app/ui/status_bar.h"
#include "doc/blend.h"
#include "doc/image.h"
#include "doc/palette.h"
#include "gfx/color.h"
#include "gfx/point.h"
#include "ui/graphics.h"
#include "ui/manager.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/preferred_size_event.h"
#include "ui/system.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "ui/widget.h"

#include <cstdlib>
#include <cstring>

namespace app {

using namespace ui;
using namespace app::skin;

WidgetType palette_view_type()
{
  static WidgetType type = kGenericWidget;
  if (type == kGenericWidget)
    type = register_widget_type();
  return type;
}

PaletteView::PaletteView(bool editable)
  : Widget(palette_view_type())
  , m_editable(editable)
  , m_columns(16)
  , m_boxsize(7*guiscale())
  , m_currentEntry(-1)
  , m_rangeAnchor(-1)
  , m_selectedEntries(Palette::MaxColors, false)
  , m_isUpdatingColumns(false)
  , m_hot(Hit::NONE)
{
  setFocusStop(true);
  setDoubleBuffered(true);

  this->border_width.l = this->border_width.r = 1 * guiscale();
  this->border_width.t = this->border_width.b = 1 * guiscale();
  this->child_spacing = 1 * guiscale();

  m_conn = App::instance()->PaletteChange.connect(&PaletteView::onAppPaletteChange, this);
}

void PaletteView::setColumns(int columns)
{
  int old_columns = m_columns;

  ASSERT(columns >= 1 && columns <= Palette::MaxColors);
  m_columns = columns;

  if (m_columns != old_columns) {
    View* view = View::getView(this);
    if (view)
      view->updateView();

    invalidate();
  }
}

void PaletteView::clearSelection()
{
  std::fill(m_selectedEntries.begin(),
            m_selectedEntries.end(), false);
}

void PaletteView::selectColor(int index)
{
  ASSERT(index >= 0 && index < Palette::MaxColors);

  if (m_currentEntry != index || !m_selectedEntries[index]) {
    m_currentEntry = index;
    m_rangeAnchor = index;
    m_selectedEntries[index] = true;

    update_scroll(m_currentEntry);
    invalidate();
  }
}

void PaletteView::selectRange(int index1, int index2)
{
  m_rangeAnchor = index1;
  m_currentEntry = index2;

  std::fill(m_selectedEntries.begin()+std::min(index1, index2),
            m_selectedEntries.begin()+std::max(index1, index2)+1, true);

  update_scroll(index2);
  invalidate();
}

int PaletteView::getSelectedEntry() const
{
  return m_currentEntry;
}

bool PaletteView::getSelectedRange(int& index1, int& index2) const
{
  int i, i2, j;

  // Find the first selected entry
  for (i=0; i<(int)m_selectedEntries.size(); ++i)
    if (m_selectedEntries[i])
      break;

  // Find the first unselected entry after i
  for (i2=i+1; i2<(int)m_selectedEntries.size(); ++i2)
    if (!m_selectedEntries[i2])
      break;

  // Find the last selected entry
  for (j=m_selectedEntries.size()-1; j>=0; --j)
    if (m_selectedEntries[j])
      break;

  if (j-i+1 == i2-i) {
    index1 = i;
    index2 = j;
    return true;
  }
  else
    return false;
}

void PaletteView::getSelectedEntries(SelectedEntries& entries) const
{
  entries = m_selectedEntries;
}

app::Color PaletteView::getColorByPosition(const gfx::Point& pos)
{
  gfx::Point relPos = pos - getBounds().getOrigin();
  Palette* palette = get_current_palette();
  for (int i=0; i<palette->size(); ++i) {
    if (getPaletteEntryBounds(i).contains(relPos))
      return app::Color::fromIndex(i);
  }
  return app::Color::fromMask();
}

bool PaletteView::onProcessMessage(Message* msg)
{
  switch (msg->type()) {

    case kMouseDownMessage:
      captureMouse();
      // Continue...

    case kMouseMoveMessage: {
      MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);

      if (m_hot.part == Hit::COLOR) {
        int idx = m_hot.color;

        StatusBar::instance()->showColor(0, "",
          app::Color::fromIndex(idx), 255);

        if (hasCapture() && idx != m_currentEntry) {
          clearSelection();

          if (msg->type() == kMouseMoveMessage)
            selectRange(m_rangeAnchor, idx);
          else
            selectColor(idx);

          // Emit signal
          PaletteIndexChangeEvent ev(this, idx, mouseMsg->buttons());
          IndexChange(ev);
        }
      }

      if (hasCapture())
        return true;

      break;
    }

    case kMouseUpMessage:
      if (hasCapture())
        releaseMouse();
      return true;

    case kMouseWheelMessage: {
      View* view = View::getView(this);
      if (view) {
        gfx::Point scroll = view->getViewScroll();
        scroll += static_cast<MouseMessage*>(msg)->wheelDelta() * 3 * m_boxsize;
        view->setViewScroll(scroll);
      }
      break;
    }

    case kMouseLeaveMessage:
      StatusBar::instance()->clearText();
      m_hot = Hit(Hit::NONE);
      invalidate();
      break;

    case kSetCursorMessage: {
      MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
      Hit hit = hitTest(mouseMsg->position() - getBounds().getOrigin());
      if (hit != m_hot) {
        m_hot = hit;
        invalidate();
      }
      if (m_hot.part == Hit::OUTLINE)
        ui::set_mouse_cursor(kMoveCursor);
      else
        ui::set_mouse_cursor(kArrowCursor);
      return true;
    }

  }

  return Widget::onProcessMessage(msg);
}

void PaletteView::onPaint(ui::PaintEvent& ev)
{
  SkinTheme* theme = static_cast<SkinTheme*>(getTheme());
  int outlineWidth = theme->dimensions.paletteOutlineWidth();
  ui::Graphics* g = ev.getGraphics();
  gfx::Rect bounds = getClientBounds();
  Palette* palette = get_current_palette();
  gfx::Color bordercolor = gfx::rgba(255, 255, 255);

  g->fillRect(gfx::rgba(0, 0, 0), bounds);

  // Draw palette entries
  for (int i=0; i<palette->size(); ++i) {
    gfx::Rect box = getPaletteEntryBounds(i);
    gfx::Color color = gfx::rgba(
      rgba_getr(palette->getEntry(i)),
      rgba_getg(palette->getEntry(i)),
      rgba_getb(palette->getEntry(i)));

    g->fillRect(color, box);

    if (m_currentEntry == i)
      g->fillRect(color_utils::blackandwhite_neg(color),
        gfx::Rect(box.getCenter(), gfx::Size(1, 1)));
  }

  // Draw selected entries
  Style::State state = Style::active();
  if (m_hot.part == Hit::OUTLINE) state += Style::hover();

  for (int i=0; i<palette->size(); ++i) {
    if (!m_selectedEntries[i])
      continue;

    const int max = Palette::MaxColors;
    bool top    = (i >= m_columns            && i-m_columns >= 0  ? m_selectedEntries[i-m_columns]: false);
    bool bottom = (i < max-m_columns         && i+m_columns < max ? m_selectedEntries[i+m_columns]: false);
    bool left   = ((i%m_columns)>0           && i-1         >= 0  ? m_selectedEntries[i-1]: false);
    bool right  = ((i%m_columns)<m_columns-1 && i+1         < max ? m_selectedEntries[i+1]: false);

    gfx::Rect box = getPaletteEntryBounds(i);
    gfx::Rect clipR = box;
    box.enlarge(outlineWidth);

    if (!left) {
      clipR.x -= outlineWidth;
      clipR.w += outlineWidth;
    }

    if (!top) {
      clipR.y -= outlineWidth;
      clipR.h += outlineWidth;
    }

    if (!right)
      clipR.w += outlineWidth;
    else {
      clipR.w += guiscale();
      box.w += outlineWidth;
    }

    if (!bottom)
      clipR.h += outlineWidth;
    else {
      clipR.h += guiscale();
      box.h += outlineWidth;
    }

    IntersectClip clip(g, clipR);
    if (clip) {
      theme->styles.timelineRangeOutline()->paint(g, box,
        NULL, state);
    }
  }
}

void PaletteView::onResize(ui::ResizeEvent& ev)
{
  if (!m_isUpdatingColumns) {
    m_isUpdatingColumns = true;
    View* view = View::getView(this);
    if (view) {
      int columns =
        (view->getViewportBounds().w-this->child_spacing*2)
        / (m_boxsize+this->child_spacing);
      setColumns(MID(1, columns, Palette::MaxColors));
    }
    m_isUpdatingColumns = false;
  }

  Widget::onResize(ev);
}

void PaletteView::onPreferredSize(ui::PreferredSizeEvent& ev)
{
  gfx::Size sz;
  request_size(&sz.w, &sz.h);
  ev.setPreferredSize(sz);
}

void PaletteView::request_size(int* w, int* h)
{
  div_t d = div(Palette::MaxColors, m_columns);
  int cols = m_columns;
  int rows = d.quot + ((d.rem)? 1: 0);

  *w = this->border_width.l + this->border_width.r +
    + cols*m_boxsize + (cols-1)*this->child_spacing;

  *h = this->border_width.t + this->border_width.b +
    + rows*m_boxsize + (rows-1)*this->child_spacing;
}

void PaletteView::update_scroll(int color)
{
  View* view = View::getView(this);
  if (!view)
    return;

  gfx::Rect vp = view->getViewportBounds();
  gfx::Point scroll;
  int x, y, cols;
  div_t d;

  scroll = view->getViewScroll();

  d = div(Palette::MaxColors, m_columns);
  cols = m_columns;

  y = (m_boxsize+this->child_spacing) * (color / cols);
  x = (m_boxsize+this->child_spacing) * (color % cols);

  if (scroll.x > x)
    scroll.x = x;
  else if (scroll.x+vp.w-m_boxsize-2 < x)
    scroll.x = x-vp.w+m_boxsize+2;

  if (scroll.y > y)
    scroll.y = y;
  else if (scroll.y+vp.h-m_boxsize-2 < y)
    scroll.y = y-vp.h+m_boxsize+2;

  view->setViewScroll(scroll);
}

void PaletteView::onAppPaletteChange()
{
  invalidate();
}

gfx::Rect PaletteView::getPaletteEntryBounds(int index)
{
  gfx::Rect bounds = getClientBounds();
  div_t d = div(Palette::MaxColors, m_columns);
  int cols = m_columns;
  int rows = d.quot + (d.rem ? 1: 0);
  int col = index % cols;
  int row = index / cols;

  return gfx::Rect(
    bounds.x + this->border_width.l + col*(m_boxsize+this->child_spacing),
    bounds.y + this->border_width.t + row*(m_boxsize+this->child_spacing),
    m_boxsize, m_boxsize);
}

PaletteView::Hit PaletteView::hitTest(const gfx::Point& pos)
{
  SkinTheme* theme = static_cast<SkinTheme*>(getTheme());
  int outlineWidth = theme->dimensions.paletteOutlineWidth();
  Palette* palette = get_current_palette();

  if (!hasCapture()) {
    // First check if the mouse is inside the selection outline.
    for (int i=0; i<palette->size(); ++i) {
      if (!m_selectedEntries[i])
        continue;

      const int max = Palette::MaxColors;
      bool top    = (i >= m_columns            && i-m_columns >= 0  ? m_selectedEntries[i-m_columns]: false);
      bool bottom = (i < max-m_columns         && i+m_columns < max ? m_selectedEntries[i+m_columns]: false);
      bool left   = ((i%m_columns)>0           && i-1         >= 0  ? m_selectedEntries[i-1]: false);
      bool right  = ((i%m_columns)<m_columns-1 && i+1         < max ? m_selectedEntries[i+1]: false);

      gfx::Rect box = getPaletteEntryBounds(i);
      box.enlarge(outlineWidth);

      if ((!top    && gfx::Rect(box.x, box.y, box.w, outlineWidth).contains(pos)) ||
        (!bottom && gfx::Rect(box.x, box.y+box.h-outlineWidth, box.w, outlineWidth).contains(pos)) ||
        (!left   && gfx::Rect(box.x, box.y, outlineWidth, box.h).contains(pos)) ||
        (!right  && gfx::Rect(box.x+box.w-outlineWidth, box.y, outlineWidth, box.h).contains(pos)))
        return Hit(Hit::OUTLINE, i);
    }
  }

  // Check if we are inside a color.
  for (int i=0; i<palette->size(); ++i) {
    gfx::Rect box = getPaletteEntryBounds(i);
    box.w += child_spacing;
    box.h += child_spacing;
    if (box.contains(pos))
      return Hit(Hit::COLOR, i);
  }

  return Hit(Hit::NONE);
}

} // namespace app
