// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/cmd_flip.h"

#include "app/app.h"
#include "app/cmd/flip_mask.h"
#include "app/cmd/flip_masked_cel.h"
#include "app/cmd/set_cel_bounds.h"
#include "app/cmd/set_mask_position.h"
#include "app/cmd/trim_cel.h"
#include "app/commands/params.h"
#include "app/context_access.h"
#include "app/document_api.h"
#include "app/document_range.h"
#include "app/i18n/strings.h"
#include "app/modules/gui.h"
#include "app/transaction.h"
#include "app/ui/timeline/timeline.h"
#include "app/util/expand_cel_canvas.h"
#include "app/util/range_utils.h"
#include "doc/algorithm/flip_image.h"
#include "doc/cel.h"
#include "doc/cels_range.h"
#include "doc/image.h"
#include "doc/layer.h"
#include "doc/mask.h"
#include "doc/sprite.h"
#include "fmt/format.h"
#include "gfx/size.h"

namespace app {

FlipCommand::FlipCommand()
  : Command("Flip", CmdRecordableFlag)
{
  m_flipMask = false;
  m_flipType = doc::algorithm::FlipHorizontal;
}

void FlipCommand::onLoadParams(const Params& params)
{
  std::string target = params.get("target");
  m_flipMask = (target == "mask");

  std::string orientation = params.get("orientation");
  m_flipType = (orientation == "vertical" ? doc::algorithm::FlipVertical:
                                            doc::algorithm::FlipHorizontal);
}

bool FlipCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable);
}

void FlipCommand::onExecute(Context* context)
{
  ContextWriter writer(context);
  Document* document = writer.document();
  Sprite* sprite = writer.sprite();

  {
    Transaction transaction(writer.context(),
      m_flipMask ?
      (m_flipType == doc::algorithm::FlipHorizontal ?
        "Flip Horizontal":
        "Flip Vertical"):
      (m_flipType == doc::algorithm::FlipHorizontal ?
        "Flip Canvas Horizontal":
        "Flip Canvas Vertical"));
    DocumentApi api = document->getApi(transaction);

    CelList cels;
    if (m_flipMask) {
      auto range = App::instance()->timeline()->range();
      if (range.enabled())
        cels = get_unique_cels(sprite, range);
      else if (writer.cel())
        cels.push_back(writer.cel());
    }
    else {
      for (Cel* cel : sprite->uniqueCels())
        cels.push_back(cel);
    }

    Mask* mask = document->mask();
    if (m_flipMask && document->isMaskVisible()) {
      Site site = *writer.site();

      for (Cel* cel : cels) {
        // TODO add support to flip masked part of a reference layer
        if (cel->layer()->isReference())
          continue;

        site.frame(cel->frame());
        site.layer(cel->layer());

        int x, y;
        Image* image = site.image(&x, &y);
        if (!image)
          continue;

        // When the mask is inside the cel, we can try to flip the
        // pixels inside the image.
        if (cel->bounds().contains(mask->bounds())) {
          gfx::Rect flipBounds = mask->bounds();
          flipBounds.offset(-x, -y);
          flipBounds &= image->bounds();
          if (flipBounds.isEmpty())
            continue;

          if (mask->bitmap() && !mask->isRectangular())
            transaction.execute(new cmd::FlipMaskedCel(cel, m_flipType));
          else
            api.flipImage(image, flipBounds, m_flipType);

          if (cel->layer()->isTransparent())
            transaction.execute(new cmd::TrimCel(cel));
        }
        // When the mask is bigger than the cel bounds, we have to
        // expand the cel, make the flip, and shrink it again.
        else {
          gfx::Rect flipBounds = (sprite->bounds() & mask->bounds());
          if (flipBounds.isEmpty())
            continue;

          ExpandCelCanvas expand(
            site, cel->layer(),
            TiledMode::NONE, transaction,
            ExpandCelCanvas::None);

          expand.validateDestCanvas(gfx::Region(flipBounds));

          if (mask->bitmap() && !mask->isRectangular())
            doc::algorithm::flip_image_with_mask(
              expand.getDestCanvas(), mask, m_flipType,
              document->bgColor(cel->layer()));
          else
            doc::algorithm::flip_image(
              expand.getDestCanvas(),
              flipBounds, m_flipType);

          expand.commit();
        }
      }
    }
    else {
      for (Cel* cel : cels) {
        Image* image = cel->image();

        // Flip reference layer cel
        if (cel->layer()->isReference()) {
          gfx::RectF bounds = cel->boundsF();

          if (m_flipType == doc::algorithm::FlipHorizontal)
            bounds.x = sprite->width() - bounds.w - bounds.x;
          if (m_flipType == doc::algorithm::FlipVertical)
            bounds.y = sprite->height() - bounds.h - bounds.y;

          transaction.execute(new cmd::SetCelBoundsF(cel, bounds));
        }
        else {
          api.setCelPosition
            (sprite, cel,
             (m_flipType == doc::algorithm::FlipHorizontal ?
              sprite->width() - image->width() - cel->x():
              cel->x()),
             (m_flipType == doc::algorithm::FlipVertical ?
              sprite->height() - image->height() - cel->y():
              cel->y()));
        }

        api.flipImage(image, image->bounds(), m_flipType);
      }
    }

    // Flip the mask.
    Image* maskBitmap = mask->bitmap();
    if (maskBitmap) {
      transaction.execute(new cmd::FlipMask(document, m_flipType));

      // Flip the mask position because the
      if (!m_flipMask)
        transaction.execute(
          new cmd::SetMaskPosition(
            document,
            gfx::Point(
              (m_flipType == doc::algorithm::FlipHorizontal ?
               sprite->width() - mask->bounds().x2():
               mask->bounds().x),
              (m_flipType == doc::algorithm::FlipVertical ?
               sprite->height() - mask->bounds().y2():
               mask->bounds().y))));

      document->generateMaskBoundaries();
    }

    transaction.commit();
  }

  update_screen_for_document(document);
}

std::string FlipCommand::onGetFriendlyName() const
{
  std::string content;
  std::string orientation;

  if (m_flipMask)
    content = Strings::commands_Flip_Selection();
  else
    content = Strings::commands_Flip_Canvas();

  if (m_flipType == doc::algorithm::FlipHorizontal)
    content = Strings::commands_Flip_Horizontally();
  else
    content = Strings::commands_Flip_Vertically();

  return fmt::format(getBaseFriendlyName(), content, orientation);
}

Command* CommandFactory::createFlipCommand()
{
  return new FlipCommand;
}

} // namespace app
