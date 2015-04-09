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
#include "app/commands/command.h"
#include "app/context.h"
#include "app/find_widget.h"
#include "app/ini_file.h"
#include "app/launcher.h"
#include "app/load_widget.h"
#include "app/modules/editors.h"
#include "app/modules/gui.h"
#include "app/pref/preferences.h"
#include "app/resource_finder.h"
#include "app/send_crash.h"
#include "app/settings/settings.h"
#include "app/ui/color_button.h"
#include "app/ui/editor/editor.h"
#include "base/bind.h"
#include "base/convert_to.h"
#include "base/path.h"
#include "doc/image.h"
#include "render/render.h"
#include "she/system.h"
#include "ui/ui.h"

#include "generated_options.h"

namespace app {

using namespace ui;

class OptionsWindow : public app::gen::Options {
public:
  OptionsWindow(Context* context, int& curSection)
    : m_settings(context->settings())
    , m_preferences(App::instance()->preferences())
    , m_globPref(m_preferences.document(nullptr))
    , m_docPref(m_preferences.document(context->activeDocument()))
    , m_curPref(&m_docPref)
    , m_checked_bg_color1(new ColorButton(app::Color::fromMask(), IMAGE_RGB))
    , m_checked_bg_color2(new ColorButton(app::Color::fromMask(), IMAGE_RGB))
    , m_pixelGridColor(new ColorButton(app::Color::fromMask(), IMAGE_RGB))
    , m_gridColor(new ColorButton(app::Color::fromMask(), IMAGE_RGB))
    , m_cursorColor(new ColorButton(Editor::get_cursor_color(), IMAGE_RGB))
    , m_curSection(curSection)
  {
    sectionListbox()->ChangeSelectedItem.connect(Bind<void>(&OptionsWindow::onChangeSection, this));
    cursorColorBox()->addChild(m_cursorColor);

    // Grid color
    m_gridColor->setId("grid_color");
    gridColorPlaceholder()->addChild(m_gridColor);

    // Pixel grid color
    m_pixelGridColor->setId("pixel_grid_color");
    pixelGridColorPlaceholder()->addChild(m_pixelGridColor);

    // Others
    if (m_preferences.general.autoshowTimeline())
      autotimeline()->setSelected(true);

    if (m_preferences.general.expandMenubarOnMouseover())
      expandMenubarOnMouseover()->setSelected(true);

    if (m_preferences.general.dataRecovery())
      enableDataRecovery()->setSelected(true);

    dataRecoveryPeriod()->setSelectedItemIndex(
      dataRecoveryPeriod()->findItemIndexByValue(
        base::convert_to<std::string>(m_preferences.general.dataRecoveryPeriod())));

    if (m_settings->getCenterOnZoom())
      centerOnZoom()->setSelected(true);

    if (m_preferences.experimental.useNativeCursor())
      nativeCursor()->setSelected(true);

    if (m_preferences.experimental.useNativeFileDialog())
      nativeFileDialog()->setSelected(true);

    if (m_preferences.experimental.flashLayer())
      flashLayer()->setSelected(true);

    if (m_settings->getShowSpriteEditorScrollbars())
      showScrollbars()->setSelected(true);

    // Scope
    gridScope()->addItem("Global");
    if (context->activeDocument()) {
      gridScope()->addItem("Current Document");
      gridScope()->setSelectedItemIndex(1);
      gridScope()->Change.connect(Bind<void>(&OptionsWindow::onChangeGridScope, this));
    }

    // Checked background size
    screenScale()->addItem("1:1");
    screenScale()->addItem("2:1");
    screenScale()->addItem("3:1");
    screenScale()->addItem("4:1");
    screenScale()->setSelectedItemIndex(get_screen_scaling()-1);

    // Right-click
    rightClickBehavior()->addItem("Paint with background color");
    rightClickBehavior()->addItem("Pick foreground color");
    rightClickBehavior()->addItem("Erase");
    rightClickBehavior()->setSelectedItemIndex((int)m_settings->getRightClickMode());

    // Zoom with Scroll Wheel
    wheelZoom()->setSelected(m_settings->getZoomWithScrollWheel());

    // Checked background size
    checkedBgSize()->addItem("16x16");
    checkedBgSize()->addItem("8x8");
    checkedBgSize()->addItem("4x4");
    checkedBgSize()->addItem("2x2");

    // Checked background colors
    checkedBgColor1Box()->addChild(m_checked_bg_color1);
    checkedBgColor2Box()->addChild(m_checked_bg_color2);

    // Reset button
    reset()->Click.connect(Bind<void>(&OptionsWindow::onReset, this));

    // Links
    locateFile()->Click.connect(Bind<void>(&OptionsWindow::onLocateConfigFile, this));
#if _WIN32
    locateCrashFolder()->Click.connect(Bind<void>(&OptionsWindow::onLocateCrashFolder, this));
#else
    locateCrashFolder()->setVisible(false);
#endif

    // Undo preferences
    undoSizeLimit()->setTextf("%d", m_preferences.undo.sizeLimit());
    undoGotoModified()->setSelected(m_preferences.undo.gotoModified());
    undoAllowNonlinearHistory()->setSelected(m_preferences.undo.allowNonlinearHistory());

    onChangeGridScope();
    sectionListbox()->selectIndex(m_curSection);
  }

  bool ok() {
    return (getKiller() == buttonOk());
  }

  void saveConfig() {
    Editor::set_cursor_color(m_cursorColor->getColor());
    m_preferences.general.autoshowTimeline(autotimeline()->isSelected());

    bool expandOnMouseover = expandMenubarOnMouseover()->isSelected();
    m_preferences.general.expandMenubarOnMouseover(expandOnMouseover);
    ui::MenuBar::setExpandOnMouseover(expandOnMouseover);

    std::string warnings;

    int newPeriod = base::convert_to<int>(dataRecoveryPeriod()->getValue());
    if (enableDataRecovery()->isSelected() != m_preferences.general.dataRecovery() ||
        newPeriod != m_preferences.general.dataRecoveryPeriod()) {
      m_preferences.general.dataRecovery(enableDataRecovery()->isSelected());
      m_preferences.general.dataRecoveryPeriod(newPeriod);

      warnings += "<<- Automatically save recovery data every";
    }

    m_settings->setCenterOnZoom(centerOnZoom()->isSelected());

    m_settings->setShowSpriteEditorScrollbars(showScrollbars()->isSelected());
    m_settings->setZoomWithScrollWheel(wheelZoom()->isSelected());
    m_settings->setRightClickMode(static_cast<RightClickMode>(rightClickBehavior()->getSelectedItemIndex()));

    m_curPref->grid.color(m_gridColor->getColor());
    m_curPref->grid.opacity(gridOpacity()->getValue());
    m_curPref->grid.autoOpacity(gridAutoOpacity()->isSelected());
    m_curPref->pixelGrid.color(m_pixelGridColor->getColor());
    m_curPref->pixelGrid.opacity(pixelGridOpacity()->getValue());
    m_curPref->pixelGrid.autoOpacity(pixelGridAutoOpacity()->isSelected());
    m_curPref->bg.type(app::gen::BgType(checkedBgSize()->getSelectedItemIndex()));
    m_curPref->bg.zoom(checkedBgZoom()->isSelected());
    m_curPref->bg.color1(m_checked_bg_color1->getColor());
    m_curPref->bg.color2(m_checked_bg_color2->getColor());

    int undo_size_limit_value;
    undo_size_limit_value = undoSizeLimit()->getTextInt();
    undo_size_limit_value = MID(1, undo_size_limit_value, 9999);

    m_preferences.undo.sizeLimit(undo_size_limit_value);
    m_preferences.undo.gotoModified(undoGotoModified()->isSelected());
    m_preferences.undo.allowNonlinearHistory(undoAllowNonlinearHistory()->isSelected());

    // Experimental features
    m_preferences.experimental.useNativeCursor(nativeCursor()->isSelected());
    m_preferences.experimental.useNativeFileDialog(nativeFileDialog()->isSelected());
    m_preferences.experimental.flashLayer(flashLayer()->isSelected());
    ui::set_use_native_cursors(
      m_preferences.experimental.useNativeCursor());

    int new_screen_scaling = screenScale()->getSelectedItemIndex()+1;
    if (new_screen_scaling != get_screen_scaling()) {
      set_screen_scaling(new_screen_scaling);
      warnings += "<<- Screen Scale";
    }

    // Save configuration
    flush_config_file();

    if (!warnings.empty()) {
      ui::Alert::show(PACKAGE
        "<<You must restart the program to see your changes to:%s"
        "||&OK", warnings.c_str());
    }
  }

private:
  void onChangeSection() {
    ListItem* item = static_cast<ListItem*>(sectionListbox()->getSelectedChild());
    if (!item)
      return;

    panel()->showChild(findChild(item->getValue().c_str()));
    m_curSection = sectionListbox()->getSelectedIndex();
  }

  void onChangeGridScope() {
    int item = gridScope()->getSelectedItemIndex();

    switch (item) {
      case 0: m_curPref = &m_globPref; break;
      case 1: m_curPref = &m_docPref; break;
    }

    m_gridColor->setColor(m_curPref->grid.color());
    gridOpacity()->setValue(m_curPref->grid.opacity());
    gridAutoOpacity()->setSelected(m_curPref->grid.autoOpacity());

    m_pixelGridColor->setColor(m_curPref->pixelGrid.color());
    pixelGridOpacity()->setValue(m_curPref->pixelGrid.opacity());
    pixelGridAutoOpacity()->setSelected(m_curPref->pixelGrid.autoOpacity());

    checkedBgSize()->setSelectedItemIndex(int(m_curPref->bg.type()));
    checkedBgZoom()->setSelected(m_curPref->bg.zoom());
    m_checked_bg_color1->setColor(m_curPref->bg.color1());
    m_checked_bg_color2->setColor(m_curPref->bg.color2());
  }

  void onReset() {
    // Reset global settings (use default values specified in pref.xml)
    if (m_curPref == &m_globPref) {
      DocumentPreferences& pref = m_globPref;

      m_gridColor->setColor(pref.grid.color.defaultValue());
      gridOpacity()->setValue(pref.grid.opacity.defaultValue());
      gridAutoOpacity()->setSelected(pref.grid.autoOpacity.defaultValue());

      m_pixelGridColor->setColor(pref.pixelGrid.color.defaultValue());
      pixelGridOpacity()->setValue(pref.pixelGrid.opacity.defaultValue());
      pixelGridAutoOpacity()->setSelected(pref.pixelGrid.autoOpacity.defaultValue());

      checkedBgSize()->setSelectedItemIndex(int(pref.bg.type.defaultValue()));
      checkedBgZoom()->setSelected(pref.bg.zoom.defaultValue());
      m_checked_bg_color1->setColor(pref.bg.color1.defaultValue());
      m_checked_bg_color2->setColor(pref.bg.color2.defaultValue());
    }
    // Reset document settings with global settings
    else {
      DocumentPreferences& pref = m_globPref;

      m_gridColor->setColor(pref.grid.color());
      gridOpacity()->setValue(pref.grid.opacity());
      gridAutoOpacity()->setSelected(pref.grid.autoOpacity());

      m_pixelGridColor->setColor(pref.pixelGrid.color());
      pixelGridOpacity()->setValue(pref.pixelGrid.opacity());
      pixelGridAutoOpacity()->setSelected(pref.pixelGrid.autoOpacity());

      checkedBgSize()->setSelectedItemIndex(int(pref.bg.type()));
      checkedBgZoom()->setSelected(pref.bg.zoom());
      m_checked_bg_color1->setColor(pref.bg.color1());
      m_checked_bg_color2->setColor(pref.bg.color2());
    }
  }

  void onLocateCrashFolder() {
    app::launcher::open_folder(base::get_file_path(app::memory_dump_filename()));
  }

  void onLocateConfigFile() {
    app::launcher::open_folder(app::main_config_filename());
  }

  ISettings* m_settings;
  Preferences& m_preferences;
  DocumentPreferences& m_globPref;
  DocumentPreferences& m_docPref;
  DocumentPreferences* m_curPref;
  ColorButton* m_checked_bg_color1;
  ColorButton* m_checked_bg_color2;
  ColorButton* m_pixelGridColor;
  ColorButton* m_gridColor;
  ColorButton* m_cursorColor;
  int& m_curSection;
};

class OptionsCommand : public Command {
public:
  OptionsCommand();
  Command* clone() const override { return new OptionsCommand(*this); }

protected:
  void onExecute(Context* context);
};

OptionsCommand::OptionsCommand()
  : Command("Options",
            "Options",
            CmdUIOnlyFlag)
{
  Preferences& preferences = App::instance()->preferences();

  ui::MenuBar::setExpandOnMouseover(
    preferences.general.expandMenubarOnMouseover());
}

void OptionsCommand::onExecute(Context* context)
{
  static int curSection = 0;

  OptionsWindow window(context, curSection);
  window.openWindowInForeground();
  if (window.ok())
    window.saveConfig();
}

Command* CommandFactory::createOptionsCommand()
{
  return new OptionsCommand;
}

} // namespace app
