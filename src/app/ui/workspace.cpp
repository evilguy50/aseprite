// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/workspace.h"

#include "app/app.h"
#include "app/ui/main_window.h"
#include "app/ui/tabs.h"
#include "app/ui/workspace_part.h"
#include "app/ui/workspace_view.h"
#include "app/ui/skin/skin_theme.h"
#include "ui/splitter.h"

#include <algorithm>
#include <queue>

namespace app {

using namespace app::skin;
using namespace ui;

Workspace::Workspace()
  : Box(JI_VERTICAL)
  , m_activePart(new WorkspacePart)
{
  SkinTheme* theme = static_cast<SkinTheme*>(getTheme());
  setBgColor(theme->colors.workspace());

  addChild(m_activePart);
}

Workspace::~Workspace()
{
  // No views at this point.
  ASSERT(m_views.empty());
}

void Workspace::addView(WorkspaceView* view)
{
  m_views.push_back(view);

  m_activePart->addView(view);

  App::instance()->getMainWindow()->getTabsBar()->addTab(dynamic_cast<TabView*>(view));

  ActiveViewChanged();          // Fire ActiveViewChanged event
}

void Workspace::removeView(WorkspaceView* view)
{
  WorkspaceViews::iterator it = std::find(m_views.begin(), m_views.end(), view);
  ASSERT(it != m_views.end());
  m_views.erase(it);

  WorkspacePart* part = getPartByView(view);
  ASSERT(part != NULL);

  part->removeView(view);
  if (part->getViewCount() == 0 &&
      part->getParent() != this) {
    bool activePartRemoved = (m_activePart == part);
    WorkspacePart* otherPart = destroyPart(part);

    if (activePartRemoved)
      m_activePart = otherPart;
  }

  App::instance()->getMainWindow()->getTabsBar()->removeTab(dynamic_cast<TabView*>(view));

  ActiveViewChanged();          // Fire ActiveViewChanged event
}

WorkspaceView* Workspace::activeView()
{
  ASSERT(m_activePart != NULL);
  return m_activePart->activeView();
}

void Workspace::setActiveView(WorkspaceView* view)
{
  ASSERT(view != NULL);

  WorkspacePart* viewPart =
    static_cast<WorkspacePart*>(view->getContentWidget()->getParent());

  viewPart->setActiveView(view);

  m_activePart = viewPart;
  ActiveViewChanged();          // Fire ActiveViewChanged event
}

void Workspace::splitView(WorkspaceView* view, int orientation)
{
  // Try to clone the workspace view.
  WorkspaceView* newView = view->cloneWorkspaceView();
  if (newView == NULL)
    return;

  // Get the part where the view-to-clone is located because we need
  // to split this part.
  WorkspacePart* viewPart =
    static_cast<WorkspacePart*>(view->getContentWidget()->getParent());

  // Create a new splitter to add new WorkspacePart on it: the given
  // "viewPart" and a new part named "newPart".
  Splitter* splitter = new Splitter(Splitter::ByPercentage, orientation);
  splitter->setExpansive(true);

  // Create the new part to contain the cloned view (see below, "newView").
  WorkspacePart* newPart = new WorkspacePart();

  // Replace the "viewPart" with the "splitter".
  Widget* parent = viewPart->getParent();
  parent->replaceChild(viewPart, splitter);
  splitter->addChild(viewPart);
  splitter->addChild(newPart);

  // The new part is the active one.
  m_activePart = newPart;

  // Add the cloned view to the active part (newPart)
  // using Workspace::addView().
  addView(newView);
  setActiveView(newView);

  layout();

  newView->onClonedFrom(view);

  ActiveViewChanged();          // Fire ActiveViewChanged event
}

WorkspacePart* Workspace::destroyPart(WorkspacePart* part)
{
  ASSERT(part != NULL);
  ASSERT(part->getViewCount() == 0);

  Widget* splitter = part->getParent();
  ASSERT(splitter != this);
  ASSERT(splitter->getChildren().size() == 2);
  splitter->removeChild(part);
  delete part;
  ASSERT(splitter->getChildren().size() == 1);

  Widget* otherWidget = splitter->getFirstChild();
  WorkspacePart* otherPart = dynamic_cast<WorkspacePart*>(otherWidget);
  if (otherPart == NULL) {
    Widget* widget = otherWidget;
    for (;;) {
      otherPart = widget->findFirstChildByType<WorkspacePart>();
      if (otherPart != NULL)
        break;

      widget = widget->getFirstChild();
    }
  }
  ASSERT(otherPart != NULL);

  splitter->removeChild(otherWidget);
  splitter->getParent()->replaceChild(splitter, otherWidget);
  delete splitter;

  layout();

  return otherPart;
}

void Workspace::makeUnique(WorkspaceView* view)
{
  WorkspaceParts parts;
  enumAllParts(parts);

  for (WorkspaceParts::iterator it=parts.begin(), end=parts.end(); it != end; ++it) {
    WorkspacePart* part = *it;
    if (part->getParent() != this) {
      while (part->activeView())
        part->removeView(part->activeView());
    }
  }

  for (WorkspaceParts::iterator it=parts.begin(), end=parts.end(); it != end; ++it) {
    WorkspacePart* part = *it;
    if (part->getParent() != this)
      destroyPart(part);
  }

  WorkspacePart* uniquePart = dynamic_cast<WorkspacePart*>(getFirstChild());
  ASSERT(uniquePart != NULL);
  m_activePart = uniquePart;

  for (WorkspaceViews::iterator it=m_views.begin(), end=m_views.end(); it != end; ++it) {
    WorkspaceView* v = *it;
    if (!v->getContentWidget()->getParent())
      uniquePart->addView(v);
  }

  setActiveView(view);
}

WorkspacePart* Workspace::getPartByView(WorkspaceView* view)
{
  WorkspaceParts parts;
  enumAllParts(parts);

  for (WorkspaceParts::iterator it=parts.begin(), end=parts.end(); it != end; ++it) {
    WorkspacePart* part = *it;
    if (part->hasView(view))
      return part;
  }

  return NULL;
}

void Workspace::enumAllParts(WorkspaceParts& parts)
{
  std::queue<Widget*> remaining;
  remaining.push(getFirstChild());
  while (!remaining.empty()) {
    Widget* widget = remaining.front();
    remaining.pop();

    ASSERT(widget != NULL);

    WorkspacePart* part = dynamic_cast<WorkspacePart*>(widget);
    if (part) {
      parts.push_back(part);
    }
    else {
      UI_FOREACH_WIDGET(widget->getChildren(), it) {
        remaining.push(*it);
      }
    }
  }
}

} // namespace app
