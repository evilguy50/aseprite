// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifndef APP_UI_WORKSPACE_PANEL_H_INCLUDED
#define APP_UI_WORKSPACE_PANEL_H_INCLUDED
#pragma once

#include "app/ui/animated_widget.h"
#include "app/ui/workspace_views.h"
#include "base/signal.h"
#include "ui/widget.h"

#include <map>
#include <vector>

namespace app {
  class Workspace;
  class WorkspaceTabs;

  class WorkspacePanel : public ui::Widget
                       , public AnimatedWidget {
    enum Ani : int {
      ANI_NONE,
      ANI_DROPAREA,
    };

  public:
    typedef WorkspaceViews::iterator iterator;

    enum PanelType {
      MAIN_PANEL,
      SUB_PANEL,
    };

    static ui::WidgetType Type();

    WorkspacePanel(PanelType panelType);
    ~WorkspacePanel();

    void setTabsBar(WorkspaceTabs* tabs);

    iterator begin() { return m_views.begin(); }
    iterator end() { return m_views.end(); }

    bool isEmpty() const { return m_views.empty(); }

    void addView(WorkspaceView* view, int pos = -1);
    void removeView(WorkspaceView* view);

    WorkspaceView* activeView();
    void setActiveView(WorkspaceView* view);

    // Drop views into workspace
    void setDropViewPreview(const gfx::Point& pos, WorkspaceView* view);
    void removeDropViewPreview();

    // Returns true if the view was docked inside the panel.
    bool dropViewAt(const gfx::Point& pos, WorkspacePanel* from, WorkspaceView* view);

  protected:
    void onPaint(ui::PaintEvent& ev) override;
    void onResize(ui::ResizeEvent& ev) override;
    void onAnimationFrame() override;
    void onAnimationStop(int animation) override;

  private:
    int calculateDropArea(const gfx::Point& pos) const;
    int getDropThreshold() const;
    void adjustTime(int& time, int flag);
    void adjustActiveViewBounds();
    Workspace* getWorkspace();

    PanelType m_panelType;
    WorkspaceTabs* m_tabs;
    WorkspaceViews m_views;
    WorkspaceView* m_activeView;
    int m_dropArea;
    int m_leftTime, m_rightTime;
    int m_topTime, m_bottomTime;
  };

  typedef std::vector<WorkspacePanel*> WorkspacePanels;

} // namespace app

#endif
