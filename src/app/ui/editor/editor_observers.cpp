// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/editor/editor_observers.h"

#include "app/ui/editor/editor_observer.h"
#include "base/bind.h"

namespace app {

void EditorObservers::notifyDestroyEditor(Editor* editor)
{
  notifyObservers(&EditorObserver::onDestroyEditor, editor);
}

void EditorObservers::notifyStateChanged(Editor* editor)
{
  notifyObservers(&EditorObserver::onStateChanged, editor);
}

void EditorObservers::notifyScrollChanged(Editor* editor)
{
  notifyObservers(&EditorObserver::onScrollChanged, editor);
}

void EditorObservers::notifyZoomChanged(Editor* editor)
{
  notifyObservers(&EditorObserver::onZoomChanged, editor);
}

void EditorObservers::notifyBeforeFrameChanged(Editor* editor)
{
  notifyObservers(&EditorObserver::onBeforeFrameChanged, editor);
}

void EditorObservers::notifyAfterFrameChanged(Editor* editor)
{
  notifyObservers(&EditorObserver::onAfterFrameChanged, editor);
}

void EditorObservers::notifyBeforeLayerChanged(Editor* editor)
{
  notifyObservers(&EditorObserver::onBeforeLayerChanged, editor);
}

void EditorObservers::notifyAfterLayerChanged(Editor* editor)
{
  notifyObservers(&EditorObserver::onAfterLayerChanged, editor);
}

} // namespace app
