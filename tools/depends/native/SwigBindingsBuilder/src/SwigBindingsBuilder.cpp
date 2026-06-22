/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

// SwigBindingsBuilder
//
// Standalone host-side code generator for Kodi's Python bindings. It reads the
// `swig -xml` description of a module and emits the CPython binding .cpp. Built
// as a native build tool (tools/depends/native/SwigBindingsBuilder), it needs
// only a host C++ compiler: no Java, no Groovy, no host Python interpreter. XML
// is parsed with tinyxml2 (Kodi's own XML library, vendored alongside).
//
//   usage: SwigBindingsBuilder <module.i.xml> <out.i.cpp>
//
// The generation logic is a direct port of the reference generator in
// tools/codegenerator (node/transform/swig_type_parser/typemaps/emit). Each
// piece is verified byte-for-byte against the committed golden output.

#include <cstdio>
#include <exception>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "Emit.h"
#include "Node.h"
#include "SwigTypeParser.h"
#include "Transform.h"
#include "tinyxml2.h"

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    std::fprintf(stderr, "usage: %s <module.i.xml> <out.i.cpp>\n", argv[0]);
    return 1;
  }

  const std::string xmlPath = argv[1];
  const std::string outPath = argv[2];

  try
  {
    tinyxml2::XMLDocument doc;
    const tinyxml2::XMLError err = doc.LoadFile(xmlPath.c_str());
    if (err != tinyxml2::XML_SUCCESS)
    {
      std::fprintf(stderr, "%s: could not parse input: %s\n", argv[0], xmlPath.c_str());
      return 1;
    }

    const tinyxml2::XMLElement* root = doc.RootElement();
    if (root == nullptr)
    {
      std::fprintf(stderr, "%s: input has no root element: %s\n", argv[0], xmlPath.c_str());
      return 1;
    }

    std::unique_ptr<swigbindings::Node> module = swigbindings::TransformSwigXml(root);

    swigbindings::ResetTypeTable();
    for (swigbindings::Node* tt : module->Kids("typetab"))
    {
      std::vector<swigbindings::TypeTableEntry> entries;
      for (swigbindings::Node* e : tt->Kids("entry"))
      {
        swigbindings::TypeTableEntry entry;
        const std::string* ns = e->Attr("namespace");
        const std::string* ty = e->Attr("type");
        const std::string* base = e->Attr("basetype");
        entry.namespaceStr = ns ? *ns : std::string();
        entry.type = ty ? *ty : std::string();
        entry.basetype = base ? *base : std::string();
        entries.push_back(entry);
      }
      swigbindings::AppendTypeTable(entries);
    }

    const std::string out = swigbindings::Generate(module.get());

    // binary mode so '\n' is never translated to '\r\n' on Windows hosts: the
    // reference (Groovy File.write) emits '\n' on every platform and the goldens
    // are '\n'. The generated bindings are ASCII, so the byte stream is stable.
    std::ofstream os(outPath, std::ios::binary);
    if (!os)
    {
      std::fprintf(stderr, "%s: could not open output for writing: %s\n", argv[0], outPath.c_str());
      return 1;
    }
    os << out;
  }
  catch (const std::exception& ex)
  {
    std::fprintf(stderr, "%s: %s\n", argv[0], ex.what());
    return 1;
  }

  return 0;
}
