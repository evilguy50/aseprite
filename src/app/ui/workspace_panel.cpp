// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/workspace_panel.h"

#include "app/ui/skin/skin_theme.h"
#include "app/ui/workspace.h"
#include "app/ui/workspace_tabs.h"
#include "app/ui/workspace_view.h"
#include "base/remove_from_container.h"
#include "ui/box.h"
#include "ui/paint_event.h"
#include "ui/resize_event.h"
#include "ui/splitter.h"

namespace app {

#define ANI_DROPAREA_TICKS  4

using namespace app::skin;
using namespace ui;

// static
WidgetType WorkspacePanel::Type()
{
  static WidgetType type = kGenericWidget;
  if (type == kGenericWidget)
    type = register_widget_type();
  return type;
}

WorkspacePanel::WorkspacePanel(PanelType panelType)
  : Widget(WorkspacePanel::Type())
  , m_panelType(panelType)
  , m_tabs(nullptr)
  , m_activeView(nullptr)
  , m_dropArea(0)
  , m_leftTime(0)
  , m_rightTime(0)
  , m_topTime(0)
  , m_bottomTime(0)
{
  SkinTheme* theme = static_cast<SkinTheme*>(getTheme());
  setBgColor(theme->colors.workspace());
}

WorkspacePanel::~WorkspacePanel()
{
  // No views at this point.
  ASSERT(m_views.empty());
}

void WorkspacePanel::setTabsBar(WorkspaceTabs* tabs)
{
  m_tabs = tabs;
  m_tabs->setPanel(this);
}

void WorkspacePanel::addView(WorkspaceView* view, int pos)
{
  if (pos < 0)
    m_views.push_back(view);
  else
    m_views.insert(m_views.begin()+pos, view);

  if (m_tabs)
    m_tabs->addTab(dynamic_cast<TabView*>(view), pos);

  // Insert the view content as a hidden widget.
  Widget* content = view->getContentWidget();
  content->setVisible(false);
  addChild(content);

  setActiveView(view);
}

void WorkspacePanel::removeView(WorkspaceView* view)
{
  base::remove_from_container(m_views, view);

  Widget* content = view->getContentWidget();
  ASSERT(hasChild(content));
  removeChild(content);

  // Remove related tab.
  if (m_tabs) {
    m_tabs->removeTab(dynamic_cast<TabView*>(view), true);

    // The selected
    TabView* tabView = m_tabs->getSelectedTab();
    view = dynamic_cast<WorkspaceView*>(tabView);
  }
  else
    view = nullptr;

  setActiveView(view);
  if (!view)
    getWorkspace()->setMainPanelAsActive();

  // Destroy this panel
  if (m_views.empty() && m_panelType == SUB_PANEL) {
    Widget* self = getParent();
    ASSERT(self->getType() == kBoxWidget);

    Widget* splitter = self->getParent();
    ASSERT(splitter->getType() == kSplitterWidget);

    Widget* parent = splitter->getParent();

    Widget* side =
      (splitter->getFirstChild() == self ?
        splitter->getLastChild():
        splitter->getFirstChild());

    splitter->removeChild(side);
    parent->replaceChild(splitter, side);

    self->deferDelete();

    parent->layout();
  }
}

WorkspaceView* WorkspacePanel::activeView()
{
  return m_activeView;
}

void WorkspacePanel::setActiveView(WorkspaceView* view)
{
  m_activeView = view;

  for (auto v : m_views)
    v->getContentWidget()->setVisible(v == view);

  if (m_tabs && view)
    m_tabs->selectTab(dynamic_cast<TabView*>(view));

  adjustActiveViewBounds();
}

void WorkspacePanel::onPaint(PaintEvent& ev)
{
  ev.getGraphics()->fillRect(getBgColor(), getClientBounds());
}

void WorkspacePanel::onResize(ui::ResizeEvent& ev)
{
  setBoundsQuietly(ev.getBounds());
  adjustActiveViewBounds();
}

void WorkspacePanel::adjustActiveViewBounds()
{
  gfx::Rect rc = getChildrenBounds();

  // Preview to drop tabs in workspace
  if (m_leftTime+m_topTime+m_rightTime+m_bottomTime > 1e-4) {
    double left = double(m_leftTime) / double(ANI_DROPAREA_TICKS);
    double top = double(m_topTime) / double(ANI_DROPAREA_TICKS);
    double right = double(m_rightTime) / double(ANI_DROPAREA_TICKS);
    double bottom = double(m_bottomTime) / double(ANI_DROPAREA_TICKS);
    double threshold = getDropThreshold();

    rc.x += int(inbetween(0.0, threshold, left));
    rc.y += int(inbetween(0.0, threshold, top));
    rc.w -= int(inbetween(0.0, threshold, left) + inbetween(0.0, threshold, right));
    rc.h -= int(inbetween(0.0, threshold, top) + inbetween(0.0, threshold, bottom));
  }

  for (Widget* child : getChildren())
    if (child->isVisible())
      child->setBounds(rc);
}

void WorkspacePanel::setDropViewPreview(const gfx::Point& pos, WorkspaceView* view)
{
  int newDropArea = calculateDropArea(pos);
  if (newDropArea != m_dropArea) {
    m_dropArea = newDropArea;
    startAnimation(ANI_DROPAREA, ANI_DROPAREA_TICKS);
  }
}

void WorkspacePanel::removeDropViewPreview()
{
  if (m_dropArea) {
    m_dropArea = 0;
    startAnimation(ANI_DROPAREA, ANI_DROPAREA_TICKS);
  }
}

void WorkspacePanel::onAnimationStop(int animation)
{
  if (animation == ANI_DROPAREA)
    layout();
}

void WorkspacePanel::onAnimationFrame()
{
  if (animation() == ANI_DROPAREA) {
    adjustTime(m_leftTime, JI_LEFT);
    adjustTime(m_topTime, JI_TOP);
    adjustTime(m_rightTime, JI_RIGHT);
    adjustTime(m_bottomTime, JI_BOTTOM);
    layout();
  }
}

void WorkspacePanel::adjustTime(int& time, int flag)
{
  if (m_dropArea & flag) {
    if (time < ANI_DROPAREA_TICKS)
      ++time;
  }
  else if (time > 0)
    --time;
}

bool WorkspacePanel::dropViewAt(const gfx::Point& pos, WorkspacePanel* from, WorkspaceView* view)
{
  int dropArea = calculateDropArea(pos);
  if (!dropArea)
    return false;

  // If we're dropping the view in the same panel, and it's the only
  // view in the panel: We cannot drop the view in the panel (because
  // if we remove the last view, the panel will be destroyed).
  if (from == this && m_views.size() == 1)
    return false;

  int splitterAlign = 0;
  if (dropArea & (JI_LEFT | JI_RIGHT)) splitterAlign = JI_HORIZONTAL;
  else if (dropArea & (JI_TOP | JI_BOTTOM)) splitterAlign = JI_VERTICAL;

  ASSERT(from);
  from->removeView(view);

  WorkspaceTabs* newTabs = new WorkspaceTabs(m_tabs->getDelegate());
  WorkspacePanel* newPanel = new WorkspacePanel(SUB_PANEL);
  newTabs->setDockedStyle();
  newPanel->setTabsBar(newTabs);
  newPanel->setExpansive(true);

  Widget* self = this;
  VBox* side = new VBox;
  side->noBorderNoChildSpacing();
  side->addChild(newTabs);
  side->addChild(newPanel);

  Splitter* splitter = new Splitter(Splitter::ByPercentage, splitterAlign);
  splitter->setExpansive(true);

  Widget* parent = getParent();
  if (parent->getType() == kBoxWidget) {
    self = parent;
    parent = self->getParent();
    ASSERT(parent->getType() == kSplitterWidget);
  }
  if (parent->getType() == Workspace::Type() ||
      parent->getType() == kSplitterWidget) {
    parent->replaceChild(self, splitter);
  }
  else {
    ASSERT(false);
  }

  double sideSpace;
  if (m_panelType == MAIN_PANEL)
    sideSpace = 30;
  else
    sideSpace = 50;

  switch (dropArea) {
    case JI_LEFT:
    case JI_TOP:
      splitter->setPosition(sideSpace);
      splitter->addChild(side);
      splitter->addChild(self);
      break;
    case JI_RIGHT:
    case JI_BOTTOM:
      splitter->setPosition(100-sideSpace);
      splitter->addChild(self);
      splitter->addChild(side);
      break;
  }

  newPanel->addView(view);
  parent->layout();
  return true;
}

int WorkspacePanel::calculateDropArea(const gfx::Point& pos) const
{
  gfx::Rect rc = getChildrenBounds();
  if (rc.contains(pos)) {
    int left = ABS(rc.x - pos.x);
    int top = ABS(rc.y - pos.y);
    int right = ABS(rc.x + rc.w - pos.x);
    int bottom = ABS(rc.y + rc.h - pos.y);
    int threshold = getDropThreshold();

    if (left < threshold && left < right && left < top && left < bottom) {
      return JI_LEFT;
    }
    else if (top < threshold && top < left && top < right && top < bottom) {
      return JI_TOP;
    }
    else if (right < threshold && right < left && right < top && right < bottom) {
      return JI_RIGHT;
    }
    else if (bottom < threshold && bottom < left && bottom < top && bottom < right) {
      return JI_BOTTOM;
    }
  }

  return 0;
}

int WorkspacePanel::getDropThreshold() const
{
  gfx::Rect cpos = getChildrenBounds();
  int threshold = 32*guiscale();
  if (threshold > cpos.w/2) threshold = cpos.w/2;
  if (threshold > cpos.h/2) threshold = cpos.h/2;
  return threshold;
}

Workspace* WorkspacePanel::getWorkspace()
{
  Widget* widget = this;
  while (widget) {
    if (widget->getType() == Workspace::Type())
      return static_cast<Workspace*>(widget);

    widget = widget->getParent();
  }
  return nullptr;
}

} // namespace app
