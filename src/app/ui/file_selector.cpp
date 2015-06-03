// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/file_selector.h"

#include "app/app.h"
#include "app/console.h"
#include "app/file/file.h"
#include "app/find_widget.h"
#include "app/ini_file.h"
#include "app/load_widget.h"
#include "app/modules/gfx.h"
#include "app/modules/gui.h"
#include "app/recent_files.h"
#include "app/ui/file_list.h"
#include "app/ui/skin/skin_parts.h"
#include "app/widget_loader.h"
#include "base/bind.h"
#include "base/fs.h"
#include "base/path.h"
#include "base/split_string.h"
#include "base/unique_ptr.h"
#include "ui/ui.h"

#include "generated_new_folder_window.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#ifndef MAX_PATH
#  define MAX_PATH 4096         // TODO this is needed for Linux, is it correct?
#endif

namespace app {

using namespace app::skin;
using namespace ui;

template<class Container>
class NullableIterator {
public:
  typedef typename Container::iterator iterator;

  NullableIterator() : m_isNull(true) { }

  void reset() { m_isNull = true; }

  bool isNull() const { return m_isNull; }
  bool isValid() const { return !m_isNull; }

  iterator getIterator() {
    ASSERT(!m_isNull);
    return m_iterator;
  }

  void setIterator(const iterator& it) {
    m_isNull = false;
    m_iterator = it;
  }

private:
  bool m_isNull;
  typename Container::iterator m_iterator;
};

// Variables used only to maintain the history of navigation.
static FileItemList* navigation_history = NULL; // Set of FileItems navigated
static NullableIterator<FileItemList> navigation_position; // Current position in the navigation history

// This map acts like a temporal customization by the user when he/she
// wants to open files.  The key (first) is the real "showExtensions"
// parameter given to the FileSelector::show() function. The value
// (second) is the selected extension by the user. It's used only in
// FileSelector::Open type of dialogs.
static std::map<std::string, std::string> preferred_open_extensions;

// Slot for App::Exit signal
static void on_exit_delete_navigation_history()
{
  delete navigation_history;
}

class CustomFileNameEntry : public ComboBox
{
public:
  CustomFileNameEntry()
    : m_fileList(nullptr) {
    setEditable(true);
    getEntryWidget()->EntryChange.connect(&CustomFileNameEntry::onEntryChange, this);
  }

  void setAssociatedFileList(FileList* fileList) {
    m_fileList = fileList;
  }

protected:

  void onEntryChange() {
    removeAllItems();

    // String to be autocompleted
    std::string left_part = getEntryWidget()->getText();
    closeListBox();

    if (left_part.empty())
      return;

    for (const IFileItem* child : m_fileList->getFileList()) {
      std::string child_name = child->getDisplayName();
      std::string::const_iterator it1, it2;

      for (it1 = child_name.begin(), it2 = left_part.begin();
           it1 != child_name.end() && it2 != left_part.end();
           ++it1, ++it2) {
        if (std::tolower(*it1) != std::tolower(*it2))
          break;
      }

      // Is the pattern (left_part) in the child_name's beginning?
      if (it1 != child_name.end() && it2 == left_part.end())
        addItem(child_name);
    }

    if (getItemCount() > 0)
      openListBox();
  }

private:
  FileList* m_fileList;
};

class CustomFileNameItem : public ListItem
{
public:
  CustomFileNameItem(const char* text, IFileItem* fileItem)
    : ListItem(text)
    , m_fileItem(fileItem)
  {
  }

  IFileItem* getFileItem() { return m_fileItem; }

private:
  IFileItem* m_fileItem;
};

class CustomFolderNameItem : public ListItem
{
public:
  CustomFolderNameItem(const char* text)
    : ListItem(text)
  {
  }
};

// We have this dummy/hidden widget only to handle special navigation
// with arrow keys. In the past this code was in the same FileSelector
// itself, but there were problems adding that window as a message
// filter. Mainly there is a special combination of widgets
// (comboboxes) that need to filter Esc key (e.g. to close the
// combobox popup). And we cannot pre-add a filter that send that key
// to the Manager before it's processed by the combobox filter.
class ArrowNavigator : public Widget {
public:
  ArrowNavigator(FileSelector* filesel)
    : Widget(kGenericWidget)
    , m_filesel(filesel) {
    setVisible(false);
  }

protected:
  bool onProcessMessage(ui::Message* msg) override {
    switch (msg->type()) {
      case kOpenMessage:
        getManager()->addMessageFilter(kKeyDownMessage, this);
        break;
      case kCloseMessage:
        getManager()->removeMessageFilter(kKeyDownMessage, this);
        break;
      case kKeyDownMessage: {
        KeyMessage* keyMsg = static_cast<KeyMessage*>(msg);
        KeyScancode scancode = keyMsg->scancode();
        int unicode = keyMsg->unicodeChar();

#ifdef __APPLE__
        bool up = (msg->cmdPressed() && scancode == kKeyUp);
        bool enter = (msg->cmdPressed() && scancode == kKeyDown);
        bool back = (msg->cmdPressed() && msg->shiftPressed() && unicode == '[');
        bool forward = (msg->cmdPressed() && msg->shiftPressed() && unicode == ']');
#else
        bool up = (msg->altPressed() && scancode == kKeyUp);
        bool enter = (msg->altPressed() && scancode == kKeyDown);
        bool back = (msg->altPressed() && scancode == kKeyLeft);
        bool forward = (msg->altPressed() && scancode == kKeyRight);
#endif

        if (up) {
          m_filesel->goUp();
          return true;
        }
        if (enter) {
          m_filesel->goInsideFolder();
          return true;
        }
        if (back) {
          m_filesel->goBack();
          return true;
        }
        if (forward) {
          m_filesel->goForward();
          return true;
        }
        return false;
      }
    }
    return Widget::onProcessMessage(msg);
  }

private:
  FileSelector* m_filesel;
};

FileSelector::FileSelector(FileSelectorType type)
  : m_type(type)
  , m_navigationLocked(false)
{
  addChild(new ArrowNavigator(this));

  m_fileName = new CustomFileNameEntry;
  m_fileName->setFocusMagnet(true);
  fileNamePlaceholder()->addChild(m_fileName);

  goBackButton()->setFocusStop(false);
  goForwardButton()->setFocusStop(false);
  goUpButton()->setFocusStop(false);
  newFolderButton()->setFocusStop(false);

  set_gfxicon_to_button(goBackButton(),
                        PART_COMBOBOX_ARROW_LEFT,
                        PART_COMBOBOX_ARROW_LEFT_SELECTED,
                        PART_COMBOBOX_ARROW_LEFT_DISABLED,
                        JI_CENTER | JI_MIDDLE);
  set_gfxicon_to_button(goForwardButton(),
                        PART_COMBOBOX_ARROW_RIGHT,
                        PART_COMBOBOX_ARROW_RIGHT_SELECTED,
                        PART_COMBOBOX_ARROW_RIGHT_DISABLED,
                        JI_CENTER | JI_MIDDLE);
  set_gfxicon_to_button(goUpButton(),
                        PART_COMBOBOX_ARROW_UP,
                        PART_COMBOBOX_ARROW_UP_SELECTED,
                        PART_COMBOBOX_ARROW_UP_DISABLED,
                        JI_CENTER | JI_MIDDLE);
  set_gfxicon_to_button(newFolderButton(),
                        PART_NEWFOLDER,
                        PART_NEWFOLDER_SELECTED,
                        PART_NEWFOLDER,
                        JI_CENTER | JI_MIDDLE);

  setup_mini_look(goBackButton());
  setup_mini_look(goForwardButton());
  setup_mini_look(goUpButton());
  setup_mini_look(newFolderButton());

  m_fileList = new FileList();
  m_fileList->setId("fileview");
  fileView()->attachToView(m_fileList);
  m_fileName->setAssociatedFileList(m_fileList);

  goBackButton()->Click.connect(Bind<void>(&FileSelector::onGoBack, this));
  goForwardButton()->Click.connect(Bind<void>(&FileSelector::onGoForward, this));
  goUpButton()->Click.connect(Bind<void>(&FileSelector::onGoUp, this));
  newFolderButton()->Click.connect(Bind<void>(&FileSelector::onNewFolder, this));
  location()->CloseListBox.connect(Bind<void>(&FileSelector::onLocationCloseListBox, this));
  fileType()->Change.connect(Bind<void>(&FileSelector::onFileTypeChange, this));
  m_fileList->FileSelected.connect(Bind<void>(&FileSelector::onFileListFileSelected, this));
  m_fileList->FileAccepted.connect(Bind<void>(&FileSelector::onFileListFileAccepted, this));
  m_fileList->CurrentFolderChanged.connect(Bind<void>(&FileSelector::onFileListCurrentFolderChanged, this));
}

void FileSelector::goBack()
{
  onGoBack();
}

void FileSelector::goForward()
{
  onGoForward();
}

void FileSelector::goUp()
{
  onGoUp();
}

void FileSelector::goInsideFolder()
{
  if (m_fileList->getSelectedFileItem() &&
      m_fileList->getSelectedFileItem()->isBrowsable()) {
    m_fileList->setCurrentFolder(
      m_fileList->getSelectedFileItem());
  }
}

std::string FileSelector::show(
  const std::string& title,
  const std::string& initialPath,
  const std::string& showExtensions)
{
  std::string result;

  FileSystemModule* fs = FileSystemModule::instance();
  LockFS lock(fs);

  fs->refresh();

  if (!navigation_history) {
    navigation_history = new FileItemList();
    App::instance()->Exit.connect(&on_exit_delete_navigation_history);
  }

  // we have to find where the user should begin to browse files (start_folder)
  std::string start_folder_path;
  IFileItem* start_folder = NULL;

  // If initialPath doesn't contain a path.
  if (base::get_file_path(initialPath).empty()) {
    // Get the saved `path' in the configuration file.
    std::string path = get_config_string("FileSelect", "CurrentDirectory", "<empty>");
    if (path == "<empty>") {
      start_folder_path = base::get_user_docs_folder();
      path = base::join_path(start_folder_path, initialPath);
    }
    start_folder = fs->getFileItemFromPath(path);
  }
  else {
    // Remove the filename.
    start_folder_path = base::join_path(base::get_file_path(initialPath), "");
  }
  start_folder_path = base::fix_path_separators(start_folder_path);

  if (!start_folder)
    start_folder = fs->getFileItemFromPath(start_folder_path);

  PRINTF("start_folder_path = %s (%p)\n", start_folder_path.c_str(), start_folder);

  setMinSize(gfx::Size(ui::display_w()*9/10, ui::display_h()*9/10));
  remapWindow();
  centerWindow();

  // Change the file formats/extensions to be shown
  std::string initialExtension = base::get_file_extension(initialPath);
  std::string exts = showExtensions;
  if (m_type == FileSelectorType::Open) {
    auto it = preferred_open_extensions.find(exts);
    if (it == preferred_open_extensions.end())
      exts = showExtensions;
    else
      exts = preferred_open_extensions[exts];
  }
  else {
    ASSERT(m_type == FileSelectorType::Save);
    if (!initialExtension.empty())
      exts = initialExtension;
  }
  m_fileList->setExtensions(exts.c_str());
  if (start_folder)
    m_fileList->setCurrentFolder(start_folder);

  // current location
  navigation_position.reset();
  addInNavigationHistory(m_fileList->getCurrentFolder());

  // fill the location combo-box
  updateLocation();
  updateNavigationButtons();

  // fill file-type combo-box
  fileType()->removeAllItems();

  // Get the default extension from the given initial file name
  m_defExtension = initialExtension;

  // File type for all formats
  {
    ListItem* item = new ListItem("All formats");
    item->setValue(showExtensions);
    fileType()->addItem(item);
  }
  // One file type for each supported image format
  std::vector<std::string> tokens;
  base::split_string(showExtensions, tokens, ",");
  for (const auto& tok : tokens) {
    // If the default extension is empty, use the first filter
    if (m_defExtension.empty())
      m_defExtension = tok;

    ListItem* item = new ListItem(tok + " files");
    item->setValue(tok);
    fileType()->addItem(item);
  }
  // All files
  {
    ListItem* item = new ListItem("All files");
    item->setValue("");         // Empty extensions means "*.*"
    fileType()->addItem(item);
  }

  // file name entry field
  m_fileName->setValue(base::get_file_name(initialPath).c_str());
  m_fileName->getEntryWidget()->selectText(0, -1);
  fileType()->setValue(exts);

  // setup the title of the window
  setText(title.c_str());

  // get the ok-button
  Widget* ok = this->findChild("ok");

  // update the view
  View::getView(m_fileList)->updateView();

  // open the window and run... the user press ok?
again:
  openWindowInForeground();
  if (getKiller() == ok ||
      getKiller() == m_fileList) {
    // open the selected file
    IFileItem* folder = m_fileList->getCurrentFolder();
    ASSERT(folder);

    std::string fn = m_fileName->getValue();
    std::string buf;
    IFileItem* enter_folder = NULL;

    // up a level?
    if (fn == "..") {
      enter_folder = folder->getParent();
      if (!enter_folder)
        enter_folder = folder;
    }
    else if (fn.empty()) {
      // show the window again
      setVisible(true);
      goto again;
    }
    else {
      // check if the user specified in "fn" a item of "fileview"
      const FileItemList& children = m_fileList->getFileList();

      std::string fn2 = fn;
#ifdef _WIN32
      fn2 = base::string_to_lower(fn2);
#endif

      for (IFileItem* child : children) {
        std::string child_name = child->getDisplayName();

#ifdef _WIN32
        child_name = base::string_to_lower(child_name);
#endif
        if (child_name == fn2) {
          enter_folder = child;
          buf = enter_folder->getFileName();
          break;
        }
      }

      if (!enter_folder) {
        // does the file-name entry have separators?
        if (base::is_path_separator(*fn.begin())) { // absolute path (UNIX style)
#ifdef _WIN32
          // get the drive of the current folder
          std::string drive = folder->getFileName();
          if (drive.size() >= 2 && drive[1] == ':') {
            buf += drive[0];
            buf += ':';
            buf += fn;
          }
          else
            buf = base::join_path("C:", fn);
#else
          buf = fn;
#endif
        }
#ifdef _WIN32
        // does the file-name entry have colon?
        else if (fn.find(':') != std::string::npos) { // absolute path on Windows
          if (fn.size() == 2 && fn[1] == ':') {
            buf = base::join_path(fn, "");
          }
          else {
            buf = fn;
          }
        }
#endif
        else {
          buf = folder->getFileName();
          buf = base::join_path(buf, fn);
        }
        buf = base::fix_path_separators(buf);

        // we can check if 'buf' is a folder, so we have to enter in it
        enter_folder = fs->getFileItemFromPath(buf);
      }
    }

    // did we find a folder to enter?
    if (enter_folder &&
        enter_folder->isFolder() &&
        enter_folder->isBrowsable()) {
      // enter in the folder that was specified in the 'm_fileName'
      m_fileList->setCurrentFolder(enter_folder);

      // clear the text of the entry widget
      m_fileName->setValue("");

      // show the window again
      setVisible(true);
      goto again;
    }
    // else file-name specified in the entry is really a file to open...

    // does it not have extension? ...we should add the extension
    // selected in the filetype combo-box
    if (base::get_file_extension(buf).empty()) {
      buf += '.';
      buf += getSelectedExtension();
    }

    if (m_type == FileSelectorType::Save && base::is_file(buf)) {
      int ret = Alert::show("Warning<<File exists, overwrite it?<<%s||&Yes||&No||&Cancel",
                            base::get_file_name(buf).c_str());
      if (ret == 2) {
        setVisible(true);
        goto again;
      }
      else if (ret == 1) {
        // Check for read-only attribute
        if (base::has_readonly_attr(buf)) {
          ui::Alert::show(
            "Problem<<The selected file is read-only. Try with other file.||&Go back");

          setVisible(true);
          goto again;
        }
      }
      // Cancel
      else if (ret != 1) {
        return "";
      }
    }

    // duplicate the buffer to return a new string
    result = buf;

    // save the path in the configuration file
    std::string lastpath = folder->getKeyName();
    set_config_string("FileSelect", "CurrentDirectory",
                      lastpath.c_str());
  }

  return result;
}

// Updates the content of the combo-box that shows the current
// location in the file-system.
void FileSelector::updateLocation()
{
  IFileItem* currentFolder = m_fileList->getCurrentFolder();
  IFileItem* fileItem = currentFolder;
  std::list<IFileItem*> locations;
  int selected_index = -1;

  while (fileItem != NULL) {
    locations.push_front(fileItem);
    fileItem = fileItem->getParent();
  }

  // Clear all the items from the combo-box
  location()->removeAllItems();

  // Add item by item (from root to the specific current folder)
  int level = 0;
  for (std::list<IFileItem*>::iterator it=locations.begin(), end=locations.end();
       it != end; ++it) {
    fileItem = *it;

    // Indentation
    std::string buf;
    for (int c=0; c<level; ++c)
      buf += "  ";

    // Location name
    buf += fileItem->getDisplayName();

    // Add the new location to the combo-box
    location()->addItem(new CustomFileNameItem(buf.c_str(), fileItem));

    if (fileItem == currentFolder)
      selected_index = level;

    level++;
  }

  // Add paths from recent files list
  {
    location()->addItem("");
    location()->addItem("-------- Recent Paths --------");

    RecentFiles::const_iterator it = App::instance()->getRecentFiles()->paths_begin();
    RecentFiles::const_iterator end = App::instance()->getRecentFiles()->paths_end();
    for (; it != end; ++it)
      location()->addItem(new CustomFolderNameItem(it->c_str()));
  }

  // Select the location
  {
    location()->setSelectedItemIndex(selected_index);
    location()->getEntryWidget()->setText(currentFolder->getDisplayName().c_str());
    location()->getEntryWidget()->deselectText();
  }
}

void FileSelector::updateNavigationButtons()
{
  // Update the state of the go back button: if the navigation-history
  // has two elements and the navigation-position isn't the first one.
  goBackButton()->setEnabled(navigation_history->size() > 1 &&
                             (navigation_position.isNull() ||
                              navigation_position.getIterator() != navigation_history->begin()));

  // Update the state of the go forward button: if the
  // navigation-history has two elements and the navigation-position
  // isn't the last one.
  goForwardButton()->setEnabled(navigation_history->size() > 1 &&
                                (navigation_position.isNull() ||
                                 navigation_position.getIterator() != navigation_history->end()-1));

  // Update the state of the go up button: if the current-folder isn't
  // the root-item.
  IFileItem* currentFolder = m_fileList->getCurrentFolder();
  goUpButton()->setEnabled(currentFolder != FileSystemModule::instance()->getRootFileItem());
}

void FileSelector::addInNavigationHistory(IFileItem* folder)
{
  ASSERT(folder != NULL);
  ASSERT(folder->isFolder());

  // Remove the history from the current position
  if (navigation_position.isValid()) {
    navigation_history->erase(navigation_position.getIterator()+1, navigation_history->end());
    navigation_position.reset();
  }

  // If the history is empty or if the last item isn't the folder that
  // we are visiting...
  if (navigation_history->empty() ||
      navigation_history->back() != folder) {
    // We can add the location in the history
    navigation_history->push_back(folder);
    navigation_position.setIterator(navigation_history->end()-1);
  }
}

void FileSelector::onGoBack()
{
  if (navigation_history->size() > 1) {
    if (navigation_position.isNull())
      navigation_position.setIterator(navigation_history->end()-1);

    if (navigation_position.getIterator() != navigation_history->begin()) {
      navigation_position.setIterator(navigation_position.getIterator()-1);

      m_navigationLocked = true;
      m_fileList->setCurrentFolder(*navigation_position.getIterator());
      m_navigationLocked = false;
    }
  }
}

void FileSelector::onGoForward()
{
  if (navigation_history->size() > 1) {
    if (navigation_position.isNull())
      navigation_position.setIterator(navigation_history->begin());

    if (navigation_position.getIterator() != navigation_history->end()-1) {
      navigation_position.setIterator(navigation_position.getIterator()+1);

      m_navigationLocked = true;
      m_fileList->setCurrentFolder(*navigation_position.getIterator());
      m_navigationLocked = false;
    }
  }
}

void FileSelector::onGoUp()
{
  m_fileList->goUp();
}

void FileSelector::onNewFolder()
{
  app::gen::NewFolderWindow window;

  window.openWindowInForeground();
  if (window.getKiller() == window.ok()) {
    IFileItem* currentFolder = m_fileList->getCurrentFolder();
    if (currentFolder) {
      std::string dirname = window.name()->getText();

      // Create the new directory
      try {
        currentFolder->createDirectory(dirname);

        // Enter in the new folder
        for (FileItemList::const_iterator it=currentFolder->getChildren().begin(),
               end=currentFolder->getChildren().end(); it != end; ++it) {
          if ((*it)->getDisplayName() == dirname) {
            m_fileList->setCurrentFolder(*it);
            break;
          }
        }
      }
      catch (const std::exception& e) {
        Console::showException(e);
      }
    }
  }
}

// Hook for the 'location' combo-box
void FileSelector::onLocationCloseListBox()
{
  // When the user change the location we have to set the
  // current-folder in the 'fileview' widget
  CustomFileNameItem* comboFileItem = dynamic_cast<CustomFileNameItem*>(location()->getSelectedItem());
  IFileItem* fileItem = (comboFileItem != NULL ? comboFileItem->getFileItem(): NULL);

  // Maybe the user selected a recent file path
  if (fileItem == NULL) {
    CustomFolderNameItem* comboFolderItem =
      dynamic_cast<CustomFolderNameItem*>(location()->getSelectedItem());

    if (comboFolderItem != NULL) {
      std::string path = comboFolderItem->getText();
      fileItem = FileSystemModule::instance()->getFileItemFromPath(path);
    }
  }

  if (fileItem != NULL) {
    m_fileList->setCurrentFolder(fileItem);

    // Refocus the 'fileview' (the focus in that widget is more
    // useful for the user)
    getManager()->setFocus(m_fileList);
  }
}

// When the user selects a new file-type (extension), we have to
// change the file-extension in the 'filename' entry widget
void FileSelector::onFileTypeChange()
{
  std::string exts = fileType()->getValue();
  if (exts != m_fileList->extensions()) {
    m_navigationLocked = true;
    m_fileList->setExtensions(exts.c_str());
    m_navigationLocked = false;

    if (m_type == FileSelectorType::Open) {
      std::string origShowExtensions = fileType()->getItem(0)->getValue();
      preferred_open_extensions[origShowExtensions] = fileType()->getValue();
    }
  }

  if (m_type == FileSelectorType::Save) {
    std::string newExtension = getSelectedExtension();
    std::string fileName = m_fileName->getValue();
    std::string currentExtension = base::get_file_extension(fileName);

    if (!currentExtension.empty())
      m_fileName->setValue((fileName.substr(0, fileName.size()-currentExtension.size())+newExtension).c_str());
  }
}

void FileSelector::onFileListFileSelected()
{
  IFileItem* fileitem = m_fileList->getSelectedFileItem();

  if (!fileitem->isFolder()) {
    std::string filename = base::get_file_name(fileitem->getFileName());

    m_fileName->setValue(filename.c_str());
  }
}

void FileSelector::onFileListFileAccepted()
{
  closeWindow(m_fileList);
}

void FileSelector::onFileListCurrentFolderChanged()
{
  if (!m_navigationLocked)
    addInNavigationHistory(m_fileList->getCurrentFolder());

  updateLocation();
  updateNavigationButtons();

  // Close the autocomplete popup just in case it's open.
  m_fileName->closeListBox();
}

std::string FileSelector::getSelectedExtension() const
{
  std::string ext = fileType()->getValue();
  if (ext.empty() || ext.find(',') != std::string::npos)
    ext = m_defExtension;
  return ext;
}

} // namespace app
