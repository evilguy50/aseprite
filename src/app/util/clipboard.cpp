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
#include "app/cmd/clear_mask.h"
#include "app/cmd/deselect_mask.h"
#include "app/console.h"
#include "app/context_access.h"
#include "app/document.h"
#include "app/document_api.h"
#include "app/document_range.h"
#include "app/document_range_ops.h"
#include "app/modules/editors.h"
#include "app/modules/gfx.h"
#include "app/modules/gui.h"
#include "app/transaction.h"
#include "app/ui/color_bar.h"
#include "app/ui/editor/editor.h"
#include "app/ui/main_window.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/timeline.h"
#include "app/ui_context.h"
#include "app/util/clipboard.h"
#include "app/util/new_image_from_mask.h"
#include "base/shared_ptr.h"
#include "doc/doc.h"
#include "render/quantization.h"

#ifdef _WIN32
  #define USE_NATIVE_WIN32_CLIPBOARD
#endif

#ifdef USE_NATIVE_WIN32_CLIPBOARD
  #include <windows.h>

  #include "app/util/clipboard_win32.h"
#endif

#include <stdexcept>

namespace app {

namespace {

  class ClipboardRange {
  public:
    ClipboardRange() : m_doc(NULL) {
    }

    bool valid() {
      return m_doc != NULL;
    }

    void invalidate() {
      m_doc = NULL;
    }

    void setRange(Document* doc, const DocumentRange& range) {
      m_doc = doc;
      m_range = range;
    }

    Document* document() const { return m_doc; }
    DocumentRange range() const { return m_range; }

  private:
    Document* m_doc;
    DocumentRange m_range;
  };

}

using namespace doc;

static bool first_time = true;
static base::SharedPtr<Palette> clipboard_palette;
static PalettePicks clipboard_picks;
static ImageRef clipboard_image;
static base::SharedPtr<Mask> clipboard_mask;
static ClipboardRange clipboard_range;

static void on_exit_delete_clipboard()
{
  clipboard_palette.reset();
  clipboard_image.reset();
  clipboard_mask.reset();
}

static void set_clipboard_image(Image* image, Mask* mask, Palette* palette, bool set_system_clipboard)
{
  if (first_time) {
    first_time = false;
    App::instance()->Exit.connect(&on_exit_delete_clipboard);
  }

  clipboard_palette.reset(palette);
  clipboard_picks.clear();
  clipboard_image.reset(image);
  clipboard_mask.reset(mask);

  // copy to the Windows clipboard
#ifdef USE_NATIVE_WIN32_CLIPBOARD
  if (set_system_clipboard)
    set_win32_clipboard_bitmap(image, mask, palette);
#endif

  clipboard_range.invalidate();
}

static bool copy_from_document(const Site& site)
{
  const app::Document* document = static_cast<const app::Document*>(site.document());

  ASSERT(document != NULL);
  ASSERT(document->isMaskVisible());

  Image* image = new_image_from_mask(site);
  if (!image)
    return false;

  const Mask* mask = document->mask();
  const Palette* pal = document->sprite()->palette(site.frame());

  set_clipboard_image(image,
                      (mask ? new Mask(*mask): nullptr),
                      (pal ? new Palette(*pal): nullptr), true);
  return true;
}

clipboard::ClipboardFormat clipboard::get_current_format()
{
#ifdef USE_NATIVE_WIN32_CLIPBOARD
  if (win32_clipboard_contains_bitmap())
    return ClipboardImage;
#endif

  if (clipboard_image)
    return ClipboardImage;
  else if (clipboard_range.valid())
    return ClipboardDocumentRange;
  else if (clipboard_palette && clipboard_picks.picks())
    return ClipboardPaletteEntries;
  else
    return ClipboardNone;
}

void clipboard::get_document_range_info(Document** document, DocumentRange* range)
{
  if (clipboard_range.valid()) {
    *document = clipboard_range.document();
    *range = clipboard_range.range();
  }
  else {
    *document = NULL;
  }
}

void clipboard::clear_content()
{
  set_clipboard_image(nullptr, nullptr, nullptr, true);
}

void clipboard::cut(ContextWriter& writer)
{
  ASSERT(writer.document() != NULL);
  ASSERT(writer.sprite() != NULL);
  ASSERT(writer.layer() != NULL);

  if (!copy_from_document(*writer.site())) {
    Console console;
    console.printf("Can't copying an image portion from the current layer\n");
  }
  else {
    {
      Transaction transaction(writer.context(), "Cut");
      transaction.execute(new cmd::ClearMask(writer.cel()));
      transaction.execute(new cmd::DeselectMask(writer.document()));
      transaction.commit();
    }
    writer.document()->generateMaskBoundaries();
    update_screen_for_document(writer.document());
  }
}

void clipboard::copy(const ContextReader& reader)
{
  ASSERT(reader.document() != NULL);

  if (!copy_from_document(*reader.site())) {
    Console console;
    console.printf("Can't copying an image portion from the current layer\n");
    return;
  }
}

void clipboard::copy_range(const ContextReader& reader, const DocumentRange& range)
{
  ASSERT(reader.document() != NULL);

  ContextWriter writer(reader);

  clear_content();
  clipboard_range.setRange(writer.document(), range);

  // TODO Replace this with a signal, because here the timeline
  // depends on the clipboard and the clipboard on the timeline.
  App::instance()->getMainWindow()
    ->getTimeline()->activateClipboardRange();
}

void clipboard::copy_image(const Image* image, const Mask* mask, const Palette* pal)
{
  set_clipboard_image(
    Image::createCopy(image),
    (mask ? new Mask(*mask): nullptr),
    (pal ? new Palette(*pal): nullptr), true);
}

void clipboard::copy_palette(const Palette* palette, const doc::PalettePicks& picks)
{
  if (!picks.picks())
    return;                     // Do nothing case

  set_clipboard_image(nullptr,
                      nullptr,
                      new Palette(*palette), true);
  clipboard_picks = picks;
}

void clipboard::paste()
{
  Editor* editor = current_editor;
  if (editor == NULL)
    return;

  Document* dstDoc = editor->document();
  Sprite* dstSpr = dstDoc->sprite();

  switch (get_current_format()) {

    case clipboard::ClipboardImage: {
#ifdef USE_NATIVE_WIN32_CLIPBOARD
      // Get the image from the clipboard.
      {
        Image* win32_image = NULL;
        Mask* win32_mask = NULL;
        Palette* win32_palette = NULL;
        get_win32_clipboard_bitmap(win32_image, win32_mask, win32_palette);
        if (win32_image)
          set_clipboard_image(win32_image, win32_mask, win32_palette, false);
      }
#endif

      if (!clipboard_image)
        return;

      Palette* dst_palette = dstSpr->palette(editor->frame());

      // Source image (clipboard or a converted copy to the destination 'imgtype')
      ImageRef src_image;
      if (clipboard_image->pixelFormat() == dstSpr->pixelFormat() &&
        // Indexed images can be copied directly only if both images
        // have the same palette.
        (clipboard_image->pixelFormat() != IMAGE_INDEXED ||
          clipboard_palette->countDiff(dst_palette, NULL, NULL) == 0)) {
        src_image = clipboard_image;
      }
      else {
        RgbMap* dst_rgbmap = dstSpr->rgbMap(editor->frame());

        src_image.reset(
          render::convert_pixel_format(
            clipboard_image.get(), NULL, dstSpr->pixelFormat(),
            DitheringMethod::NONE, dst_rgbmap, clipboard_palette.get(),
            false,
            0));
      }

      // Change to MovingPixelsState
      editor->pasteImage(src_image.get(),
                         clipboard_mask.get());
      break;
    }

    case clipboard::ClipboardDocumentRange: {
      DocumentRange srcRange = clipboard_range.range();
      Document* srcDoc = clipboard_range.document();
      Sprite* srcSpr = srcDoc->sprite();
      std::vector<Layer*> srcLayers;
      std::vector<Layer*> dstLayers;
      srcSpr->getLayersList(srcLayers);
      dstSpr->getLayersList(dstLayers);

      switch (srcRange.type()) {

        case DocumentRange::kCels: {
          // We can use a document range op (copy_range) to copy/paste
          // cels in the same document.
          if (srcDoc == dstDoc) {
            Timeline* timeline = App::instance()->getMainWindow()->getTimeline();
            DocumentRange dstRange = timeline->range();
            LayerIndex dstLayer = srcSpr->layerToIndex(editor->layer());
            frame_t dstFrame = editor->frame();

            if (dstRange.enabled()) {
              dstLayer = dstRange.layerEnd();
              dstFrame = dstRange.frameBegin();
            }

            LayerIndex dstLayer2(int(dstLayer)-srcRange.layers()+1);
            dstRange.startRange(dstLayer, dstFrame, DocumentRange::kCels);
            dstRange.endRange(dstLayer2, dstFrame+srcRange.frames()-1);

            // This is the app::copy_range (not clipboard::copy_range()).
            app::copy_range(srcDoc, srcRange, dstRange, kDocumentRangeBefore);
            editor->invalidate();
            return;
          }

          Transaction transaction(UIContext::instance(), "Paste Cels");
          DocumentApi api = dstDoc->getApi(transaction);

          // Add extra frames if needed
          frame_t dstFrameBegin = editor->frame();
          while (dstFrameBegin+srcRange.frames() > dstSpr->totalFrames())
            api.addFrame(dstSpr, dstSpr->totalFrames());

          for (LayerIndex
                 i = srcRange.layerEnd(),
                 j = dstSpr->layerToIndex(editor->layer());
               i >= srcRange.layerBegin() &&
                 i >= LayerIndex(0) &&
                 j >= LayerIndex(0); --i, --j) {
            // Maps a linked Cel in the original sprite with its
            // corresponding copy in the new sprite. In this way
            // we can.
            std::map<Cel*, Cel*> relatedCels;

            for (frame_t frame = srcRange.frameBegin(),
                   dstFrame = dstFrameBegin;
                 frame <= srcRange.frameEnd();
                 ++frame, ++dstFrame) {
              Cel* srcCel = srcLayers[i]->cel(frame);
              Cel* srcLink = nullptr;

              if (srcCel && srcCel->image()) {
                bool createCopy = true;

                if (dstLayers[j]->isContinuous() &&
                    srcCel->links()) {
                  srcLink = srcCel->link();
                  if (!srcLink)
                    srcLink = srcCel;

                  if (srcLink) {
                    Cel* dstRelated = relatedCels[srcLink];
                    if (dstRelated) {
                      createCopy = false;

                      // Create a link from dstRelated
                      api.copyCel(
                        static_cast<LayerImage*>(dstLayers[j]), dstRelated->frame(),
                        static_cast<LayerImage*>(dstLayers[j]), dstFrame);
                    }
                  }
                }

                if (createCopy) {
                  api.copyCel(
                    static_cast<LayerImage*>(srcLayers[i]), frame,
                    static_cast<LayerImage*>(dstLayers[j]), dstFrame);

                  if (srcLink)
                    relatedCels[srcLink] = dstLayers[j]->cel(dstFrame);
                }
              }
              else {
                Cel* dstCel = dstLayers[j]->cel(dstFrame);
                if (dstCel)
                  api.clearCel(dstCel);
              }
            }

          }

          transaction.commit();
          editor->invalidate();
          break;
        }

        case DocumentRange::kFrames: {
          Transaction transaction(UIContext::instance(), "Paste Frames");
          DocumentApi api = dstDoc->getApi(transaction);
          frame_t dstFrame = frame_t(editor->frame() + 1);

          for (frame_t frame = srcRange.frameBegin(); frame <= srcRange.frameEnd(); ++frame) {
            api.addFrame(dstSpr, dstFrame);

            for (LayerIndex
                   i = LayerIndex(srcLayers.size()-1),
                   j = LayerIndex(dstLayers.size()-1);
                   i >= LayerIndex(0) &&
                   j >= LayerIndex(0); --i, --j) {
              Cel* cel = static_cast<LayerImage*>(srcLayers[i])->cel(frame);
              if (cel && cel->image()) {
                api.copyCel(
                  static_cast<LayerImage*>(srcLayers[i]), frame,
                  static_cast<LayerImage*>(dstLayers[j]), dstFrame);
              }
            }

            ++dstFrame;
          }

          transaction.commit();
          editor->invalidate();
          break;
        }

        case DocumentRange::kLayers: {
          if (srcDoc->colorMode() != dstDoc->colorMode())
            throw std::runtime_error("You cannot copy layers of document with different color modes");

          Transaction transaction(UIContext::instance(), "Paste Layers");
          DocumentApi api = dstDoc->getApi(transaction);

          // Expand frames of dstDoc if it's needed.
          frame_t maxFrame(0);
          for (LayerIndex i = srcRange.layerBegin();
               i <= srcRange.layerEnd() &&
               i < LayerIndex(srcLayers.size()); ++i) {
            Cel* lastCel = static_cast<LayerImage*>(srcLayers[i])->getLastCel();
            if (lastCel && maxFrame < lastCel->frame())
              maxFrame = lastCel->frame();
          }
          while (dstSpr->totalFrames() < maxFrame+1)
            api.addEmptyFrame(dstSpr, dstSpr->totalFrames());

          for (LayerIndex i = srcRange.layerBegin(); i <= srcRange.layerEnd(); ++i) {
            Layer* afterThis;
            if (srcLayers[i]->isBackground() &&
                !dstDoc->sprite()->backgroundLayer()) {
              afterThis = nullptr;
            }
            else
              afterThis = dstSpr->folder()->getLastLayer();

            LayerImage* newLayer = new LayerImage(dstSpr);
            api.addLayer(dstSpr->folder(), newLayer, afterThis);

            srcDoc->copyLayerContent(
              srcLayers[i], dstDoc, newLayer);
          }

          transaction.commit();
          editor->invalidate();
          break;
        }

      }
      break;
    }

  }
}

bool clipboard::get_image_size(gfx::Size& size)
{
#ifdef USE_NATIVE_WIN32_CLIPBOARD
  // Get the image from the clipboard.
  return get_win32_clipboard_bitmap_size(size);
#else
  if (clipboard_image) {
    size.w = clipboard_image->width();
    size.h = clipboard_image->height();
    return true;
  }
  else
    return false;
#endif
}

Palette* clipboard::get_palette()
{
  if (clipboard::get_current_format() == ClipboardPaletteEntries) {
    ASSERT(clipboard_palette);
    return clipboard_palette.get();
  }
  else
    return nullptr;
}

const PalettePicks& clipboard::get_palette_picks()
{
  return clipboard_picks;
}

} // namespace app
