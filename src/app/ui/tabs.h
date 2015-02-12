// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifndef APP_UI_TABS_H_INCLUDED
#define APP_UI_TABS_H_INCLUDED
#pragma once

#include "ui/mouse_buttons.h"
#include "ui/timer.h"
#include "ui/widget.h"

#include <vector>

namespace ui {
  class Graphics;
}

namespace app {
  class Tabs;

  // Required interface to be implemented by each new tab that is added
  // in the Tabs widget.
  class TabView {
  public:
    virtual ~TabView() { }

    // Returns the text to be shown in the tab.
    virtual std::string getTabText() = 0;
  };

  // Interface used to control notifications from the Tabs widget.
  class TabsDelegate {
  public:
    virtual ~TabsDelegate() { }

    // Called when the user presses a mouse button over a tab.
    virtual void clickTab(Tabs* tabs, TabView* tabView, ui::MouseButtons buttons) = 0;

    // Called when the mouse is over a tab (the data can be null if the
    // mouse just leave all tabs)
    virtual void mouseOverTab(Tabs* tabs, TabView* tabView) = 0;
  };

  // Tabs control. Used to show opened documents.
  class Tabs : public ui::Widget {
    struct Tab {
      TabView* view;
      std::string text;
      int width;

      Tab(TabView* view) : view(view), width(0) {
      }
    };

    typedef std::vector<Tab*> TabsList;
    typedef TabsList::iterator TabsListIterator;

    enum Ani { ANI_NONE,
               ANI_ADDING_TAB,
               ANI_REMOVING_TAB,
               ANI_SCROLL,
               ANI_SMOOTH_SCROLL };

  public:
    Tabs(TabsDelegate* delegate);
    ~Tabs();

    void addTab(TabView* tabView);
    void removeTab(TabView* tabView);
    void updateTabsText();

    void selectTab(TabView* tabView);
    void selectNextTab();
    void selectPreviousTab();
    TabView* getSelectedTab();

    void startScrolling();
    void stopScrolling();

  protected:
    bool onProcessMessage(ui::Message* msg) override;
    void onPaint(ui::PaintEvent& ev) override;
    void onResize(ui::ResizeEvent& ev) override;
    void onPreferredSize(ui::PreferredSizeEvent& ev) override;
    void onInitTheme(ui::InitThemeEvent& ev) override;
    void onSetText() override;

  private:
    void startAni(Ani ani);
    void stopAni();

    void selectTabInternal(Tab* tab);
    void drawTab(ui::Graphics* g, const gfx::Rect& box, Tab* tab, int y_delta, bool selected);
    TabsListIterator getTabIteratorByView(TabView* tabView);
    Tab* getTabByView(TabView* tabView);
    int getMaxScrollX();
    void makeTabVisible(Tab* tab);
    void setScrollX(int scroll_x);
    void calculateHot();
    void calcTabWidth(Tab* tab);

    TabsList m_list_of_tabs;
    Tab* m_hot;
    Tab* m_selected;
    int m_scrollX;

    // Delegate of notifications
    TabsDelegate* m_delegate;

    // Variables for animation purposes
    ui::Timer m_timer;
    int m_begScrollX;             // Initial X position of scroll in the animation when you scroll with mouse wheel
    int m_endScrollX;             // Final X position of scroll in the animation when you scroll with mouse wheel
    Ani m_ani;                    // Current animation
    int m_ani_t;                  // Number of ticks from the beginning of the animation
    Tab* m_removedTab;
    Tab* m_nextTabOfTheRemovedOne;

    // Buttons to scroll tabs (useful when there are more tabs than visible area)
    class ScrollButton;
    ScrollButton* m_button_left;
    ScrollButton* m_button_right;
  };

} // namespace app

#endif
