// Aseprite Render Library
// Copyright (c) 2001-2015 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef RENDER_RENDER_H_INCLUDED
#define RENDER_RENDER_H_INCLUDED
#pragma once

#include "doc/anidir.h"
#include "doc/blend_mode.h"
#include "doc/color.h"
#include "doc/frame.h"
#include "doc/pixel_format.h"
#include "gfx/fwd.h"
#include "gfx/size.h"
#include "render/extra_type.h"
#include "render/zoom.h"

namespace gfx {
  class Clip;
}

namespace doc {
  class Cel;
  class FrameTag;
  class Image;
  class Layer;
  class Palette;
  class Sprite;
}

namespace render {
  using namespace doc;

  enum class BgType {
    NONE,
    TRANSPARENT,
    CHECKED,
  };

  enum class OnionskinType {
    NONE,
    MERGE,
    RED_BLUE_TINT,
  };

  class OnionskinOptions {
  public:
    OnionskinOptions(OnionskinType type)
      : m_type(type)
      , m_prevFrames(0)
      , m_nextFrames(0)
      , m_opacityBase(0)
      , m_opacityStep(0)
      , m_loopTag(nullptr) {
    }

    OnionskinType type() const { return m_type; }
    int prevFrames() const { return m_prevFrames; }
    int nextFrames() const { return m_nextFrames; }
    int opacityBase() const { return m_opacityBase; }
    int opacityStep() const { return m_opacityStep; }
    FrameTag* loopTag() const { return m_loopTag; }

    void type(OnionskinType type) { m_type = type; }
    void prevFrames(int prevFrames) { m_prevFrames = prevFrames; }
    void nextFrames(int nextFrames) { m_nextFrames = nextFrames; }
    void opacityBase(int base) { m_opacityBase = base; }
    void opacityStep(int step) { m_opacityStep = step; }
    void loopTag(FrameTag* loopTag) { m_loopTag = loopTag; }

  private:
    OnionskinType m_type;
    int m_prevFrames;
    int m_nextFrames;
    int m_opacityBase;
    int m_opacityStep;
    FrameTag* m_loopTag;
  };

  class Render {
  public:
    Render();

    // Background configuration
    void setBgType(BgType type);
    void setBgZoom(bool state);
    void setBgColor1(color_t color);
    void setBgColor2(color_t color);
    void setBgCheckedSize(const gfx::Size& size);

    // Sets the preview image. This preview image is an alternative
    // image to be used for the given layer/frame.
    void setPreviewImage(const Layer* layer, frame_t frame, Image* drawable);
    void removePreviewImage();

    // Sets an extra cel/image to be drawn after the current
    // layer/frame.
    void setExtraImage(
      ExtraType type,
      const Cel* cel, const Image* image, BlendMode blendMode,
      const Layer* currentLayer,
      frame_t currentFrame);
    void removeExtraImage();

    void setOnionskin(const OnionskinOptions& options);
    void disableOnionskin();

    void renderSprite(
      Image* dstImage,
      const Sprite* sprite,
      frame_t frame);

    void renderSprite(
      Image* dstImage,
      const Sprite* sprite,
      frame_t frame,
      const gfx::Clip& area);

    void renderLayer(
      Image* dstImage,
      const Layer* layer,
      frame_t frame);

    void renderLayer(
      Image* dstImage,
      const Layer* layer,
      frame_t frame,
      const gfx::Clip& area,
      BlendMode blend_mode = BlendMode::UNSPECIFIED);

    // Main function used to render the sprite. Draws the given sprite
    // frame in a new image and return it. Note: zoomedRect must have
    // the zoom applied (zoomedRect = zoom.apply(spriteRect)).
    void renderSprite(
      Image* dstImage,
      const Sprite* sprite,
      frame_t frame,
      const gfx::Clip& area,
      Zoom zoom);

    // Extra functions
    void renderBackground(Image* image,
      const gfx::Clip& area,
      Zoom zoom);

    void renderImage(Image* dst_image, const Image* src_image,
      const Palette* pal, int x, int y, Zoom zoom,
      int opacity, BlendMode blend_mode);

  private:
    typedef void (*RenderScaledImage)(
      Image* dst, const Image* src, const Palette* pal,
      const gfx::Clip& area,
      int opacity, BlendMode blend_mode, Zoom zoom);

    void renderLayer(
      const Layer* layer,
      Image* image,
      const gfx::Clip& area,
      frame_t frame, Zoom zoom,
      RenderScaledImage renderScaledImage,
      bool render_background,
      bool render_transparent,
      BlendMode blend_mode);

    void renderCel(
      Image* dst_image,
      const Image* cel_image,
      const Palette* pal,
      const Cel* cel,
      const gfx::Clip& area,
      RenderScaledImage scaled_func,
      int opacity, BlendMode blend_mode, Zoom zoom);

    static RenderScaledImage getRenderScaledImageFunc(
      PixelFormat dstFormat,
      PixelFormat srcFormat);

    const Sprite* m_sprite;
    const Layer* m_currentLayer;
    frame_t m_currentFrame;
    ExtraType m_extraType;
    const Cel* m_extraCel;
    const Image* m_extraImage;
    BlendMode m_extraBlendMode;

    BgType m_bgType;
    bool m_bgZoom;
    color_t m_bgColor1;
    color_t m_bgColor2;
    gfx::Size m_bgCheckedSize;
    int m_globalOpacity;
    const Layer* m_selectedLayer;
    frame_t m_selectedFrame;
    Image* m_previewImage;
    OnionskinOptions m_onionskin;
  };

  void composite_image(Image* dst, const Image* src,
    int x, int y, int opacity, BlendMode blend_mode);

} // namespace render

#endif
