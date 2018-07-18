// Aseprite
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/app_menus.h"
#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/context.h"
#include "app/file_selector.h"
#include "app/i18n/strings.h"
#include "app/match_words.h"
#include "app/resource_finder.h"
#include "app/tools/tool.h"
#include "app/tools/tool_box.h"
#include "app/ui/app_menuitem.h"
#include "app/ui/keyboard_shortcuts.h"
#include "app/ui/search_entry.h"
#include "app/ui/select_accelerator.h"
#include "app/ui/separator_in_view.h"
#include "app/ui/skin/skin_theme.h"
#include "base/bind.h"
#include "base/fs.h"
#include "base/scoped_value.h"
#include "base/split_string.h"
#include "base/string.h"
#include "fmt/format.h"
#include "ui/alert.h"
#include "ui/graphics.h"
#include "ui/listitem.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/resize_event.h"
#include "ui/separator.h"
#include "ui/size_hint_event.h"
#include "ui/splitter.h"
#include "ui/system.h"

#include "keyboard_shortcuts.xml.h"

#include <set>

#define KEYBOARD_FILENAME_EXTENSION "aseprite-keys"

namespace app {

using namespace skin;
using namespace tools;
using namespace ui;

namespace {

class HeaderSplitter : public Splitter {
public:
  HeaderSplitter() : Splitter(Splitter::ByPixel, HORIZONTAL) {
  }
  void onPositionChange() override {
    Splitter::onPositionChange();

    Widget* p = parent();
    while (p && p->type() != kViewWidget)
      p = p->parent();
    if (p)
      p->layout();
  }
};

class HeaderItem : public ListItem {
public:
  HeaderItem()
    : m_actionLabel("Action")
    , m_keyLabel("Key")
    , m_contextLabel("Context") {
    setBorder(gfx::Border(0));

    auto theme = SkinTheme::instance();
    m_actionLabel.setStyle(theme->styles.listHeaderLabel());
    m_keyLabel.setStyle(theme->styles.listHeaderLabel());
    m_contextLabel.setStyle(theme->styles.listHeaderLabel());

    m_splitter1.setPosition(ui::display_w()*3/4 * 4/10);
    m_splitter2.setPosition(ui::display_w()*3/4 * 2/10);

    addChild(&m_splitter1);
    m_splitter1.addChild(&m_actionLabel);
    m_splitter1.addChild(&m_splitter2);
    m_splitter2.addChild(&m_keyLabel);
    m_splitter2.addChild(&m_contextLabel);
  }

  int keyXPos() const {
    return m_keyLabel.bounds().x - bounds().x;
  }

  int contextXPos() const {
    return m_contextLabel.bounds().x - bounds().x;
  }

private:
  HeaderSplitter m_splitter1, m_splitter2;
  Label m_actionLabel;
  Label m_keyLabel;
  Label m_contextLabel;
};

class KeyItem : public ListItem {

  // Used to avoid deleting the Add/Change/Del buttons on
  // kMouseLeaveMessage when a foreground window is popup on a signal
  // generated by those same buttons.
  struct LockButtons {
    KeyItem* keyItem;
    LockButtons(KeyItem* keyItem) : keyItem(keyItem) {
      keyItem->m_lockButtons = true;
    };
    ~LockButtons() {
      keyItem->m_lockButtons = false;
    };
  };

public:
  KeyItem(const std::string& text,
          const KeyPtr& key,
          AppMenuItem* menuitem,
          const int level,
          HeaderItem* headerItem)
    : ListItem(text)
    , m_key(key)
    , m_keyOrig(key ? new Key(*key): nullptr)
    , m_menuitem(menuitem)
    , m_level(level)
    , m_hotAccel(-1)
    , m_lockButtons(false)
    , m_headerItem(headerItem) {
    gfx::Border border = this->border();
    border.top(0);
    border.bottom(0);
    setBorder(border);
  }

  KeyPtr key() { return m_key; }
  AppMenuItem* menuitem() const { return m_menuitem; }

  void restoreKeys() {
    if (m_key && m_keyOrig)
      *m_key = *m_keyOrig;

    if (m_menuitem && !m_keyOrig)
      m_menuitem->setKey(nullptr);
  }

  std::string searchableText() const {
    if (m_menuitem) {
      Widget* w = m_menuitem;

      // If the menu has a submenu, this item cannot be triggered with a key
      // TODO make this possible: we should be able to open a menu with a key
      if (w->type() == kMenuItemWidget &&
          static_cast<MenuItem*>(w)->getSubmenu())
        return std::string();

      std::string result;
      while (w && w->type() == kMenuItemWidget) {
        if (!result.empty())
          result.insert(0, " > ");
        result.insert(0, w->text());

        w = w->parent();
        if (w && w->type() == kMenuWidget)
          w = static_cast<Menu*>(w)->getOwnerMenuItem();
        else
          w = nullptr;
      }
      return result;
    }
    else {
      return text();
    }
  }

private:

  void onChangeAccel(int index) {
    LockButtons lock(this);
    Accelerator origAccel = m_key->accels()[index];
    SelectAccelerator window(origAccel, m_key->keycontext());
    window.openWindowInForeground();

    if (window.isModified()) {
      m_key->disableAccel(origAccel);
      if (!window.accel().isEmpty())
        m_key->add(window.accel(), KeySource::UserDefined);
    }

    this->window()->layout();
  }

  void onDeleteAccel(int index) {
    LockButtons lock(this);
    // We need to create a copy of the accelerator because
    // Key::disableAccel() will modify the accels() collection itself.
    ui::Accelerator accel = m_key->accels()[index];

    if (ui::Alert::show(
          fmt::format(
            Strings::alerts_delete_shortcut(),
            accel.toString())) != 1)
      return;

    m_key->disableAccel(accel);
    window()->layout();
  }

  void onAddAccel() {
    LockButtons lock(this);
    ui::Accelerator accel;
    SelectAccelerator window(accel, m_key ? m_key->keycontext(): KeyContext::Any);
    window.openWindowInForeground();

    if (window.isModified()) {
      if (!m_key) {
        ASSERT(m_menuitem);
        if (!m_menuitem)
          return;

        m_key = app::KeyboardShortcuts::instance()->command(
          m_menuitem->getCommand()->id().c_str(),
          m_menuitem->getParams());

        m_menuitem->setKey(m_key);
      }

      m_key->add(window.accel(), KeySource::UserDefined);
    }

    this->window()->layout();
  }

  void onSizeHint(SizeHintEvent& ev) override {
    gfx::Size size = textSize();
    size.w = size.w + border().width();
    size.h = size.h + border().height() + 4*guiscale();

    if (m_key && m_key->keycontext() != KeyContext::Any) {
      int w =
        m_headerItem->contextXPos() +
        Graphics::measureUITextLength(
          convertKeyContextToUserFriendlyString(m_key->keycontext()), font());
      size.w = MAX(size.w, w);
    }

    if (m_key && !m_key->accels().empty()) {
      size_t combos = m_key->accels().size();
      if (combos > 1)
        size.h *= combos;
    }

    ev.setSizeHint(size);
  }

  void onPaint(PaintEvent& ev) override {
    Graphics* g = ev.graphics();
    SkinTheme* theme = static_cast<SkinTheme*>(this->theme());
    gfx::Rect bounds = clientBounds();
    gfx::Color fg, bg;

    if (isSelected()) {
      fg = theme->colors.listitemSelectedText();
      bg = theme->colors.listitemSelectedFace();
    }
    else {
      fg = theme->colors.listitemNormalText();
      bg = theme->colors.listitemNormalFace();
    }

    g->fillRect(bg, bounds);

    int y = bounds.y + 2*guiscale();
    const int th = textSize().h;
    // Position of the second and third columns
    const int keyXPos = bounds.x + m_headerItem->keyXPos();
    const int contextXPos = bounds.x + m_headerItem->contextXPos();

    bounds.shrink(border());
    {
      int x = bounds.x + m_level*16 * guiscale();
      IntersectClip clip(g, gfx::Rect(x, y, keyXPos - x, th));
      if (clip)
        g->drawUIText(text(), fg, bg, gfx::Point(x, y), 0);
    }

    if (m_key && !m_key->accels().empty()) {
      if (m_key->keycontext() != KeyContext::Any) {
        g->drawText(
          convertKeyContextToUserFriendlyString(m_key->keycontext()), fg, bg,
          gfx::Point(contextXPos, y));
      }

      const int dh = th + 4*guiscale();
      IntersectClip clip(g, gfx::Rect(keyXPos, y,
                                      contextXPos - keyXPos,
                                      dh * m_key->accels().size()));
      if (clip) {
        int i = 0;
        for (const Accelerator& accel : m_key->accels()) {
          if (i != m_hotAccel || !m_changeButton) {
            g->drawText(
              accel.toString(), fg, bg,
              gfx::Point(keyXPos, y));
          }
          y += dh;
          ++i;
        }
      }
    }
  }

  void onResize(ResizeEvent& ev) override {
    ListItem::onResize(ev);
    destroyButtons();
  }

  bool onProcessMessage(Message* msg) override {
    switch (msg->type()) {

      case kMouseLeaveMessage: {
        destroyButtons();
        invalidate();
        break;
      }

      case kMouseMoveMessage: {
        gfx::Rect bounds = this->bounds();
        MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);

        const Accelerators* accels = (m_key ? &m_key->accels() : NULL);
        int y = bounds.y;
        int dh = textSize().h + 4*guiscale();
        int maxi = (accels && accels->size() > 1 ? accels->size(): 1);

        for (int i=0; i<maxi; ++i, y += dh) {
          int w = Graphics::measureUITextLength(
            (accels && i < (int)accels->size() ? (*accels)[i].toString().c_str(): ""),
            font());
          gfx::Rect itemBounds(bounds.x + m_headerItem->keyXPos(), y, w, dh);
          itemBounds = itemBounds.enlarge(
            gfx::Border(
              4*guiscale(), 0,
              6*guiscale(), 1*guiscale()));

          if (accels &&
              i < (int)accels->size() &&
              mouseMsg->position().y >= itemBounds.y &&
              mouseMsg->position().y < itemBounds.y+itemBounds.h) {
            if (m_hotAccel != i) {
              m_hotAccel = i;

              m_changeConn = obs::connection();
              m_changeButton.reset(new Button(""));
              m_changeConn = m_changeButton->Click.connect(base::Bind<void>(&KeyItem::onChangeAccel, this, i));
              m_changeButton->setStyle(SkinTheme::instance()->styles.miniButton());
              addChild(m_changeButton.get());

              m_deleteConn = obs::connection();
              m_deleteButton.reset(new Button(""));
              m_deleteConn = m_deleteButton->Click.connect(base::Bind<void>(&KeyItem::onDeleteAccel, this, i));
              m_deleteButton->setStyle(SkinTheme::instance()->styles.miniButton());
              addChild(m_deleteButton.get());

              m_changeButton->setBgColor(gfx::ColorNone);
              m_changeButton->setBounds(itemBounds);
              m_changeButton->setText((*accels)[i].toString());

              const char* label = "x";
              m_deleteButton->setBgColor(gfx::ColorNone);
              m_deleteButton->setBounds(
                gfx::Rect(
                  itemBounds.x + itemBounds.w + 2*guiscale(),
                  itemBounds.y,
                  Graphics::measureUITextLength(
                    label, font()) + 4*guiscale(),
                  itemBounds.h));
              m_deleteButton->setText(label);

              invalidate();
            }
          }

          if (i == 0 && !m_addButton &&
              (!m_menuitem || m_menuitem->getCommand())) {
            m_addConn = obs::connection();
            m_addButton.reset(new Button(""));
            m_addConn = m_addButton->Click.connect(base::Bind<void>(&KeyItem::onAddAccel, this));
            m_addButton->setStyle(SkinTheme::instance()->styles.miniButton());
            addChild(m_addButton.get());

            itemBounds.w = 8*guiscale() + Graphics::measureUITextLength("Add", font());
            itemBounds.x -= itemBounds.w + 2*guiscale();

            m_addButton->setBgColor(gfx::ColorNone);
            m_addButton->setBounds(itemBounds);
            m_addButton->setText("Add");

            invalidate();
          }
        }
        break;
      }
    }
    return ListItem::onProcessMessage(msg);
  }

  void destroyButtons() {
    m_changeConn = obs::connection();
    m_deleteConn = obs::connection();
    m_addConn = obs::connection();

    if (!m_lockButtons) {
      m_changeButton.reset();
      m_deleteButton.reset();
      m_addButton.reset();
    }
    // Just hide the buttons
    else {
      if (m_changeButton) m_changeButton->setVisible(false);
      if (m_deleteButton) m_deleteButton->setVisible(false);
      if (m_addButton) m_addButton->setVisible(false);
    }

    m_hotAccel = -1;
  }

  KeyPtr m_key;
  KeyPtr m_keyOrig;
  AppMenuItem* m_menuitem;
  int m_level;
  ui::Accelerators m_newAccels;
  base::SharedPtr<ui::Button> m_changeButton;
  base::SharedPtr<ui::Button> m_deleteButton;
  base::SharedPtr<ui::Button> m_addButton;
  obs::scoped_connection m_changeConn;
  obs::scoped_connection m_deleteConn;
  obs::scoped_connection m_addConn;
  int m_hotAccel;
  bool m_lockButtons;
  HeaderItem* m_headerItem;
};

class KeyboardShortcutsWindow : public app::gen::KeyboardShortcuts {
public:
  KeyboardShortcutsWindow(const std::string& searchText)
    : m_searchChange(false) {
    setAutoRemap(false);

    section()->addChild(new ListItem("Menus"));
    section()->addChild(new ListItem("Commands"));
    section()->addChild(new ListItem("Tools"));
    section()->addChild(new ListItem("Action Modifiers"));

    m_listBoxes.push_back(menus());
    m_listBoxes.push_back(commands());
    m_listBoxes.push_back(tools());
    m_listBoxes.push_back(actions());

    search()->Change.connect(base::Bind<void>(&KeyboardShortcutsWindow::onSearchChange, this));
    section()->Change.connect(base::Bind<void>(&KeyboardShortcutsWindow::onSectionChange, this));
    importButton()->Click.connect(base::Bind<void>(&KeyboardShortcutsWindow::onImport, this));
    exportButton()->Click.connect(base::Bind<void>(&KeyboardShortcutsWindow::onExport, this));
    resetButton()->Click.connect(base::Bind<void>(&KeyboardShortcutsWindow::onReset, this));

    fillAllLists();

    if (!searchText.empty()) {
      search()->setText(searchText);
      onSearchChange();
    }
  }

  ~KeyboardShortcutsWindow() {
    deleteAllKeyItems();
  }

  void restoreKeys() {
    for (KeyItem* keyItem : m_allKeyItems) {
      keyItem->restoreKeys();
    }
  }

private:
  void deleteAllKeyItems() {
    deleteList(searchList());
    deleteList(menus());
    deleteList(commands());
    deleteList(tools());
    deleteList(actions());
    ASSERT(m_allKeyItems.empty());
  }

  void fillAllLists() {
    deleteAllKeyItems();

    // Load keyboard shortcuts
    fillMenusList(menus(), AppMenus::instance()->getRootMenu(), 0);
    fillToolsList(tools(), App::instance()->toolBox());

    for (const KeyPtr& key : *app::KeyboardShortcuts::instance()) {
      if (key->type() == KeyType::Tool ||
          key->type() == KeyType::Quicktool) {
        continue;
      }

      std::string text = key->triggerString();
      switch (key->keycontext()) {
        case KeyContext::SelectionTool:
        case KeyContext::TranslatingSelection:
        case KeyContext::ScalingSelection:
        case KeyContext::RotatingSelection:
        case KeyContext::MoveTool:
        case KeyContext::FreehandTool:
        case KeyContext::ShapeTool:
          text =
            convertKeyContextToUserFriendlyString(key->keycontext())
            + ": " + text;
          break;
      }
      KeyItem* keyItem = new KeyItem(text, key, nullptr, 0, &m_headerItem);

      ListBox* listBox = nullptr;
      switch (key->type()) {
        case KeyType::Command:
          listBox = this->commands();
          break;
        case KeyType::Action:
          listBox = this->actions();
          break;
      }

      ASSERT(listBox);
      if (listBox) {
        m_allKeyItems.push_back(keyItem);
        listBox->addChild(keyItem);
      }
    }

    commands()->sortItems();
    tools()->sortItems();
    actions()->sortItems();

    section()->selectIndex(0);
    updateViews();
  }

  void deleteList(Widget* listbox) {
    if (m_headerItem.parent() == listbox)
      listbox->removeChild(&m_headerItem);

    while (listbox->lastChild()) {
      Widget* item = listbox->lastChild();
      listbox->removeChild(item);

      auto it = std::find(m_allKeyItems.begin(), m_allKeyItems.end(), item);
      if (it != m_allKeyItems.end())
        m_allKeyItems.erase(it);

      delete item;
    }
  }

  void fillSearchList(const std::string& search) {
    deleteList(searchList());

    MatchWords match(search);

    int sectionIdx = 0;         // index 0 is menus
    for (auto listBox : m_listBoxes) {
      Separator* group = nullptr;

      for (auto item : listBox->children()) {
        if (KeyItem* keyItem = dynamic_cast<KeyItem*>(item)) {
          std::string itemText = keyItem->searchableText();
          if (!match(itemText))
            continue;

          if (!group) {
            group = new SeparatorInView(
              section()->children()[sectionIdx]->text(), HORIZONTAL);
            searchList()->addChild(group);
          }

          KeyItem* copyItem =
            new KeyItem(itemText,
                        keyItem->key(),
                        keyItem->menuitem(), 0,
                        &m_headerItem);

          m_allKeyItems.push_back(copyItem);
          searchList()->addChild(copyItem);
        }
      }

      ++sectionIdx;
    }
  }

  void onSearchChange() {
    base::ScopedValue<bool> flag(m_searchChange, true, false);
    std::string searchText = search()->text();

    if (searchText.empty())
      section()->selectIndex(0);
    else {
      fillSearchList(searchText);
      section()->selectChild(nullptr);
    }

    updateViews();
  }

  void onSectionChange() {
    if (m_searchChange)
      return;

    search()->setText("");
    updateViews();
  }

  void updateViews() {
    int s = section()->getSelectedIndex();
    searchView()->setVisible(s < 0);
    menusView()->setVisible(s == 0);
    commandsView()->setVisible(s == 1);
    toolsView()->setVisible(s == 2);
    actionsView()->setVisible(s == 3);

    if (m_headerItem.parent())
      m_headerItem.parent()->removeChild(&m_headerItem);
    if (s < 0)
      searchList()->insertChild(0, &m_headerItem);
    else
      m_listBoxes[s]->insertChild(0, &m_headerItem);

    layout();
  }

  void onImport() {
    base::paths exts = { KEYBOARD_FILENAME_EXTENSION };
    base::paths filename;
    if (!app::show_file_selector(
          "Import Keyboard Shortcuts", "", exts,
          FileSelectorType::Open, filename))
      return;

    ASSERT(!filename.empty());

    app::KeyboardShortcuts::instance()->importFile(
      filename.front(), KeySource::UserDefined);

    fillAllLists();
    layout();
  }

  void onExport() {
    base::paths exts = { KEYBOARD_FILENAME_EXTENSION };
    base::paths filename;

    if (!app::show_file_selector(
          "Export Keyboard Shortcuts", "", exts,
          FileSelectorType::Save, filename))
      return;

    ASSERT(!filename.empty());

    app::KeyboardShortcuts::instance()->exportFile(filename.front());
  }

  void onReset() {
    if (ui::Alert::show(Strings::alerts_restore_all_shortcuts()) == 1) {
      app::KeyboardShortcuts::instance()->reset();
      layout();
    }
  }

  void fillMenusList(ListBox* listbox, Menu* menu, int level) {
    for (auto child : menu->children()) {
      if (AppMenuItem* menuItem = dynamic_cast<AppMenuItem*>(child)) {
        if (menuItem == AppMenus::instance()->getRecentListMenuitem())
          continue;

        KeyItem* keyItem = new KeyItem(
          menuItem->text().c_str(),
          menuItem->key(), menuItem, level,
          &m_headerItem);

        m_allKeyItems.push_back(keyItem);
        listbox->addChild(keyItem);

        if (menuItem->hasSubmenu())
          fillMenusList(listbox, menuItem->getSubmenu(), level+1);
      }
    }
  }

  void fillToolsList(ListBox* listbox, ToolBox* toolbox) {
    for (Tool* tool : *toolbox) {
      std::string text = tool->getText();

      KeyPtr key = app::KeyboardShortcuts::instance()->tool(tool);
      KeyItem* keyItem = new KeyItem(text, key, nullptr, 0,
                                     &m_headerItem);
      m_allKeyItems.push_back(keyItem);
      listbox->addChild(keyItem);

      text += " (quick)";
      key = app::KeyboardShortcuts::instance()->quicktool(tool);
      keyItem = new KeyItem(text, key, nullptr, 0,
                            &m_headerItem);
      m_allKeyItems.push_back(keyItem);
      listbox->addChild(keyItem);
    }
  }

  std::vector<ListBox*> m_listBoxes;
  std::vector<KeyItem*> m_allKeyItems;
  bool m_searchChange;
  HeaderItem m_headerItem;
};

} // anonymous namespace

class KeyboardShortcutsCommand : public Command {
public:
  KeyboardShortcutsCommand();
  Command* clone() const override { return new KeyboardShortcutsCommand(*this); }

protected:
  void onLoadParams(const Params& params) override;
  void onExecute(Context* context) override;

private:
  void addMissingKeyboardShortcutsForCommands();

  std::string m_search;
};

KeyboardShortcutsCommand::KeyboardShortcutsCommand()
  : Command(CommandId::KeyboardShortcuts(), CmdUIOnlyFlag)
{
}

void KeyboardShortcutsCommand::onLoadParams(const Params& params)
{
  m_search = params.get("search");
}

void KeyboardShortcutsCommand::onExecute(Context* context)
{
  addMissingKeyboardShortcutsForCommands();

  // Here we copy the m_search field because
  // KeyboardShortcutsWindow::fillAllLists() modifies this same
  // KeyboardShortcutsCommand instance (so m_search will be "")
  // TODO Seeing this, we need a complete new way to handle UI commands execution
  std::string neededSearchCopy = m_search;
  KeyboardShortcutsWindow window(neededSearchCopy);

  window.setBounds(gfx::Rect(0, 0, ui::display_w()*3/4, ui::display_h()*3/4));

  window.centerWindow();
  window.setVisible(true);
  window.openWindowInForeground();

  if (window.closer() == window.ok()) {
    KeyboardShortcuts::instance()->UserChange();

    // Save keyboard shortcuts in configuration file
    {
      ResourceFinder rf;
      rf.includeUserDir("user." KEYBOARD_FILENAME_EXTENSION);
      std::string fn = rf.getFirstOrCreateDefault();
      KeyboardShortcuts::instance()->exportFile(fn);
    }
  }
  else {
    window.restoreKeys();
  }

  AppMenus::instance()->syncNativeMenuItemKeyShortcuts();
}

void KeyboardShortcutsCommand::addMissingKeyboardShortcutsForCommands()
{
  std::set<std::string> commandsAlreadyAdded;
  auto keys = app::KeyboardShortcuts::instance();
  for (const KeyPtr& key : *keys) {
    if (key->type() != KeyType::Command)
      continue;

    if (key->params().empty())
      commandsAlreadyAdded.insert(key->command()->id());
  }

  std::vector<std::string> ids;
  Commands* commands = Commands::instance();
  commands->getAllIds(ids);

  for (const std::string& id : ids) {
    Command* command = commands->byId(id.c_str());

    // Don't add commands that need params (they will be added to
    // the list using the list of keyboard shortcuts from gui.xml).
    if (command->needsParams())
      continue;

    auto it = commandsAlreadyAdded.find(command->id());
    if (it != commandsAlreadyAdded.end())
      continue;

    // Create the new Key element in KeyboardShortcuts for this
    // command without params.
    keys->command(command->id().c_str());
  }
}

Command* CommandFactory::createKeyboardShortcutsCommand()
{
  return new KeyboardShortcutsCommand;
}

} // namespace app
