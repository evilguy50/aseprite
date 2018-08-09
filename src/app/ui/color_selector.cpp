// Aseprite
// Copyright (C) 2016-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#define COLSEL_TRACE(...)

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/color_selector.h"

#include "app/color_utils.h"
#include "app/modules/gfx.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/status_bar.h"
#include "base/concurrent_queue.h"
#include "base/scoped_value.h"
#include "base/thread.h"
#include "os/surface.h"
#include "os/system.h"
#include "ui/manager.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/register_message.h"
#include "ui/scale.h"
#include "ui/size_hint_event.h"
#include "ui/system.h"

#include <cmath>
#include <condition_variable>
#include <thread>

namespace app {

using namespace app::skin;
using namespace ui;

// TODO This logic could be used to redraw any widget:
// 1. We send a onPaintSurfaceInBgThread() to paint the widget on a
//    offscreen buffer
// 2. When the painting is done, we flip the buffer onto the screen
// 3. If we receive another onPaint() we can cancel the background
//    painting and start another onPaintSurfaceInBgThread()
//
// An alternative (better) way:
// 1. Create an alternative ui::Graphics implementation that generates
//    a list commands for the render thread
// 2. Widgets can still use the same onPaint()
// 3. The background threads consume the list of commands and render
//    the screen.
//
// The bad side is that is harder to invalidate the commands that will
// render an old state of the widget. So the render thread should
// start caring about invalidating old commands (outdated regions) or
// cleaning the queue if it gets too big.
//
class ColorSelector::Painter {
public:
  Painter() : m_canvas(nullptr) {
  }

  ~Painter() {
    ASSERT(!m_canvas);
  }

  void addRef() {
    assert_ui_thread();

    if (m_ref == 0)
      m_paintingThread = std::thread([this]{ paintingProc(); });

    ++m_ref;
  }

  void releaseRef() {
    assert_ui_thread();

    --m_ref;
    if (m_ref == 0) {
      {
        std::unique_lock<std::mutex> lock(m_mutex);
        stopCurrentPainting(lock);

        m_killing = true;
        m_paintingCV.notify_one();
      }

      m_paintingThread.join();
      if (m_canvas) {
        m_canvas->dispose();
        m_canvas = nullptr;
      }
    }
  }

  os::Surface* getCanvas(int w, int h, gfx::Color bgColor) {
    assert_ui_thread();

    if (!m_canvas ||
        m_canvas->width() != w ||
        m_canvas->height() != h) {
      std::unique_lock<std::mutex> lock(m_mutex);
      stopCurrentPainting(lock);

      auto oldCanvas = m_canvas;
      m_canvas = os::instance()->createSurface(w, h);
      m_canvas->fillRect(bgColor, gfx::Rect(0, 0, w, h));
      if (oldCanvas) {
        m_canvas->drawSurface(oldCanvas, 0, 0);
        oldCanvas->dispose();
      }
    }
    return m_canvas;
  }

  void startBgPainting(ColorSelector* colorSelector,
                       const gfx::Rect& mainBounds,
                       const gfx::Rect& bottomBarBounds,
                       const gfx::Rect& alphaBarBounds) {
    assert_ui_thread();
    COLSEL_TRACE("COLSEL: startBgPainting for %p\n", colorSelector);

    std::unique_lock<std::mutex> lock(m_mutex);
    stopCurrentPainting(lock);

    m_colorSelector = colorSelector;
    m_manager = colorSelector->manager();
    m_mainBounds = mainBounds;
    m_bottomBarBounds = bottomBarBounds;
    m_alphaBarBounds = alphaBarBounds;

    m_stopPainting = false;
    m_paintingCV.notify_one();
  }

private:

  void stopCurrentPainting(std::unique_lock<std::mutex>& lock) {
    // Stop current
    if (m_colorSelector) {
      COLSEL_TRACE("COLSEL: stoppping painting of %p\n", m_colorSelector);

      // TODO use another condition variable here
      m_stopPainting = true;
      m_waitStopCV.wait(lock);
    }

    ASSERT(m_colorSelector == nullptr);
  }

  void paintingProc() {
    COLSEL_TRACE("COLSEL: paintingProc starts\n");

    std::unique_lock<std::mutex> lock(m_mutex);
    while (true) {
      m_paintingCV.wait(lock);

      if (m_killing)
        break;

      auto colorSel = m_colorSelector;
      if (!colorSel)
        break;

      COLSEL_TRACE("COLSEL: starting painting in bg for %p\n", colorSel);

      // Do the intesive painting in the background thread
      {
        lock.unlock();
        colorSel->onPaintSurfaceInBgThread(
          m_canvas,
          m_mainBounds,
          m_bottomBarBounds,
          m_alphaBarBounds,
          m_stopPainting);
        lock.lock();
      }

      m_colorSelector = nullptr;

      if (m_stopPainting) {
        COLSEL_TRACE("COLSEL: painting for %p stopped\n");
        m_waitStopCV.notify_one();
      }
      else {
        COLSEL_TRACE("COLSEL: painting for %p done and sending message\n");
        colorSel->m_paintFlags |= DoneFlag;
      }
    }

    COLSEL_TRACE("COLSEL: paintingProc ends\n");
  }

  int m_ref = 0;
  bool m_killing = false;
  bool m_stopPainting = false;
  std::mutex m_mutex;
  std::condition_variable m_paintingCV;
  std::condition_variable m_waitStopCV;
  os::Surface* m_canvas;
  ColorSelector* m_colorSelector;
  ui::Manager* m_manager;
  gfx::Rect m_mainBounds;
  gfx::Rect m_bottomBarBounds;
  gfx::Rect m_alphaBarBounds;
  std::thread m_paintingThread;
};

static ColorSelector::Painter painter;

ColorSelector::ColorSelector()
  : Widget(kGenericWidget)
  , m_paintFlags(AllAreasFlag)
  , m_lockColor(false)
  , m_capturedInBottom(false)
  , m_capturedInAlpha(false)
  , m_timer(100, this)
{
  initTheme();
  painter.addRef();
}

ColorSelector::~ColorSelector()
{
  painter.releaseRef();
}

void ColorSelector::selectColor(const app::Color& color)
{
  if (m_lockColor)
    return;

  if (m_color != color)
    m_paintFlags |= onNeedsSurfaceRepaint(color);

  m_color = color;
  invalidate();
}

app::Color ColorSelector::getColorByPosition(const gfx::Point& pos)
{
  gfx::Rect rc = childrenBounds();
  if (rc.isEmpty())
    return app::Color::fromMask();

  const int u = pos.x - rc.x;
  const int umax = MAX(1, rc.w-1);

  const gfx::Rect bottomBarBounds = this->bottomBarBounds();
  if (( hasCapture() && m_capturedInBottom) ||
      (!hasCapture() && bottomBarBounds.contains(pos)))
    return getBottomBarColor(u, umax);

  const gfx::Rect alphaBarBounds = this->alphaBarBounds();
  if (( hasCapture() && m_capturedInAlpha) ||
      (!hasCapture() && alphaBarBounds.contains(pos)))
    return getAlphaBarColor(u, umax);

  const int v = pos.y - rc.y;
  const int vmax = MAX(1, rc.h-bottomBarBounds.h-alphaBarBounds.h-1);
  return getMainAreaColor(u, umax,
                          v, vmax);
}

app::Color ColorSelector::getAlphaBarColor(const int u, const int umax)
{
  int alpha = (255 * u / umax);
  app::Color color = m_color;
  color.setAlpha(MID(0, alpha, 255));
  return color;
}

void ColorSelector::onSizeHint(SizeHintEvent& ev)
{
  ev.setSizeHint(gfx::Size(32*ui::guiscale(), 32*ui::guiscale()));
}

bool ColorSelector::onProcessMessage(ui::Message* msg)
{
  switch (msg->type()) {

    case kMouseDownMessage:
      if (manager()->getCapture())
        break;

      captureMouse();

      // Continue...

    case kMouseMoveMessage: {
      MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
      const gfx::Point pos = mouseMsg->position();

      if (msg->type() == kMouseDownMessage) {
        m_capturedInBottom = bottomBarBounds().contains(pos);
        m_capturedInAlpha = alphaBarBounds().contains(pos);
      }

      app::Color color = getColorByPosition(pos);
      if (color != app::Color::fromMask()) {
        base::ScopedValue<bool> switcher(m_lockColor, subColorPicked(), false);

        StatusBar::instance()->showColor(0, "", color);
        if (hasCapture())
          ColorChange(color, mouseMsg->buttons());
      }
      break;
    }

    case kMouseUpMessage:
      if (hasCapture()) {
        m_capturedInBottom = false;
        m_capturedInAlpha = false;
        releaseMouse();
      }
      return true;

    case kSetCursorMessage: {
      MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
      app::Color color = getColorByPosition(mouseMsg->position());
      if (color.getType() != app::Color::MaskType) {
        ui::set_mouse_cursor(kCustomCursor, SkinTheme::instance()->cursors.eyedropper());
        return true;
      }
      break;
    }

    case kMouseWheelMessage:
      if (!hasCapture()) {
        double scale = 1.0;
        if (msg->shiftPressed() ||
            msg->ctrlPressed() ||
            msg->altPressed()) {
          scale = 15.0;
        }

        double newHue = m_color.getHsvHue()
          + scale*(+ static_cast<MouseMessage*>(msg)->wheelDelta().x
                   - static_cast<MouseMessage*>(msg)->wheelDelta().y);

        while (newHue < 0.0)
          newHue += 360.0;
        newHue = std::fmod(newHue, 360.0);

        if (newHue != m_color.getHsvHue()) {
          app::Color newColor =
            app::Color::fromHsv(
              newHue,
              m_color.getHsvSaturation(),
              m_color.getHsvValue(),
              m_color.getAlpha());

          ColorChange(newColor, kButtonNone);
        }
      }
      break;

    case kTimerMessage:
      if (m_paintFlags & DoneFlag) {
        m_timer.stop();
        invalidate();
        return true;
      }
      break;

  }

  return Widget::onProcessMessage(msg);
}

void ColorSelector::onInitTheme(ui::InitThemeEvent& ev)
{
  Widget::onInitTheme(ev);
  setBorder(gfx::Border(3*ui::guiscale()));
}

void ColorSelector::onResize(ui::ResizeEvent& ev)
{
  Widget::onResize(ev);

  // We'll need to redraw the whole surface again with the new widget
  // size.
  m_paintFlags = AllAreasFlag;
}

void ColorSelector::onPaint(ui::PaintEvent& ev)
{
  ui::Graphics* g = ev.graphics();
  SkinTheme* theme = static_cast<SkinTheme*>(this->theme());

  theme->drawRect(g, clientBounds(),
                  theme->parts.editorNormal().get(),
                  false);       // Do not fill the center

  gfx::Rect rc = clientChildrenBounds();
  if (rc.isEmpty())
    return;

  g->drawSurface(
    painter.getCanvas(rc.w, rc.h, theme->colors.workspace()),
    rc.x, rc.y);

  gfx::Rect bottomBarBounds = this->bottomBarBounds();
  gfx::Rect alphaBarBounds = this->alphaBarBounds();
  rc.h -= bottomBarBounds.h + alphaBarBounds.h;
  onPaintMainArea(g, rc);

  if (!bottomBarBounds.isEmpty()) {
    bottomBarBounds.offset(-bounds().origin());
    onPaintBottomBar(g, bottomBarBounds);
  }

  if (!alphaBarBounds.isEmpty()) {
    alphaBarBounds.offset(-bounds().origin());
    onPaintAlphaBar(g, alphaBarBounds);
  }

  if ((m_paintFlags & AllAreasFlag) != 0) {
    m_paintFlags &= ~DoneFlag;
    m_timer.start();

    gfx::Point d = -rc.origin();
    rc.offset(d);
    if (!bottomBarBounds.isEmpty()) bottomBarBounds.offset(d);
    if (!alphaBarBounds.isEmpty()) alphaBarBounds.offset(d);
    painter.startBgPainting(this, rc, bottomBarBounds, alphaBarBounds);
  }
}

void ColorSelector::onPaintAlphaBar(ui::Graphics* g, const gfx::Rect& rc)
{
  const double lit = m_color.getHslLightness();
  const int alpha = m_color.getAlpha();
  const gfx::Point pos(rc.x + int(rc.w * alpha / 255),
                       rc.y + rc.h/2);
  paintColorIndicator(g, pos, lit < 0.5);
}

void ColorSelector::onPaintSurfaceInBgThread(
  os::Surface* s,
  const gfx::Rect& main,
  const gfx::Rect& bottom,
  const gfx::Rect& alpha,
  bool& stop)
{
  if ((m_paintFlags & AlphaBarFlag) && !alpha.isEmpty()) {
    draw_alpha_slider(s, alpha, m_color);
    if (stop)
      return;
    m_paintFlags ^= AlphaBarFlag;
  }
}

int ColorSelector::onNeedsSurfaceRepaint(const app::Color& newColor)
{
  return (m_color.getRed()   != newColor.getRed()   ||
          m_color.getGreen() != newColor.getGreen() ||
          m_color.getBlue()  != newColor.getBlue()  ? AlphaBarFlag: 0);
}

void ColorSelector::paintColorIndicator(ui::Graphics* g,
                                        const gfx::Point& pos,
                                        const bool white)
{
  SkinTheme* theme = static_cast<SkinTheme*>(this->theme());
  os::Surface* icon = theme->parts.colorWheelIndicator()->bitmap(0);

  g->drawColoredRgbaSurface(
    icon,
    white ? gfx::rgba(255, 255, 255): gfx::rgba(0, 0, 0),
    pos.x-icon->width()/2,
    pos.y-icon->height()/2);
}

gfx::Rect ColorSelector::bottomBarBounds() const
{
  const gfx::Rect rc = childrenBounds();
  const int size = 8*guiscale();      // TODO 8 should be configurable in theme.xml
  if (rc.h > 2*size) {
    if (rc.h > 3*size)          // Alpha bar is visible too
      return gfx::Rect(rc.x, rc.y2()-size*2, rc.w, size);
    else
      return gfx::Rect(rc.x, rc.y2()-size, rc.w, size);
  }
  else
    return gfx::Rect();
}

gfx::Rect ColorSelector::alphaBarBounds() const
{
  const gfx::Rect rc = childrenBounds();
  const int size = 8*guiscale();      // TODO 8 should be configurable in theme.xml
  if (rc.h > 3*size)
    return gfx::Rect(rc.x, rc.y2()-size, rc.w, size);
  else
    return gfx::Rect();
}

} // namespace app
