// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>

#include "app/app.h"
#include "app/cmd/set_palette.h"
#include "app/color.h"
#include "app/console.h"
#include "app/context.h"
#include "app/context_access.h"
#include "app/document.h"
#include "app/modules/gfx.h"
#include "app/modules/gui.h"
#include "app/modules/palettes.h"
#include "app/transaction.h"
#include "app/ui/color_selector.h"
#include "app/ui/palette_view.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/skin/style.h"
#include "app/ui/styled_button.h"
#include "app/ui_context.h"
#include "base/bind.h"
#include "base/scoped_value.h"
#include "doc/image.h"
#include "doc/image_bits.h"
#include "doc/palette.h"
#include "doc/sprite.h"
#include "gfx/border.h"
#include "gfx/size.h"
#include "ui/ui.h"

namespace app {

using namespace ui;
using namespace doc;

enum {
  INDEX_MODE,
  RGB_MODE,
  HSB_MODE,
  GRAY_MODE,
  MASK_MODE
};

class ColorSelector::WarningIcon : public StyledButton {
public:
  WarningIcon()
    : StyledButton(skin::SkinTheme::instance()->styles.warningBox()) {
  }
};

ColorSelector::ColorSelector()
  : PopupWindowPin("Color Selector", PopupWindow::kCloseOnClickInOtherWindow)
  , m_vbox(JI_VERTICAL)
  , m_topBox(JI_HORIZONTAL)
  , m_color(app::Color::fromMask())
  , m_colorPalette(false, PaletteView::SelectOneColor, this, 7*guiscale())
  , m_colorType(5)
  , m_maskLabel("Transparent Color Selected")
  , m_warningIcon(new WarningIcon)
  , m_disableHexUpdate(false)
{
  m_colorType.addItem("Index");
  m_colorType.addItem("RGB");
  m_colorType.addItem("HSB");
  m_colorType.addItem("Gray");
  m_colorType.addItem("Mask");

  m_topBox.setBorder(gfx::Border(0));
  m_topBox.child_spacing = 0;

  m_colorPaletteContainer.attachToView(&m_colorPalette);

  m_colorPaletteContainer.setExpansive(true);

  m_topBox.addChild(&m_colorType);
  m_topBox.addChild(new Separator("", JI_VERTICAL));
  m_topBox.addChild(&m_hexColorEntry);
  m_topBox.addChild(m_warningIcon);
  {
    Box* miniVbox = new Box(JI_VERTICAL);
    miniVbox->addChild(getPin());
    m_topBox.addChild(new BoxFiller);
    m_topBox.addChild(miniVbox);
  }
  m_vbox.addChild(&m_topBox);
  m_vbox.addChild(&m_colorPaletteContainer);
  m_vbox.addChild(&m_rgbSliders);
  m_vbox.addChild(&m_hsvSliders);
  m_vbox.addChild(&m_graySlider);
  m_vbox.addChild(&m_maskLabel);
  addChild(&m_vbox);

  m_colorType.ItemChange.connect(&ColorSelector::onColorTypeClick, this);
  m_warningIcon->Click.connect(&ColorSelector::onFixWarningClick, this);

  m_rgbSliders.ColorChange.connect(&ColorSelector::onColorSlidersChange, this);
  m_hsvSliders.ColorChange.connect(&ColorSelector::onColorSlidersChange, this);
  m_graySlider.ColorChange.connect(&ColorSelector::onColorSlidersChange, this);
  m_hexColorEntry.ColorChange.connect(&ColorSelector::onColorHexEntryChange, this);

  selectColorType(app::Color::RgbType);
  setPreferredSize(gfx::Size(300*guiscale(), getPreferredSize().h));

  m_onPaletteChangeConn =
    App::instance()->PaletteChange.connect(&ColorSelector::onPaletteChange, this);

  m_tooltips.addTooltipFor(m_warningIcon, "This color isn't in the palette\nPress here to add it.", JI_BOTTOM);

  initTheme();
}

ColorSelector::~ColorSelector()
{
  getPin()->getParent()->removeChild(getPin());
}

void ColorSelector::setColor(const app::Color& color, SetColorOptions options)
{
  m_color = color;

  if (color.getType() == app::Color::IndexType) {
    m_colorPalette.deselect();
    m_colorPalette.selectColor(color.getIndex());
  }

  m_rgbSliders.setColor(m_color);
  m_hsvSliders.setColor(m_color);
  m_graySlider.setColor(m_color);
  if (!m_disableHexUpdate)
    m_hexColorEntry.setColor(m_color);

  if (options == ChangeType)
    selectColorType(m_color.getType());

  int index = get_current_palette()->findExactMatch(
    m_color.getRed(),
    m_color.getGreen(),
    m_color.getBlue());

  m_warningIcon->setVisible(index < 0);
  m_warningIcon->getParent()->layout();
}

app::Color ColorSelector::getColor() const
{
  return m_color;
}

void ColorSelector::onPaletteViewIndexChange(int index, ui::MouseButtons buttons)
{
  setColorWithSignal(app::Color::fromIndex(index));
}

void ColorSelector::onColorSlidersChange(ColorSlidersChangeEvent& ev)
{
  setColorWithSignal(ev.color());
  findBestfitIndex(ev.color());
}

void ColorSelector::onColorHexEntryChange(const app::Color& color)
{
  // Disable updating the hex entry so we don't override what the user
  // is writting in the text field.
  m_disableHexUpdate = true;

  setColorWithSignal(color);
  findBestfitIndex(color);

  m_disableHexUpdate = false;
}

void ColorSelector::onColorTypeClick()
{
  app::Color newColor;

  switch (m_colorType.selectedItem()) {
    case INDEX_MODE:
      newColor = app::Color::fromIndex(getColor().getIndex());
      break;
    case RGB_MODE:
      newColor = app::Color::fromRgb(getColor().getRed(), getColor().getGreen(), getColor().getBlue());
      break;
    case HSB_MODE:
      newColor = app::Color::fromHsv(getColor().getHue(), getColor().getSaturation(), getColor().getValue());
      break;
    case GRAY_MODE:
      newColor = app::Color::fromGray(getColor().getGray());
      break;
    case MASK_MODE:
      newColor = app::Color::fromMask();
      break;
  }

  setColorWithSignal(newColor);
}

void ColorSelector::onFixWarningClick(ui::Event& ev)
{
  try {
    Palette* newPalette = get_current_palette(); // System current pal
    color_t newColor = doc::rgba(
      m_color.getRed(),
      m_color.getGreen(),
      m_color.getBlue(), 255);
    int index = newPalette->findExactMatch(
      m_color.getRed(),
      m_color.getGreen(),
      m_color.getBlue());

    // It should be -1, because the user has pressed the warning
    // button that is available only when the color isn't in the
    // palette.
    ASSERT(index < 0);
    if (index >= 0)
      return;

    int lastUsed = -1;

    ContextWriter writer(UIContext::instance(), 500);
    Document* document(writer.document());
    Sprite* sprite = NULL;
    if (document) {
      sprite = writer.sprite();

      // Find used entries in all stock images. In this way we can start
      // looking for duplicated color entries in the palette from the
      // last used one.
      if (sprite->pixelFormat() == IMAGE_INDEXED) {
        lastUsed = sprite->transparentColor();

        std::vector<Image*> images;
        sprite->getImages(images);
        for (Image* image : images) {
          const LockImageBits<IndexedTraits> bits(image);
          for (LockImageBits<IndexedTraits>::const_iterator it=bits.begin(); it!=bits.end(); ++it) {
            if (lastUsed < *it)
              lastUsed = *it;
          }
        }
      }
    }

    for (int i=lastUsed+1; i<(int)newPalette->size(); ++i) {
      color_t c = newPalette->getEntry(i);
      int altI = newPalette->findExactMatch(
        rgba_getr(c), rgba_getg(c), rgba_getb(c));
      if (altI < i) {
        index = i;
        break;
      }
    }

    if (index < 0) {
      if (newPalette->size() < Palette::MaxColors) {
        newPalette->addEntry(newColor);
        index = newPalette->size()-1;
      }

      if (index < 0) {
        Alert::show(
          "Error<<The palette is full."
          "<<You cannot have more than %d colors.||&OK",
          Palette::MaxColors);
        return;
      }
    }
    else {
      newPalette->setEntry(index, newColor);
    }

    if (document) {
      frame_t frame = writer.frame();

      Transaction transaction(writer.context(), "Add palette entry", ModifyDocument);
      transaction.execute(new cmd::SetPalette(sprite, frame, newPalette));
      transaction.commit();
    }

    set_current_palette(newPalette, false);
    ui::Manager::getDefault()->invalidate();

    m_warningIcon->setVisible(index < 0);
    m_warningIcon->getParent()->layout();
  }
  catch (base::Exception& e) {
    Console::showException(e);
  }
}

void ColorSelector::onPaletteChange()
{
  setColor(getColor(), DoNotChangeType);
  invalidate();
}

void ColorSelector::findBestfitIndex(const app::Color& color)
{
  // Find bestfit palette entry
  int r = color.getRed();
  int g = color.getGreen();
  int b = color.getBlue();

  // Search for the closest color to the RGB values
  int i = get_current_palette()->findBestfit(r, g, b);
  if (i >= 0 && i < 256) {
    m_colorPalette.deselect();
    m_colorPalette.selectColor(i);
  }
}

void ColorSelector::setColorWithSignal(const app::Color& color)
{
  setColor(color, ChangeType);

  // Fire ColorChange signal
  ColorChange(color);
}

void ColorSelector::selectColorType(app::Color::Type type)
{
  m_colorPaletteContainer.setVisible(type == app::Color::IndexType);
  m_rgbSliders.setVisible(type == app::Color::RgbType);
  m_hsvSliders.setVisible(type == app::Color::HsvType);
  m_graySlider.setVisible(type == app::Color::GrayType);
  m_maskLabel.setVisible(type == app::Color::MaskType);

  switch (type) {
    case app::Color::IndexType: m_colorType.setSelectedItem(INDEX_MODE); break;
    case app::Color::RgbType:   m_colorType.setSelectedItem(RGB_MODE); break;
    case app::Color::HsvType:   m_colorType.setSelectedItem(HSB_MODE); break;
    case app::Color::GrayType:  m_colorType.setSelectedItem(GRAY_MODE); break;
    case app::Color::MaskType:  m_colorType.setSelectedItem(MASK_MODE); break;
  }

  m_vbox.layout();
  m_vbox.invalidate();
}

} // namespace app
