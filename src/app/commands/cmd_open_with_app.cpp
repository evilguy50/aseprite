// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/command.h"
#include "app/context.h"
#include "app/context_access.h"
#include "app/document.h"
#include "app/launcher.h"

namespace app {

class OpenWithAppCommand : public Command {
public:
  OpenWithAppCommand();
  Command* clone() const override { return new OpenWithAppCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

OpenWithAppCommand::OpenWithAppCommand()
  : Command("OpenWithApp",
            "Open With Associated Application",
            CmdUIOnlyFlag)
{
}

bool OpenWithAppCommand::onEnabled(Context* context)
{
  const ContextReader reader(context);
  return
    reader.document() &&
    reader.document()->isAssociatedToFile();
}

void OpenWithAppCommand::onExecute(Context* context)
{
  launcher::open_file(context->activeDocument()->filename());
}

Command* CommandFactory::createOpenWithAppCommand()
{
  return new OpenWithAppCommand;
}

} // namespace app
