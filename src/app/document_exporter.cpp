// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/document_exporter.h"

#include "app/cmd/set_pixel_format.h"
#include "app/console.h"
#include "app/document.h"
#include "app/file/file.h"
#include "app/filename_formatter.h"
#include "app/ui_context.h"
#include "base/convert_to.h"
#include "base/path.h"
#include "base/unique_ptr.h"
#include "doc/algorithm/shrink_bounds.h"
#include "doc/cel.h"
#include "doc/dithering_method.h"
#include "doc/image.h"
#include "doc/layer.h"
#include "doc/palette.h"
#include "doc/primitives.h"
#include "doc/sprite.h"
#include "gfx/packing_rects.h"
#include "gfx/size.h"
#include "render/render.h"

#include <cstdio>
#include <fstream>
#include <iostream>

using namespace doc;

namespace app {

class DocumentExporter::Sample {
public:
  Sample(Document* document, Sprite* sprite, Layer* layer,
    frame_t frame, const std::string& filename) :
    m_document(document),
    m_sprite(sprite),
    m_layer(layer),
    m_frame(frame),
    m_filename(filename),
    m_originalSize(sprite->width(), sprite->height()),
    m_trimmedBounds(0, 0, sprite->width(), sprite->height()),
    m_inTextureBounds(0, 0, sprite->width(), sprite->height()) {
  }

  Document* document() const { return m_document; }
  Sprite* sprite() const { return m_sprite; }
  Layer* layer() const { return m_layer; }
  frame_t frame() const { return m_frame; }
  std::string filename() const { return m_filename; }
  const gfx::Size& originalSize() const { return m_originalSize; }
  const gfx::Rect& trimmedBounds() const { return m_trimmedBounds; }
  const gfx::Rect& inTextureBounds() const { return m_inTextureBounds; }

  bool trimmed() const {
    return m_trimmedBounds.x > 0
      || m_trimmedBounds.y > 0
      || m_trimmedBounds.w != m_originalSize.w
      || m_trimmedBounds.h != m_originalSize.h;
  }

  void setOriginalSize(const gfx::Size& size) { m_originalSize = size; }
  void setTrimmedBounds(const gfx::Rect& bounds) { m_trimmedBounds = bounds; }
  void setInTextureBounds(const gfx::Rect& bounds) { m_inTextureBounds = bounds; }

private:
  Document* m_document;
  Sprite* m_sprite;
  Layer* m_layer;
  frame_t m_frame;
  std::string m_filename;
  gfx::Size m_originalSize;
  gfx::Rect m_trimmedBounds;
  gfx::Rect m_inTextureBounds;
};

class DocumentExporter::Samples {
public:
  typedef std::list<Sample> List;
  typedef List::iterator iterator;
  typedef List::const_iterator const_iterator;

  bool empty() const { return m_samples.empty(); }

  void addSample(const Sample& sample) {
    m_samples.push_back(sample);
  }

  iterator begin() { return m_samples.begin(); }
  iterator end() { return m_samples.end(); }
  const_iterator begin() const { return m_samples.begin(); }
  const_iterator end() const { return m_samples.end(); }

private:
  List m_samples;
};

class DocumentExporter::LayoutSamples {
public:
  virtual ~LayoutSamples() { }
  virtual void layoutSamples(Samples& samples, int& width, int& height) = 0;
};

class DocumentExporter::SimpleLayoutSamples :
    public DocumentExporter::LayoutSamples {
public:
  void layoutSamples(Samples& samples, int& width, int& height) override {
    const Sprite* oldSprite = NULL;
    const Layer* oldLayer = NULL;

    gfx::Point framePt(0, 0);
    gfx::Size rowSize(0, 0);

    for (auto& sample : samples) {
      const Sprite* sprite = sample.sprite();
      const Layer* layer = sample.layer();
      gfx::Size size = sample.trimmedBounds().getSize();

      if (oldSprite) {
        // If the user didn't specify a width for the texture, we put
        // each sprite/layer in a different row.
        if (width == 0) {
          // New sprite or layer, go to next row.
          if (oldSprite != sprite || oldLayer != layer) {
            framePt.x = 0;
            framePt.y += rowSize.h;
            rowSize = size;
          }
        }
        // When a texture width is specified, we can put different
        // sprites/layers in each row until we reach the texture
        // right-border.
        else if (framePt.x+size.w > width) {
          framePt.x = 0;
          framePt.y += rowSize.h;
          rowSize = size;
        }
      }

      sample.setInTextureBounds(gfx::Rect(framePt, size));

      // Next frame position.
      framePt.x += size.w;
      rowSize = rowSize.createUnion(size);

      oldSprite = sprite;
      oldLayer = layer;
    }
  }
};

class DocumentExporter::BestFitLayoutSamples :
    public DocumentExporter::LayoutSamples {
public:
  void layoutSamples(Samples& samples, int& width, int& height) override {
    gfx::PackingRects pr;

    for (auto& sample : samples)
      pr.add(sample.trimmedBounds().getSize());

    if (width == 0 || height == 0) {
      gfx::Size sz = pr.bestFit();
      width = sz.w;
      height = sz.h;
    }
    else
      pr.pack(gfx::Size(width, height));

    auto it = samples.begin();
    for (auto& rc : pr) {
      ASSERT(it != samples.end());
      it->setInTextureBounds(rc);
      ++it;
    }
  }
};

DocumentExporter::DocumentExporter()
 : m_dataFormat(DefaultDataFormat)
 , m_textureFormat(DefaultTextureFormat)
 , m_textureWidth(0)
 , m_textureHeight(0)
 , m_texturePack(false)
 , m_scale(1.0)
 , m_scaleMode(DefaultScaleMode)
 , m_ignoreEmptyCels(false)
 , m_trimCels(false)
{
}

void DocumentExporter::exportSheet()
{
  // We output the metadata to std::cout if the user didn't specify a file.
  std::ofstream fos;
  std::streambuf* osbuf;
  if (m_dataFilename.empty())
    osbuf = std::cout.rdbuf();
  else {
    fos.open(m_dataFilename.c_str(), std::ios::out);
    osbuf = fos.rdbuf();
  }
  std::ostream os(osbuf);

  // Steps for sheet construction:
  // 1) Capture the samples (each sprite+frame pair)
  Samples samples;
  captureSamples(samples);
  if (samples.empty()) {
    Console console;
    console.printf("No documents to export");
    return;
  }

  // 2) Layout those samples in a texture field.
  if (m_texturePack) {
    BestFitLayoutSamples layout;
    layout.layoutSamples(samples, m_textureWidth, m_textureHeight);
  }
  else {
    SimpleLayoutSamples layout;
    layout.layoutSamples(samples, m_textureWidth, m_textureHeight);
  }

  // 3) Create and render the texture.
  base::UniquePtr<Document> textureDocument(
    createEmptyTexture(samples));

  Sprite* texture = textureDocument->sprite();
  Image* textureImage = texture->folder()->getFirstLayer()
    ->cel(frame_t(0))->image();

  renderTexture(samples, textureImage);

  // Save the metadata.
  createDataFile(samples, os, textureImage);

  // Save the image files.
  if (!m_textureFilename.empty()) {
    textureDocument->setFilename(m_textureFilename.c_str());
    save_document(UIContext::instance(), textureDocument.get());
  }
}

void DocumentExporter::captureSamples(Samples& samples)
{
  std::vector<char> buf(32);

  for (auto& item : m_documents) {
    Document* doc = item.doc;
    Sprite* sprite = doc->sprite();
    Layer* layer = item.layer;
    bool hasFrames = (doc->sprite()->totalFrames() > frame_t(1));
    bool hasLayer = (layer != NULL);

    std::string format = m_filenameFormat;
    if (format.empty()) {
      if (hasFrames && hasLayer)
        format = "{title} ({layer}) {frame}.{extension}";
      else if (hasFrames)
        format = "{title} {frame}.{extension}";
      else if (hasLayer)
        format = "{title} ({layer}).{extension}";
      else
        format = "{fullname}";
    }

    for (frame_t frame=frame_t(0);
         frame<sprite->totalFrames(); ++frame) {
      std::string filename =
        filename_formatter(format,
          doc->filename(),
          layer ? layer->name(): "",
          (sprite->totalFrames() > frame_t(1)) ? frame: frame_t(-1));

      Sample sample(doc, sprite, layer, frame, filename);

      if (m_ignoreEmptyCels || m_trimCels) {
        if (layer && layer->isImage() && !layer->cel(frame)) {
          // Empty cel this sample completely
          continue;
        }

        base::UniquePtr<Image> sampleRender(
          Image::create(sprite->pixelFormat(),
            sprite->width(),
            sprite->height(),
            m_sampleRenderBuf));

        sampleRender->setMaskColor(sprite->transparentColor());
        clear_image(sampleRender, sprite->transparentColor());
        renderSample(sample, sampleRender);

        gfx::Rect frameBounds;
        doc::color_t refColor = 0;

        if (m_trimCels)
          refColor = get_pixel(sampleRender, 0, 0);
        else if (m_ignoreEmptyCels)
          refColor = sprite->transparentColor();

        if (!algorithm::shrink_bounds(sampleRender, frameBounds, refColor)) {
          // If shrink_bounds returns false, it's because the whole
          // image is transparent (equal to the mask color).
          continue;
        }

        if (m_trimCels)
          sample.setTrimmedBounds(frameBounds);
      }

      samples.addSample(sample);
    }
  }
}

Document* DocumentExporter::createEmptyTexture(const Samples& samples)
{
  Palette* palette = NULL;
  PixelFormat pixelFormat = IMAGE_INDEXED;
  gfx::Rect fullTextureBounds(0, 0, m_textureWidth, m_textureHeight);
  int maxColors = 256;

  for (Samples::const_iterator
         it = samples.begin(),
         end = samples.end(); it != end; ++it) {
    // We try to render an indexed image. But if we find a sprite with
    // two or more palettes, or two of the sprites have different
    // palettes, we've to use RGB format.
    if (pixelFormat == IMAGE_INDEXED) {
      if (it->sprite()->pixelFormat() != IMAGE_INDEXED) {
        pixelFormat = IMAGE_RGB;
      }
      else if (it->sprite()->getPalettes().size() > 1) {
        pixelFormat = IMAGE_RGB;
      }
      else if (palette != NULL
        && palette->countDiff(it->sprite()->palette(frame_t(0)), NULL, NULL) > 0) {
        pixelFormat = IMAGE_RGB;
      }
      else
        palette = it->sprite()->palette(frame_t(0));
    }

    fullTextureBounds = fullTextureBounds.createUnion(it->inTextureBounds());
  }

  base::UniquePtr<Sprite> sprite(Sprite::createBasicSprite(
      pixelFormat, fullTextureBounds.w, fullTextureBounds.h, maxColors));

  if (palette != NULL)
    sprite->setPalette(palette, false);

  base::UniquePtr<Document> document(new Document(sprite));
  sprite.release();

  return document.release();
}

void DocumentExporter::renderTexture(const Samples& samples, Image* textureImage)
{
  textureImage->clear(0);

  for (const auto& sample : samples) {
    // Make the sprite compatible with the texture so the render()
    // works correctly.
    if (sample.sprite()->pixelFormat() != textureImage->pixelFormat()) {
      cmd::SetPixelFormat(
        sample.sprite(),
        textureImage->pixelFormat(),
        DitheringMethod::NONE).execute(UIContext::instance());
    }

    renderSample(sample, textureImage);
  }
}

void DocumentExporter::createDataFile(const Samples& samples, std::ostream& os, Image* textureImage)
{
  os << "{ \"frames\": {\n";
  for (Samples::const_iterator
         it = samples.begin(),
         end = samples.end(); it != end; ) {
    const Sample& sample = *it;
    gfx::Size srcSize = sample.originalSize();
    gfx::Rect spriteSourceBounds = sample.trimmedBounds();
    gfx::Rect frameBounds = sample.inTextureBounds();

    os << "   \"" << sample.filename() << "\": {\n"
       << "    \"frame\": { "
       << "\"x\": " << frameBounds.x << ", "
       << "\"y\": " << frameBounds.y << ", "
       << "\"w\": " << frameBounds.w << ", "
       << "\"h\": " << frameBounds.h << " },\n"
       << "    \"rotated\": false,\n"
       << "    \"trimmed\": " << (sample.trimmed() ? "true": "false") << ",\n"
       << "    \"spriteSourceSize\": { "
       << "\"x\": " << spriteSourceBounds.x << ", "
       << "\"y\": " << spriteSourceBounds.y << ", "
       << "\"w\": " << spriteSourceBounds.w << ", "
       << "\"h\": " << spriteSourceBounds.h << " },\n"
       << "    \"sourceSize\": { "
       << "\"w\": " << srcSize.w << ", "
       << "\"h\": " << srcSize.h << " },\n"
       << "    \"duration\": " << sample.sprite()->frameDuration(sample.frame()) << "\n"
       << "   }";

    if (++it != samples.end())
      os << ",\n";
    else
      os << "\n";
  }

  os << " },\n"
     << " \"meta\": {\n"
     << "  \"app\": \"" << WEBSITE << "\",\n"
     << "  \"version\": \"" << VERSION << "\",\n";
  if (!m_textureFilename.empty())
    os << "  \"image\": \"" << m_textureFilename.c_str() << "\",\n";
  os << "  \"format\": \"" << (textureImage->pixelFormat() == IMAGE_RGB ? "RGBA8888": "I8") << "\",\n"
     << "  \"size\": { "
     << "\"w\": " << textureImage->width() << ", "
     << "\"h\": " << textureImage->height() << " },\n"
     << "  \"scale\": \"" << m_scale << "\"\n"
     << " }\n"
     << "}\n";
}

void DocumentExporter::renderSample(const Sample& sample, doc::Image* dst)
{
  render::Render render;
  gfx::Clip clip(
    sample.inTextureBounds().x,
    sample.inTextureBounds().y, sample.trimmedBounds());

  if (sample.layer()) {
    render.renderLayer(dst, sample.layer(), sample.frame(), clip);
  }
  else {
    render.renderSprite(dst, sample.sprite(), sample.frame(), clip);
  }
}

} // namespace app
