// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/crash/session.h"

#include "app/console.h"
#include "app/context.h"
#include "app/crash/read_document.h"
#include "app/crash/write_document.h"
#include "app/document.h"
#include "app/document_access.h"
#include "app/file/file.h"
#include "app/ui_context.h"
#include "base/bind.h"
#include "base/convert_to.h"
#include "base/fs.h"
#include "base/path.h"
#include "base/process.h"
#include "base/split_string.h"
#include "base/string.h"

namespace app {
namespace crash {

Session::Backup::Backup(const std::string& dir)
  : m_dir(dir)
{
  DocumentInfo info;
  read_document_info(dir, info);

  std::vector<char> buf(1024);
  sprintf(&buf[0], "%s Sprite %dx%d with %d frame(s)",
    info.format == IMAGE_RGB ? "RGB":
    info.format == IMAGE_GRAYSCALE ? "Grayscale":
    info.format == IMAGE_INDEXED ? "Indexed":
    info.format == IMAGE_BITMAP ? "Bitmap": "Unknown",
    info.width, info.height, info.frames);
  m_desc = &buf[0];
}

Session::Session(const std::string& path)
  : m_path(path)
  , m_pid(0)
{
}

Session::~Session()
{
}

std::string Session::name() const
{
  std::string name = base::get_file_title(m_path);
  std::vector<std::string> parts;
  base::split_string(name, parts, "-");

  if (parts.size() == 3) {
    return "Session date: " + parts[0] + " time: " + parts[1] + " (PID " + parts[2] + ")";
  }
  else
    return name;
}

const Session::Backups& Session::backups()
{
  if (m_backups.empty()) {
    for (auto& item : base::list_files(m_path)) {
      std::string docDir = base::join_path(m_path, item);
      if (base::is_directory(docDir)) {
        m_backups.push_back(new Backup(docDir));
      }
    }
  }
  return m_backups;
}

bool Session::isRunning()
{
  loadPid();
  return base::is_process_running(m_pid);
}

bool Session::isEmpty()
{
  for (auto& item : base::list_files(m_path)) {
    if (base::is_directory(base::join_path(m_path, item)))
      return false;
  }
  return true;
}

void Session::create(base::pid pid)
{
  m_pid = pid;

#ifdef _WIN32
  std::ofstream of(base::from_utf8(pidFilename()));
#else
  std::ofstream of(pidFilename());
#endif
  of << m_pid;
}

void Session::removeFromDisk()
{
  if (base::is_file(pidFilename()))
    base::delete_file(pidFilename());

  try {
    base::remove_directory(m_path);
  }
  catch (const std::exception& ex) {
    TRACE("Session directory cannot be removed, it's not empty\nError: '%s'\n",
      ex.what());
  }
}

void Session::saveDocumentChanges(app::Document* doc)
{
  DocumentReader reader(doc);
  DocumentWriter writer(reader);
  app::Context ctx;
  std::string dir = base::join_path(m_path,
    base::convert_to<std::string>(doc->id()));
  TRACE("DataRecovery: Saving document '%s'...\n", dir.c_str());

  if (!base::is_directory(dir))
    base::make_directory(dir);

  // Save document information
  write_document(dir, doc);
}

void Session::removeDocument(app::Document* doc)
{
  delete_document_internals(doc);

  // Delete document backup directory
  std::string dir = base::join_path(m_path,
    base::convert_to<std::string>(doc->id()));
  if (base::is_directory(dir))
    deleteDirectory(dir);
}

void Session::restoreBackup(Backup* backup)
{
  Console console;
  try {
    app::Document* doc = read_document(backup->dir());
    if (doc)
      UIContext::instance()->documents().add(doc);
  }
  catch (const std::exception& ex) {
    Console::showException(ex);
  }
}

void Session::deleteBackup(Backup* backup)
{
  try {
    auto it = std::find(m_backups.begin(), m_backups.end(), backup);
    ASSERT(it != m_backups.end());
    if (it != m_backups.end())
      m_backups.erase(it);

    if (base::is_directory(backup->dir()))
      deleteDirectory(backup->dir());
  }
  catch (const std::exception& ex) {
    Console::showException(ex);
  }
}

void Session::loadPid()
{
  if (m_pid)
    return;

  std::string pidfile = pidFilename();
  if (base::is_file(pidfile)) {
    std::ifstream pf(pidfile);
    if (pf)
      pf >> m_pid;
  }
}

std::string Session::pidFilename() const
{
  return base::join_path(m_path, "pid");
}

void Session::deleteDirectory(const std::string& dir)
{
  for (auto& item : base::list_files(dir)) {
    std::string objfn = base::join_path(dir, item);
    if (base::is_file(objfn))
      base::delete_file(objfn);
  }
  base::remove_directory(dir);
}

} // namespace crash
} // namespace app
