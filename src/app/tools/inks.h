// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#include "app/tools/ink_processing.h"

#include "app/app.h"                // TODO avoid to include this file
#include "app/color_utils.h"
#include "app/context.h"
#include "app/document.h"
#include "app/document_undo.h"
#include "app/tools/pick_ink.h"
#include "doc/mask.h"
#include "doc/slice.h"
#include "gfx/region.h"

namespace app {
namespace tools {

// Ink used for tools which paint with primary/secondary
// (or foreground/background colors)
class PaintInk : public Ink {
public:
  enum Type { Simple, WithFg, WithBg, Copy, LockAlpha };

private:
  Type m_type;
  AlgoHLine m_proc;

public:
  PaintInk(Type type) : m_type(type), m_proc(nullptr) { }

  Ink* clone() override { return new PaintInk(*this); }

  bool isPaint() const override { return true; }

  void prepareInk(ToolLoop* loop) override {
    switch (m_type) {

      case Simple:
        // Do nothing, use the default colors
        break;

      case WithFg:
      case WithBg:
        {
          int color = (m_type == WithFg ? loop->getFgColor():
                                          loop->getBgColor());
          loop->setPrimaryColor(color);
          loop->setSecondaryColor(color);
        }
        break;
    }

    auto pixelFormat = loop->sprite()->pixelFormat();

    if (loop->getBrush()->type() == doc::kImageBrushType)
      m_proc = get_ink_proc<BrushInkProcessing>(pixelFormat);
    else {
      switch (m_type) {
        case Simple: {
          bool opaque = false;

          if (loop->getOpacity() == 255) {
            color_t color = loop->getPrimaryColor();

            switch (loop->sprite()->pixelFormat()) {
              case IMAGE_RGB:
                opaque = (rgba_geta(color) == 255);
                break;
              case IMAGE_GRAYSCALE:
                opaque = (graya_geta(color) == 255);
                break;
              case IMAGE_INDEXED:
                color = get_current_palette()->getEntry(color);
                opaque = (rgba_geta(color) == 255);
                break;
            }
          }

          // Use a faster ink, direct copy
          if (opaque)
            m_proc = get_ink_proc<CopyInkProcessing>(pixelFormat);
          else
            m_proc = get_ink_proc<TransparentInkProcessing>(pixelFormat);
          break;
        }
        case Copy:
          m_proc = get_ink_proc<CopyInkProcessing>(pixelFormat);
          break;
        case LockAlpha:
          m_proc = get_ink_proc<LockAlphaInkProcessing>(pixelFormat);
          break;
        default:
          m_proc = get_ink_proc<TransparentInkProcessing>(pixelFormat);
          break;
      }
    }
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop) override {
    ASSERT(m_proc);
    (*m_proc)(x1, y, x2, loop);
  }

};


class ShadingInk : public Ink {
private:
  AlgoHLine m_proc;

public:
  ShadingInk() { }

  Ink* clone() override { return new ShadingInk(*this); }

  bool isPaint() const override { return true; }
  bool isShading() const override { return true; }

  void prepareInk(ToolLoop* loop) override {
    m_proc = get_ink_proc<ShadingInkProcessing>(loop->sprite()->pixelFormat());
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop) override {
    (*m_proc)(x1, y, x2, loop);
  }

};


class ScrollInk : public Ink {
public:
  Ink* clone() override { return new ScrollInk(*this); }

  bool isScrollMovement() const override { return true; }

  void prepareInk(ToolLoop* loop) override {
    // Do nothing
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop) override {
    // Do nothing
  }

};


class ZoomInk : public Ink {
public:
  Ink* clone() override { return new ZoomInk(*this); }

  bool isZoom() const override { return true; }
  void prepareInk(ToolLoop* loop) override { }
  void inkHline(int x1, int y, int x2, ToolLoop* loop) override { }
};


class MoveInk : public Ink {
public:
  Ink* clone() override { return new MoveInk(*this); }

  bool isCelMovement() const override { return true; }
  void prepareInk(ToolLoop* loop) override { }
  void inkHline(int x1, int y, int x2, ToolLoop* loop) override { }
};


class SliceInk : public Ink {
  AlgoHLine m_proc;
  bool m_createSlice;
  gfx::Rect m_maxBounds;

public:
  SliceInk() {
    m_createSlice = false;
  }

  Ink* clone() override { return new SliceInk(*this); }

  bool isSlice() const override { return true; }
  bool needsCelCoordinates() const override { return false; }

  void prepareInk(ToolLoop* loop) override {
    m_proc = get_ink_proc<XorInkProcessing>(loop->sprite()->pixelFormat());
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop) override {
    if (m_createSlice)
      m_maxBounds |= gfx::Rect(x1, y, x2-x1+1, 1);
    else
      (*m_proc)(x1, y, x2, loop);
  }

  void setFinalStep(ToolLoop* loop, bool state) override {
    m_createSlice = state;
    if (state) {
      m_maxBounds = gfx::Rect(0, 0, 0, 0);
    }
    else if (loop->getMouseButton() == ToolLoop::Left) {
      Slice* slice = new Slice;
      SliceKey key(m_maxBounds);
      slice->insert(loop->getFrame(), key);
      loop->addSlice(slice);
    }
  }
};


class EraserInk : public Ink {
public:
  enum Type { Eraser, ReplaceFgWithBg, ReplaceBgWithFg };

private:
  AlgoHLine m_proc;
  Type m_type;

public:
  EraserInk(Type type) : m_type(type) { }

  Ink* clone() override { return new EraserInk(*this); }

  bool isPaint() const override { return true; }
  bool isEffect() const override { return true; }
  bool isEraser() const override { return true; }

  void prepareInk(ToolLoop* loop) override {
    switch (m_type) {

      case Eraser: {
        color_t primary = app_get_color_to_clear_layer(loop->getLayer());
        color_t secondary = app_get_color_to_clear_layer(loop->getLayer());

        if (loop->getOpacity() == 255) {
          m_proc = get_ink_proc<CopyInkProcessing>(loop->sprite()->pixelFormat());
        }
        else {
          // For opaque layers
          if (loop->getLayer()->isBackground()) {
            m_proc = get_ink_proc<TransparentInkProcessing>(loop->sprite()->pixelFormat());
          }
          // For transparent layers
          else {
            m_proc = get_ink_proc<MergeInkProcessing>(loop->sprite()->pixelFormat());

            if (loop->sprite()->pixelFormat() == IMAGE_INDEXED) {
              primary = loop->sprite()->transparentColor();
            }
          }
        }

        // TODO app_get_color_to_clear_layer should receive the context as parameter
        loop->setPrimaryColor(primary);
        loop->setSecondaryColor(secondary);
        break;
      }

      case ReplaceFgWithBg:
        m_proc = get_ink_proc<ReplaceInkProcessing>(loop->sprite()->pixelFormat());

        loop->setPrimaryColor(loop->getFgColor());
        loop->setSecondaryColor(loop->getBgColor());
        break;

      case ReplaceBgWithFg:
        m_proc = get_ink_proc<ReplaceInkProcessing>(loop->sprite()->pixelFormat());

        loop->setPrimaryColor(loop->getBgColor());
        loop->setSecondaryColor(loop->getFgColor());
        break;
    }
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop) override {
    (*m_proc)(x1, y, x2, loop);
  }
};


class BlurInk : public Ink {
  AlgoHLine m_proc;

public:
  Ink* clone() override { return new BlurInk(*this); }

  bool isPaint() const override { return true; }
  bool isEffect() const override { return true; }
  bool needsSpecialSourceArea() const override { return true; }

  void prepareInk(ToolLoop* loop) override {
    m_proc = get_ink_proc<BlurInkProcessing>(loop->sprite()->pixelFormat());
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop) override {
    (*m_proc)(x1, y, x2, loop);
  }

  void createSpecialSourceArea(const gfx::Region& dirtyArea, gfx::Region& sourceArea) const override {
    // We need one pixel more for each side, to use a 3x3 convolution matrix.
    for (const auto& rc : dirtyArea) {
      sourceArea.createUnion(sourceArea,
        gfx::Region(gfx::Rect(rc).enlarge(1)));
    }
  }
};


class JumbleInk : public Ink {
  AlgoHLine m_proc;

public:
  Ink* clone() override { return new JumbleInk(*this); }

  bool isPaint() const override { return true; }
  bool isEffect() const override { return true; }
  bool needsSpecialSourceArea() const override { return true; }

  void prepareInk(ToolLoop* loop) override {
    m_proc = get_ink_proc<JumbleInkProcessing>(loop->sprite()->pixelFormat());
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop) override {
    (*m_proc)(x1, y, x2, loop);
  }

  void createSpecialSourceArea(const gfx::Region& dirtyArea, gfx::Region& sourceArea) const override {
    // We need one pixel more for each side.
    for (const auto& rc : dirtyArea) {
      sourceArea.createUnion(sourceArea,
        gfx::Region(gfx::Rect(rc).enlarge(1)));
    }
  }
};


// Ink used for selection tools (like Rectangle Marquee, Lasso, Magic Wand, etc.)
class SelectionInk : public Ink {
  bool m_modify_selection;
  Mask m_mask;
  Rect m_maxBounds;
  AlgoHLine m_proc;

public:
  SelectionInk() {
    m_modify_selection = false;
  }

  Ink* clone() override { return new SelectionInk(*this); }

  void prepareInk(ToolLoop* loop) override {
    m_proc = get_ink_proc<XorInkProcessing>(loop->sprite()->pixelFormat());
  }

  bool isSelection() const override { return true; }
  bool needsCelCoordinates() const override {
    return (m_modify_selection ? false: true);
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop) override {
    if (m_modify_selection) {
      int modifiers = int(loop->getModifiers());

      if ((modifiers & (int(ToolLoopModifiers::kReplaceSelection) |
                        int(ToolLoopModifiers::kAddSelection))) != 0) {
        m_mask.add(gfx::Rect(x1, y, x2-x1+1, 1));
      }
      else if ((modifiers & int(ToolLoopModifiers::kSubtractSelection)) != 0) {
        m_mask.subtract(gfx::Rect(x1, y, x2-x1+1, 1));
      }

      m_maxBounds |= gfx::Rect(x1, y, x2-x1+1, 1);
    }
    else {
      (*m_proc)(x1, y, x2, loop);
    }
  }

  void setFinalStep(ToolLoop* loop, bool state) override {
    m_modify_selection = state;

    if (state) {
      m_maxBounds = loop->getMask()->bounds();

      m_mask.copyFrom(loop->getMask());
      m_mask.freeze();
      m_mask.reserve(loop->sprite()->bounds());
    }
    else {
      // We can intersect the used bounds in inkHline() calls to
      // reduce the shrink computation.
      m_mask.intersect(m_maxBounds);

      m_mask.unfreeze();

      loop->setMask(&m_mask);
      loop->getDocument()->setTransformation(
        Transformation(RectF(m_mask.bounds())));

      m_mask.clear();
    }
  }

};


} // namespace tools
} // namespace app
