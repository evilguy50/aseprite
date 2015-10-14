// SHE library
// Copyright (C) 2012-2015  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "she/skia/skia_window_osx.h"

#include "base/unique_ptr.h"
#include "gfx/size.h"
#include "she/event.h"
#include "she/event_queue.h"
#include "she/osx/window.h"
#include "she/skia/skia_display.h"
#include "she/system.h"

#include "mac/SkCGUtils.h"

#if SK_SUPPORT_GPU

  #include "GrContext.h"
  #include "she/gl/gl_context_cgl.h"
  #include "she/skia/gl_context_skia.h"
  #include "she/skia/skia_surface.h"

#endif

namespace she {

class SkiaWindow::Impl : public OSXWindowImpl {
public:
  Impl(EventQueue* queue, SkiaDisplay* display,
       int width, int height, int scale)
    : m_display(display)
    , m_backend(Backend::NONE)
#if SK_SUPPORT_GPU
    , m_nsGL(nil)
#endif
  {
    m_closing = false;
    m_window = [[OSXWindow alloc] initWithImpl:this
                                         width:width
                                        height:height
                                         scale:scale];
  }

  ~Impl() {
#if SK_SUPPORT_GPU
    if (m_backend == Backend::GL)
      detachGL();
#endif
  }

  gfx::Size clientSize() const {
    return [m_window clientSize];
  }

  gfx::Size restoredSize() const {
    return [m_window restoredSize];
  }

  int scale() const {
    return [m_window scale];
  }

  void setScale(int scale) {
    [m_window setScale:scale];
  }

  void setVisible(bool visible) {
    if (visible) {
      // Make the first OSXWindow as the main one.
      [m_window makeKeyAndOrderFront:nil];

      // The main window can be changed only when the NSWindow
      // is visible (i.e. when NSWindow::canBecomeMainWindow
      // returns YES).
      [m_window makeMainWindow];
    }
    else {
      [m_window close];
    }
  }

  void setTitle(const std::string& title) {
    [m_window setTitle:[NSString stringWithUTF8String:title.c_str()]];
  }

  void setMousePosition(const gfx::Point& position) {
    [m_window setMousePosition:position];
  }

  bool setNativeMouseCursor(NativeCursor cursor) {
    return ([m_window setNativeMouseCursor:cursor] ? true: false);
  }

  void updateWindow(const gfx::Rect& bounds) {
    int scale = this->scale();
    NSView* view = m_window.contentView;
    [view setNeedsDisplayInRect:
            NSMakeRect(bounds.x*scale,
                       view.frame.size.height - (bounds.y+bounds.h)*scale,
                       bounds.w*scale,
                       bounds.h*scale)];
    [view displayIfNeeded];
  }

  void* handle() {
    return (__bridge void*)m_window;
  }

  // OSXWindowImpl impl

  void onClose() override {
    m_closing = true;
  }

  void onResize(const gfx::Size& size) override {
    bool gpu = she::instance()->gpuAcceleration();
    (void)gpu;

    // Disable GPU acceleration because it isn't ready yet.
    gpu = false;

#if SK_SUPPORT_GPU
    if (gpu && attachGL()) {
      m_backend = Backend::GL;
    }
    else
#endif
    {
#if SK_SUPPORT_GPU
      detachGL();
#endif
      m_backend = Backend::NONE;
    }

#if SK_SUPPORT_GPU
    if (m_glCtx)
      createRenderTarget(size);
#endif

    m_display->resize(size);
  }

  void onDrawRect(const gfx::Rect& rect) override {
#if SK_SUPPORT_GPU
    // Flush operations to the SkCanvas
    {
      SkiaSurface* surface = static_cast<SkiaSurface*>(m_display->getSurface());
      surface->flush();
    }
#endif

    switch (m_backend) {

      case Backend::NONE:
        paintGC(rect);
        break;

#ifdef SK_SUPPORT_GPU
      case Backend::GL:
        if (m_nsGL)
          [m_nsGL flushBuffer];
        break;
#endif
    }
  }

  void onWindowChanged() override {
#ifdef SK_SUPPORT_GPU
    if (m_nsGL)
      [m_nsGL setView:[m_window contentView]];
#endif
  }

private:
#if SK_SUPPORT_GPU
  bool attachGL() {
    if (!m_glCtx) {
      try {
        auto ctx = new GLContextSkia<GLContextCGL>(nullptr);

        m_glCtx.reset(ctx);
        m_grCtx.reset(GrContext::Create(kOpenGL_GrBackend,
                                        (GrBackendContext)m_glCtx->gl()));

        m_nsGL = [[NSOpenGLContext alloc]
                   initWithCGLContextObj:m_glCtx->cglContext()];

        [m_nsGL setView:m_window.contentView];
      }
      catch (const std::exception& ex) {
        LOG("Cannot create GL context: %s\n", ex.what());
        detachGL();
        return false;
      }
    }
    return true;
  }

  void detachGL() {
    if (m_nsGL)
      m_nsGL = nil;
    m_glCtx.reset(nullptr);
  }

  void createRenderTarget(const gfx::Size& size) {
    int scale = this->scale();
    m_lastSize = size;

    GrBackendRenderTargetDesc desc;
    desc.fWidth = size.w;
    desc.fHeight = size.h;
    desc.fConfig = kSkia8888_GrPixelConfig;
    desc.fOrigin = kBottomLeft_GrSurfaceOrigin;
    desc.fSampleCnt = m_glCtx->getSampleCount();
    desc.fStencilBits = m_glCtx->getStencilBits();
    desc.fRenderTargetHandle = 0; // direct frame buffer
    m_grRenderTarget.reset(m_grCtx->textureProvider()->wrapBackendRenderTarget(desc));
    m_skSurfaceDirect.reset(
      SkSurface::NewRenderTargetDirect(m_grRenderTarget));

    if (scale == 1) {
      m_skSurface.reset(m_skSurfaceDirect);
    }
    else {
      m_skSurface.reset(
        SkSurface::NewRenderTarget(
          m_grCtx,
          SkSurface::kYes_Budgeted,
          SkImageInfo::MakeN32Premul(MAX(1, size.w / scale),
                                     MAX(1, size.h / scale)),
          m_glCtx->getSampleCount()));
    }

    if (!m_skSurface)
      throw std::runtime_error("Error creating OpenGL surface for main display");

    m_display->setSkiaSurface(new SkiaSurface(m_skSurface));

    if (m_nsGL)
      [m_nsGL update];
  }
#endif

  void paintGC(const gfx::Rect& rect) {
    SkiaSurface* surface = static_cast<SkiaSurface*>(m_display->getSurface());
    const SkBitmap& bitmap = surface->bitmap();

    ASSERT(bitmap.width() * bitmap.bytesPerPixel() == bitmap.rowBytes());
    bitmap.lockPixels();

    {
      NSRect viewBounds = [[m_window contentView] bounds];
      NSGraphicsContext* gc = [NSGraphicsContext currentContext];
      CGContextRef cg = (CGContextRef)[gc graphicsPort];
      CGImageRef img = SkCreateCGImageRef(bitmap);
      if (img) {
        CGRect r = CGRectMake(viewBounds.origin.x,
                              viewBounds.origin.y,
                              viewBounds.size.width,
                              viewBounds.size.height);
        CGContextSaveGState(cg);
        CGContextSetInterpolationQuality(cg, kCGInterpolationNone);
        CGContextDrawImage(cg, r, img);
        CGContextRestoreGState(cg);
        CGImageRelease(img);
      }
    }

    bitmap.unlockPixels();
  }

  SkiaDisplay* m_display;
  Backend m_backend;
  bool m_closing;
  OSXWindow* m_window;
#if SK_SUPPORT_GPU
  base::UniquePtr<GLContextSkia<GLContextCGL> > m_glCtx;
  NSOpenGLContext* m_nsGL;
  SkAutoTUnref<GrContext> m_grCtx;
  SkAutoTUnref<GrRenderTarget> m_grRenderTarget;
  SkAutoTDelete<SkSurface> m_skSurfaceDirect;
  SkAutoTDelete<SkSurface> m_skSurface;
  gfx::Size m_lastSize;
#endif
};

SkiaWindow::SkiaWindow(EventQueue* queue, SkiaDisplay* display,
                       int width, int height, int scale)
  : m_impl(new Impl(queue, display,
                    width, height, scale))
{
}

SkiaWindow::~SkiaWindow()
{
  destroyImpl();
}

void SkiaWindow::destroyImpl()
{
  delete m_impl;
  m_impl = nullptr;
}

int SkiaWindow::scale() const
{
  if (m_impl)
    return m_impl->scale();
  else
    return 1;
}

void SkiaWindow::setScale(int scale)
{
  if (m_impl)
    m_impl->setScale(scale);
}

void SkiaWindow::setVisible(bool visible)
{
  if (!m_impl)
    return;

  m_impl->setVisible(visible);
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
  if (!m_impl)
    return gfx::Size(0, 0);

  return m_impl->clientSize();
}

gfx::Size SkiaWindow::restoredSize() const
{
  if (!m_impl)
    return gfx::Size(0, 0);

  return m_impl->restoredSize();
}

void SkiaWindow::setTitle(const std::string& title)
{
  if (!m_impl)
    return;

  m_impl->setTitle(title);
}

void SkiaWindow::captureMouse()
{
}

void SkiaWindow::releaseMouse()
{
}

void SkiaWindow::setMousePosition(const gfx::Point& position)
{
  if (m_impl)
    m_impl->setMousePosition(position);
}

bool SkiaWindow::setNativeMouseCursor(NativeCursor cursor)
{
  if (m_impl)
    return m_impl->setNativeMouseCursor(cursor);
  else
    return false;
}

void SkiaWindow::updateWindow(const gfx::Rect& bounds)
{
  if (m_impl)
    m_impl->updateWindow(bounds);
}

void* SkiaWindow::handle()
{
  if (m_impl)
    return (void*)m_impl->handle();
  else
    return nullptr;
}

} // namespace she
