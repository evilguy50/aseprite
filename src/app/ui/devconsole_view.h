// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifndef APP_UI_DEVCONSOLE_VIEW_H_INCLUDED
#define APP_UI_DEVCONSOLE_VIEW_H_INCLUDED
#pragma once

#include "app/ui/tabs.h"
#include "app/ui/workspace_view.h"
#include "ui/box.h"
#include "ui/label.h"
#include "ui/textbox.h"
#include "ui/view.h"

namespace app {
  class DevConsoleView : public ui::Box
                       , public TabView
                       , public WorkspaceView {
  public:
    DevConsoleView();
    ~DevConsoleView();

    // TabView implementation
    std::string getTabText() override;
    TabIcon getTabIcon() override;

    // WorkspaceView implementation
    ui::Widget* getContentWidget() override { return this; }
    WorkspaceView* cloneWorkspaceView() override;
    void onWorkspaceViewSelected() override;
    void onClonedFrom(WorkspaceView* from) override;
    bool onCloseView(Workspace* workspace) override;
    void onTabPopup(Workspace* workspace) override;

  protected:
    bool onProcessMessage(ui::Message* msg) override;
    void onExecuteCommand(const std::string& cmd);

  private:
    class CommmandEntry;

    ui::View m_view;
    ui::TextBox m_textBox;
    ui::HBox m_bottomBox;
    ui::Label m_label;
    CommmandEntry* m_entry;
  };

} // namespace app

#endif
