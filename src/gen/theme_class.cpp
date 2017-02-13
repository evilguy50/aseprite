// Aseprite Code Generator
// Copyright (c) 2015-2017 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "gen/theme_class.h"

#include "base/string.h"
#include "gen/common.h"

#include <iostream>
#include <vector>

void gen_theme_class(TiXmlDocument* doc, const std::string& inputFn)
{
  std::vector<std::string> dimensions;
  std::vector<std::string> colors;
  std::vector<std::string> parts;
  std::vector<std::string> styles;
  std::vector<std::string> newStyles;

  TiXmlHandle handle(doc);
  TiXmlElement* elem = handle
    .FirstChild("theme")
    .FirstChild("dimensions")
    .FirstChild("dim").ToElement();
  while (elem) {
    const char* id = elem->Attribute("id");
    dimensions.push_back(id);
    elem = elem->NextSiblingElement();
  }

  elem = handle
    .FirstChild("theme")
    .FirstChild("colors")
    .FirstChild("color").ToElement();
  while (elem) {
    const char* id = elem->Attribute("id");
    colors.push_back(id);
    elem = elem->NextSiblingElement();
  }

  elem = handle
    .FirstChild("theme")
    .FirstChild("parts")
    .FirstChild("part").ToElement();
  while (elem) {
    const char* id = elem->Attribute("id");
    if (!strchr(id, ':'))
      parts.push_back(id);
    elem = elem->NextSiblingElement();
  }

  elem = handle
    .FirstChild("theme")
    .FirstChild("stylesheet")
    .FirstChild("style").ToElement();
  while (elem) {
    const char* id = elem->Attribute("id");
    if (!strchr(id, ':'))
      styles.push_back(id);
    elem = elem->NextSiblingElement();
  }

  elem = handle
    .FirstChild("theme")
    .FirstChild("styles")
    .FirstChild("style").ToElement();
  while (elem) {
    const char* id = elem->Attribute("id");
    newStyles.push_back(id);
    elem = elem->NextSiblingElement();
  }

  std::cout
    << "// Don't modify, generated file from " << inputFn << "\n"
    << "\n"
    << "#ifndef GENERATED_THEME_H_INCLUDED\n"
    << "#define GENERATED_THEME_H_INCLUDED\n"
    << "#pragma once\n"
    << "\n"
    << "namespace app {\n"
    << "namespace gen {\n"
    << "\n"
    << "  template<typename T>\n"
    << "  class ThemeFile {\n"
    << "  public:\n"
    << "\n";

  // Dimensions sub class
  std::cout
    << "    class Dimensions {\n"
    << "      template<typename> friend class ThemeFile;\n"
    << "    public:\n";
  for (auto dimension : dimensions) {
    std::string id = convert_xmlid_to_cppid(dimension, false);
    std::cout
      << "      int " << id << "() const { return m_" << id << "; }\n";
  }
  std::cout
    << "    private:\n";
  for (auto dimension : dimensions) {
    std::string id = convert_xmlid_to_cppid(dimension, false);
    std::cout
      << "      int m_" << id << ";\n";
  }
  std::cout
    << "    };\n";

  // Colors sub class
  std::cout
    << "    class Colors {\n"
    << "      template<typename> friend class ThemeFile;\n"
    << "    public:\n";
  for (auto color : colors) {
    std::string id = convert_xmlid_to_cppid(color, false);
    std::cout
      << "      gfx::Color " << id << "() const { return m_" << id << "; }\n";
  }
  std::cout
    << "    private:\n";
  for (auto color : colors) {
    std::string id = convert_xmlid_to_cppid(color, false);
    std::cout
      << "      gfx::Color m_" << id << ";\n";
  }
  std::cout
    << "    };\n";

  // Parts sub class
  std::cout
    << "    class Parts {\n"
    << "      template<typename> friend class ThemeFile;\n"
    << "    public:\n";
  for (auto part : parts) {
    std::string id = convert_xmlid_to_cppid(part, false);
    std::cout
      << "      const skin::SkinPartPtr& " << id << "() const { return m_" << id << "; }\n";
  }
  std::cout
    << "    private:\n";
  for (auto part : parts) {
    std::string id = convert_xmlid_to_cppid(part, false);
    std::cout
      << "      skin::SkinPartPtr m_" << id << ";\n";
  }
  std::cout
    << "    };\n";

  // Styles sub class
  std::cout
    << "\n"
    << "    class Styles {\n"
    << "      template<typename> friend class ThemeFile;\n"
    << "    public:\n";
  for (auto style : styles) {
    std::string id = convert_xmlid_to_cppid(style, false);
    std::cout
      << "      skin::Style* " << id << "() const { return m_" << id << "; }\n";
  }
  std::cout
    << "    private:\n";
  for (auto style : styles) {
    std::string id = convert_xmlid_to_cppid(style, false);
    std::cout
      << "      skin::Style* m_" << id << ";\n";
  }
  std::cout
    << "    };\n";

  // New styles sub class
  std::cout
    << "\n"
    << "    class NewStyles {\n"
    << "      template<typename> friend class ThemeFile;\n"
    << "    public:\n";
  for (auto style : newStyles) {
    std::string id = convert_xmlid_to_cppid(style, false);
    std::cout
      << "      ui::Style* " << id << "() const { return m_" << id << "; }\n";
  }
  std::cout
    << "    private:\n";
  for (auto style : newStyles) {
    std::string id = convert_xmlid_to_cppid(style, false);
    std::cout
      << "      ui::Style* m_" << id << ";\n";
  }
  std::cout
    << "    };\n";

  std::cout
    << "\n"
    << "    Dimensions dimensions;\n"
    << "    Colors colors;\n"
    << "    Parts parts;\n"
    << "    Styles styles;\n"
    << "    NewStyles newStyles;\n"
    << "\n"
    << "  protected:\n"
    << "    void updateInternals() {\n";
  for (auto dimension : dimensions) {
    std::string id = convert_xmlid_to_cppid(dimension, false);
    std::cout << "      byId(dimensions.m_" << id
              << ", \"" << dimension << "\");\n";
  }
  for (auto color : colors) {
    std::string id = convert_xmlid_to_cppid(color, false);
    std::cout << "      byId(colors.m_" << id
              << ", \"" << color << "\");\n";
  }
  for (auto part : parts) {
    std::string id = convert_xmlid_to_cppid(part, false);
    std::cout << "      byId(parts.m_" << id
              << ", \"" << part << "\");\n";
  }
  for (auto style : styles) {
    std::string id = convert_xmlid_to_cppid(style, false);
    std::cout << "      byId(styles.m_" << id
              << ", \"" << style << "\");\n";
  }
  for (auto newStyle : newStyles) {
    std::string id = convert_xmlid_to_cppid(newStyle, false);
    std::cout << "      byId(newStyles.m_" << id
              << ", \"" << newStyle << "\");\n";
  }
  std::cout
    << "    }\n"
    << "\n"
    << "  private:\n"
    << "    void byId(int& dimension, const std::string& id) {\n"
    << "      dimension = static_cast<T*>(this)->getDimensionById(id);\n"
    << "    }\n"
    << "    void byId(gfx::Color& color, const std::string& id) {\n"
    << "      color = static_cast<T*>(this)->getColorById(id);\n"
    << "    }\n"
    << "    void byId(skin::SkinPartPtr& part, const std::string& id) {\n"
    << "      part = static_cast<T*>(this)->getPartById(id);\n"
    << "    }\n"
    << "    void byId(skin::Style*& style, const std::string& id) {\n"
    << "      style = static_cast<T*>(this)->getStyle(id);\n"
    << "    }\n"
    << "    void byId(ui::Style*& style, const std::string& id) {\n"
    << "      style = static_cast<T*>(this)->getNewStyle(id);\n"
    << "    }\n";

  std::cout
    << "  };\n"
    << "\n"
    << "} // namespace gen\n"
    << "} // namespace app\n"
    << "\n"
    << "#endif\n";
}
