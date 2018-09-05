// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef ENABLE_SCRIPTING
  #error ENABLE_SCRIPTING must be defined
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/commands/params.h"
#include "app/console.h"
#include "app/resource_finder.h"
#include "app/script/engine.h"
#include "base/fs.h"
#include "fmt/format.h"
#include "ui/manager.h"

#include <cstdio>

namespace app {

class ConsoleEngineDelegate : public script::EngineDelegate {
public:
  void onConsolePrint(const char* text) override {
    m_console.printf("%s\n", text);
  }

private:
  Console m_console;
};

class RunScriptCommand : public Command {
public:
  RunScriptCommand();
  Command* clone() const override { return new RunScriptCommand(*this); }

protected:
  void onLoadParams(const Params& params) override;
  void onExecute(Context* context) override;
  std::string onGetFriendlyName() const override;

private:
  std::string m_filename;
};

RunScriptCommand::RunScriptCommand()
  : Command(CommandId::RunScript(), CmdRecordableFlag)
{
}

void RunScriptCommand::onLoadParams(const Params& params)
{
  m_filename = params.get("filename");
  if (base::get_file_path(m_filename).empty()) {
    ResourceFinder rf;
    rf.includeDataDir(base::join_path("scripts", m_filename).c_str());
    if (rf.findFirst())
      m_filename = rf.filename();
  }
}

void RunScriptCommand::onExecute(Context* context)
{
  script::Engine* engine = App::instance()->scriptEngine();

  ConsoleEngineDelegate delegate;
  engine->setDelegate(&delegate);
  engine->evalFile(m_filename);
  engine->setDelegate(nullptr);

  ui::Manager::getDefault()->invalidate();
}

std::string RunScriptCommand::onGetFriendlyName() const
{
  if (m_filename.empty())
    return getBaseFriendlyName();
  else
    return fmt::format("{0}: {1}",
                       getBaseFriendlyName(),
                       base::get_file_name(m_filename));
}

Command* CommandFactory::createRunScriptCommand()
{
  return new RunScriptCommand;
}

} // namespace app
