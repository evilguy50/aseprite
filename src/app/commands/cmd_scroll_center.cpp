// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/context_access.h"
#include "app/modules/editors.h"
#include "app/ui/editor/editor.h"

namespace app {

class ScrollCenterCommand : public Command {
public:
  ScrollCenterCommand();
  Command* clone() const override { return new ScrollCenterCommand(*this); }

protected:
  void onExecute(Context* context) override;
};

ScrollCenterCommand::ScrollCenterCommand()
  : Command("ScrollCenter",
            "Scroll to center of canvas",
            CmdUIOnlyFlag)
{
}

void ScrollCenterCommand::onExecute(Context* context)
{
  current_editor->setDefaultScroll();
}

Command* CommandFactory::createScrollCenterCommand()
{
  return new ScrollCenterCommand;
}

} //namespace app
