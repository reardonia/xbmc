/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PythonTools.h"

#include <iostream>
#include <stdexcept>
#include <vector>

#include "Helper.h"
#include "SwigTypeParser.h"

namespace swigbindings
{

namespace
{

bool StartsWith(const std::string& s, const std::string& prefix)
{
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string Replace(std::string s, const std::string& from, const std::string& to)
{
  if (from.empty())
    return s;
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos)
  {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
  return s;
}

// api spec ltype -> python parse format char. A miss returns the empty string;
// callers treat "" exactly as the Python None (object, format 'O').
std::string LtypeToFormatChar(const std::string& ltype)
{
  if (ltype == "p.char")
    return "s";
  if (ltype == "bool")
    return "b";
  if (ltype == "int")
    return "i";
  if (ltype == "unsigned int")
    return "I";
  if (ltype == "long")
    return "l";
  if (ltype == "unsigned long")
    return "k";
  if (ltype == "double")
    return "d";
  if (ltype == "float")
    return "f";
  if (ltype == "long long")
    return "L";
  return std::string();
}

bool LtypeHasFormatChar(const std::string& ltype)
{
  return !LtypeToFormatChar(ltype).empty();
}

} // namespace

bool ParameterCanBeUsedDirectly(Node* param)
{
  const std::string* type = param->Attr("type");
  return LtypeHasFormatChar(ConvertTypeToLTypeForParam(type ? *type : std::string()));
}

std::string MakeFormatStringFromParameters(Node* method)
{
  if (method == nullptr)
    return std::string();
  std::vector<Node*> params = method->Kids("parm");
  std::string fmt;
  bool previousDefaulted = false;
  for (Node* param : params)
  {
    const std::string* defaultValue = param->Attr("value");
    const std::string* type = param->Attr("type");
    const std::string paramtype = ConvertTypeToLTypeForParam(type ? *type : std::string());
    std::string curFormat = LtypeToFormatChar(paramtype);
    if (curFormat.empty()) // then we will assume it's an object
      curFormat = "O";
    if (defaultValue != nullptr && !previousDefaulted)
    {
      fmt += "|";
      previousDefaulted = true;
    }
    fmt += curFormat;
  }
  return fmt;
}

std::string GetClassNameAsVariable(Node* clazz)
{
  return Replace(FindFullClassName(clazz), "::", "_");
}

std::string GetPyMethodName(Node* method, MethodType methodType)
{
  bool hv = false;
  const std::string full = FindFullClassName(method, hv);
  const bool haveClazz = hv;
  const std::string clazz = haveClazz ? Replace(full, "::", "_") : std::string();

  if (!(haveClazz || methodType == MethodType::Method))
    throw std::runtime_error("Cannot use a non-class function as a constructor or destructor");
  if (!(method->Name() != "class" ||
        (methodType == MethodType::Constructor || methodType == MethodType::Destructor)))
    throw std::runtime_error("class node used with non-ctor/dtor method type");
  if (!(method->Name() != "constructor" || methodType == MethodType::Constructor))
    throw std::runtime_error("Cannot use a constructor node and not identify the type as a constructor");
  if (!(method->Name() != "destructor" || methodType == MethodType::Destructor))
    throw std::runtime_error("Cannot use a destructor node and not identify the type as a destructor");

  if (!haveClazz)
  {
    const std::string* sn = method->Attr("sym_name");
    return sn ? *sn : std::string();
  }

  if (methodType == MethodType::Constructor)
    return clazz + "_New";

  if (methodType == MethodType::Destructor)
    return clazz + "_Dealloc";

  const std::string* nm = method->Attr("name");
  if (nm != nullptr && StartsWith(*nm, "operator "))
  {
    const std::string tail = nm->substr(9);
    if (tail == "[]")
      return clazz + "_operatorIndex_";
    if (tail == "()")
      return clazz + "_callable_";
  }

  const std::string* sn = method->Attr("sym_name");
  return clazz + "_" + (sn ? *sn : std::string());
}

std::string MakeDocString(Node* docnode)
{
  if (docnode == nullptr || docnode->Name() != "doc")
    throw std::runtime_error("Invalid doc Node passed to MakeDocString");

  const std::string* value = docnode->Attr("value");
  const std::string text = value ? *value : std::string();

  // split on '\n'
  std::vector<std::string> lines;
  {
    size_t start = 0;
    for (;;)
    {
      const size_t p = text.find('\n', start);
      if (p == std::string::npos)
      {
        lines.push_back(text.substr(start));
        break;
      }
      lines.push_back(text.substr(start, p - start));
      start = p + 1;
    }
  }

  std::string ret;
  const size_t n = lines.size();
  for (size_t index = 0; index < n; ++index)
  {
    std::string val = lines[index];
    val = Replace(val, "\\n", "");   // remove extraneous \n's (literal backslash-n)
    val = Replace(val, "\\", "\\\\"); // escape backslash
    val = Replace(val, "\"", "\\\""); // escape quotes
    ret += std::string("\"") + val + "\\n\"" + (index != n - 1 ? "\n" : "");
  }
  return ret;
}

Node* FindValidBaseClass(Node* clazz, Node* module, bool warn)
{
  std::vector<Node*> baselists = clazz->Kids("baselist");
  if (baselists.size() >= 2)
    throw std::runtime_error("class has multiple baselists - need code to separate the public one");

  std::vector<Node*> knownbases;
  if (!baselists.empty())
  {
    for (Node* b : baselists[0]->Kids("base"))
    {
      const std::string* bname = b->Attr("name");
      Node* baseclassnode = FindClassNodeByName(module, bname ? *bname : std::string(), clazz);
      if (baseclassnode != nullptr)
      {
        knownbases.push_back(baseclassnode);
      }
      else if (warn && !IsKnownBaseType(bname ? *bname : std::string(), clazz))
      {
        const std::string* mn = module->Attr("name");
        std::cerr << "WARNING: the base class " << (bname ? *bname : std::string()) << " for "
                  << FindFullClassName(clazz) << " is unrecognized within "
                  << (mn ? *mn : std::string()) << "." << "\n";
      }
    }
  }
  if (knownbases.size() >= 2)
    throw std::runtime_error("too many known base classes; multiple inheritance unsupported");
  return knownbases.empty() ? nullptr : knownbases[0];
}

} // namespace swigbindings
