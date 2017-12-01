// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/context_access.h"
#include "app/ui/timeline/timeline.h"
#include "ui/base.h"

namespace app {

class MoveCelCommand : public Command {
public:
  MoveCelCommand();
  Command* clone() const override { return new MoveCelCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

MoveCelCommand::MoveCelCommand()
  : Command("MoveCel", CmdUIOnlyFlag)
{
}

bool MoveCelCommand::onEnabled(Context* context)
{
  return App::instance()->timeline()->isMovingCel();
}

void MoveCelCommand::onExecute(Context* context)
{
  App::instance()->timeline()->dropRange(Timeline::kMove);
}

Command* CommandFactory::createMoveCelCommand()
{
  return new MoveCelCommand;
}

} // namespace app
