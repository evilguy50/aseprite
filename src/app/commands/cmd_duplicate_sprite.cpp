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
#include "app/context_access.h"
#include "app/ini_file.h"
#include "app/modules/editors.h"
#include "app/ui_context.h"
#include "base/path.h"
#include "doc/sprite.h"
#include "ui/ui.h"

#include "duplicate_sprite.xml.h"

#include <cstdio>

namespace app {

using namespace ui;

class DuplicateSpriteCommand : public Command {
public:
  DuplicateSpriteCommand();
  Command* clone() const override { return new DuplicateSpriteCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

DuplicateSpriteCommand::DuplicateSpriteCommand()
  : Command("DuplicateSprite",
            "Duplicate Sprite",
            CmdUIOnlyFlag)
{
}

bool DuplicateSpriteCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsReadable);
}

void DuplicateSpriteCommand::onExecute(Context* context)
{
  const ContextReader reader(context);
  const Document* document = reader.document();

  // Load the window widget
  app::gen::DuplicateSprite window;
  std::string fn = document->filename();
  std::string ext = base::get_file_extension(fn);
  window.srcName()->setText(base::get_file_name(fn));
  window.dstName()->setText(base::get_file_title(fn) +
    " Copy" + (!ext.empty() ? "." + ext: ""));

  if (get_config_bool("DuplicateSprite", "Flatten", false))
    window.flatten()->setSelected(true);

  // Open the window
  window.openWindowInForeground();

  if (window.getKiller() == window.ok()) {
    set_config_bool("DuplicateSprite", "Flatten", window.flatten()->isSelected());

    // Make a copy of the document
    Document* docCopy;
    if (window.flatten()->isSelected())
      docCopy = document->duplicate(DuplicateWithFlattenLayers);
    else
      docCopy = document->duplicate(DuplicateExactCopy);

    docCopy->setFilename(window.dstName()->getText().c_str());
    docCopy->setContext(context);
  }
}

Command* CommandFactory::createDuplicateSpriteCommand()
{
  return new DuplicateSpriteCommand;
}

} // namespace app
