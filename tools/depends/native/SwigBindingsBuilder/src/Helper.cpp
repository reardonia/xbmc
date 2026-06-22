/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Helper.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "SwigTypeParser.h"

namespace swigbindings
{

namespace
{

// --- registry state (populated by Setup) ----------------------------------
std::vector<Node*> g_classes;
std::vector<std::pair<std::string, TypemapFn>> g_outExact;
std::vector<PatternEntry> g_outPatterns;
TypemapFn g_defaultOut;
std::vector<std::pair<std::string, TypemapFn>> g_inExact;
std::vector<PatternEntry> g_inPatterns;
TypemapFn g_defaultIn;

// the active Sequence (Python module-global `_current`), or nullptr.
Sequence* g_current = nullptr;

bool StartsWith(const std::string& s, const std::string& prefix)
{
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string& s, const std::string& suffix)
{
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string Strip(const std::string& s)
{
  size_t a = 0;
  size_t b = s.size();
  auto isws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; };
  while (a < b && isws(s[a]))
    ++a;
  while (b > a && isws(s[b - 1]))
    --b;
  return s.substr(a, b - a);
}

std::vector<std::string> SplitComma(const std::string& s)
{
  std::vector<std::string> out;
  size_t start = 0;
  for (;;)
  {
    const size_t p = s.find(',', start);
    if (p == std::string::npos)
    {
      out.push_back(s.substr(start));
      break;
    }
    out.push_back(s.substr(start, p - start));
    start = p + 1;
  }
  return out;
}

const TypemapFn* ExactLookup(const std::vector<std::pair<std::string, TypemapFn>>& exact,
                             const std::string& key)
{
  for (const auto& kv : exact)
    if (kv.first == key)
      return &kv.second;
  return nullptr;
}

// re.fullmatch semantics: the whole string must match.
const TypemapFn* FirstMatch(const std::vector<PatternEntry>& patterns, const std::string& s)
{
  for (const auto& pe : patterns)
    if (std::regex_match(s, pe.rx))
      return &pe.fn;
  return nullptr;
}

// recursive RFIND on '::' (namespace chop) reused by IsKnownApiType.
long RFindSep(const std::string& s)
{
  const size_t p = s.rfind("::");
  return p == std::string::npos ? -1 : static_cast<long>(p);
}

} // namespace

void Setup(const std::vector<Node*>& classes,
           const std::vector<std::pair<std::string, TypemapFn>>& outExact,
           const std::vector<PatternEntry>& outPatterns,
           TypemapFn defaultOut,
           const std::vector<std::pair<std::string, TypemapFn>>& inExact,
           const std::vector<PatternEntry>& inPatterns,
           TypemapFn defaultIn)
{
  g_classes = classes;
  g_outExact = outExact;
  g_outPatterns = outPatterns;
  g_defaultOut = std::move(defaultOut);
  g_inExact = inExact;
  g_inPatterns = inPatterns;
  g_defaultIn = std::move(defaultIn);
}

std::string GetOutConversion(const std::string& apiType,
                             const std::string& apiName,
                             Node* method,
                             const OverrideBindings* overrideBindings,
                             bool recurse)
{
  const TypemapFn* convertTemplate = ExactLookup(g_outExact, apiType);
  std::string className;
  bool haveClassName = false;

  if (convertTemplate == nullptr && StartsWith(apiType, "p."))
  {
    Node* refRoot = Parents(method)[0];
    Node* classNode = FindClassNodeByName(refRoot, GetRootType(apiType), method);
    if (classNode != nullptr)
    {
      bool hv = false;
      className = FindFullClassName(classNode, hv);
      haveClassName = hv;
      convertTemplate = &g_defaultOut;
    }
  }

  if (convertTemplate == nullptr)
    convertTemplate = FirstMatch(g_outPatterns, apiType);

  if (convertTemplate == nullptr)
  {
    bool matched = false;
    const std::string knownApiType = IsKnownApiType(apiType, method, matched);
    if (matched)
    {
      convertTemplate = &g_defaultOut;
      className = knownApiType;
      haveClassName = true;
    }
  }

  if (convertTemplate == nullptr)
  {
    const std::string apiTypeResolved = SwigType_resolve_all_typedefs(apiType);
    if (apiTypeResolved != apiType)
      return GetOutConversion(apiTypeResolved, apiName, method, overrideBindings, recurse);
    else if (recurse)
      return GetOutConversion(SwigType_ltype(apiType), apiName, method, overrideBindings, false);
    else
    {
      bool m = false;
      IsKnownApiType(apiType, method, m);
      if (!m)
        throw std::runtime_error("WARNING: Cannot convert the return value of swig type " + apiType +
                                 " for the call " + FindFullClassName(method) +
                                 "::" + CallingName(method));
    }
  }

  const bool seqSetHere = (g_current == nullptr);
  Sequence localSeq;
  if (seqSetHere)
    g_current = &localSeq;
  Sequence* seq = g_current;

  Bindings bindings;
  bindings.result = apiName;
  bindings.api = "apiResult";
  bindings.type = apiType;
  bindings.method = method;
  bindings.sequence = seq;
  if (haveClassName)
    bindings.classname = className;
  if (overrideBindings != nullptr)
  {
    if (overrideBindings->result)
      bindings.result = *overrideBindings->result;
    if (overrideBindings->api)
      bindings.api = *overrideBindings->api;
    if (overrideBindings->type)
      bindings.type = *overrideBindings->type;
    if (overrideBindings->ltype)
      bindings.ltype = *overrideBindings->ltype;
    if (overrideBindings->sequence != nullptr)
      bindings.sequence = overrideBindings->sequence;
  }

  // Matches the Python: a still-null template here would crash (None not
  // callable). It cannot happen on valid input; guard rather than UB.
  if (convertTemplate == nullptr)
    throw std::runtime_error("getOutConversion: no template for swig type " + apiType);

  const std::string ret = (*convertTemplate)(bindings);
  if (seqSetHere)
    g_current = nullptr;
  return ret;
}

std::string GetInConversion(const std::string& apiType,
                            const std::string& apiName,
                            const std::string& slName,
                            Node* method,
                            const OverrideBindings* overrideBindings)
{
  const std::string paramName = apiName;
  const TypemapFn* convertTemplate = ExactLookup(g_inExact, apiType);

  const std::string apiLType = ConvertTypeToLTypeForParam(apiType);
  if (convertTemplate == nullptr)
    convertTemplate = ExactLookup(g_inExact, apiLType);

  if (convertTemplate == nullptr && StartsWith(apiType, "p."))
  {
    const std::string thisNamespace = FindNamespace(method);
    const std::string target = apiLType.size() >= 2 ? apiLType.substr(2) : std::string();
    Node* clazz = nullptr;
    for (Node* it : g_classes)
    {
      const std::string ffcn = FindFullClassName(it);
      const std::string* symName = it->Attr("sym_name");
      if (ffcn == target ||
          (symName != nullptr && *symName == target && thisNamespace == FindNamespace(it)))
      {
        clazz = it;
        break;
      }
    }
    if (clazz != nullptr)
      convertTemplate = &g_defaultIn;
  }

  if (convertTemplate == nullptr)
    convertTemplate = FirstMatch(g_inPatterns, apiType);

  if (convertTemplate == nullptr)
    convertTemplate = FirstMatch(g_inPatterns, apiLType);

  if (convertTemplate == nullptr)
  {
    const std::string apiTypeResolved = SwigType_resolve_all_typedefs(apiType);
    if (apiTypeResolved != apiType)
      return GetInConversion(apiTypeResolved, apiName, slName, method, overrideBindings);
    bool m1 = false;
    bool m2 = false;
    IsKnownApiType(apiType, method, m1);
    IsKnownApiType(apiLType, method, m2);
    if (!m1 && !m2)
      std::cerr << "WARNING: Unknown parameter type: " << apiType << " (or " << apiLType
                << ") for the call " << FindFullClassName(method) << "::" << CallingName(method)
                << "\n";
    convertTemplate = &g_defaultIn;
  }

  // convertTemplate is always non-null past this point.
  const bool seqSetHere = (g_current == nullptr);
  Sequence localSeq;
  if (seqSetHere)
    g_current = &localSeq;
  Sequence* seq = g_current;

  Bindings bindings;
  bindings.type = apiType;
  bindings.ltype = apiLType;
  bindings.slarg = slName;
  bindings.api = apiName;
  bindings.param = paramName;
  bindings.method = method;
  bindings.sequence = seq;
  if (overrideBindings != nullptr)
  {
    if (overrideBindings->result)
      bindings.result = *overrideBindings->result;
    if (overrideBindings->api)
      bindings.api = *overrideBindings->api;
    if (overrideBindings->type)
      bindings.type = *overrideBindings->type;
    if (overrideBindings->ltype)
      bindings.ltype = *overrideBindings->ltype;
    if (overrideBindings->sequence != nullptr)
      bindings.sequence = overrideBindings->sequence;
  }

  const std::string ret = (*convertTemplate)(bindings);
  if (seqSetHere)
    g_current = nullptr;
  return ret;
}

// --- node helpers ----------------------------------------------------------
bool HasDefinedConstructor(Node* clazz)
{
  return !clazz->Kids("constructor").empty();
}

bool HasDoc(Node* methodOrClass)
{
  std::vector<Node*> docs = methodOrClass->Kids("doc");
  return !docs.empty() && docs[0]->Attr("value") != nullptr;
}

bool HasHiddenConstructor(Node* clazz)
{
  if (!HasDefinedConstructor(clazz))
    return false;
  Node* ctor = clazz->Kids("constructor")[0];
  const std::string* access = ctor->Attr("access");
  return access != nullptr && *access != "public";
}

Node* FindClassNodeByName(Node* module, const std::string& classname, Node* referenceNode)
{
  for (Node* it : module->DepthFirst())
  {
    if (it->Name() != "class")
      continue;
    if (Strip(FindFullClassName(it)) == Strip(classname))
      return it;
    if (referenceNode != nullptr)
    {
      bool hv = false;
      const std::string ffcn = FindFullClassName(it, hv);
      if ((FindNamespace(referenceNode) + classname) == (hv ? ffcn : std::string()))
        return it;
    }
    const std::string* nm = it->Attr("name");
    if (nm != nullptr && *nm == classname)
      return it;
  }
  return nullptr;
}

Node* FindClassNode(Node* node)
{
  if (node->Name() == "class")
    return node;
  return node->Parent() == nullptr ? nullptr : FindClassNode(node->Parent());
}

std::string FindFullClassName(Node* node, bool& hasValue)
{
  // separator "::", filename false (the only form the build path uses).
  std::string ret;
  bool haveRet = false;
  std::vector<Node*> rents = Parents(node, [](Node* it) { return it->Name() == "class"; });
  if (node->Name() == "class")
    rents.push_back(node);
  for (Node* it : rents)
  {
    const std::string* sn = it->Attr("sym_name");
    const std::string val = sn ? *sn : std::string();
    if (!haveRet)
    {
      ret = val;
      haveRet = true;
    }
    else
    {
      ret += "::" + val;
    }
  }
  if (haveRet)
  {
    hasValue = true;
    return FindNamespace(node, "::", true, false) + ret;
  }
  hasValue = false;
  return std::string();
}

std::string FindFullClassName(Node* node)
{
  bool hv = false;
  return FindFullClassName(node, hv);
}

std::string FindNamespace(Node* node,
                          const std::string& separator,
                          bool endingSeparator,
                          bool filename)
{
  std::string ret;
  bool haveRet = false;
  for (Node* it : Parents(node, [](Node* n) { return n->Name() == "namespace"; }))
  {
    const std::string* nm = it->Attr("name");
    std::string data = nm ? *nm : std::string();
    if (filename)
    {
      // data.replace("_", "__")
      std::string tmp;
      for (char c : data)
      {
        if (c == '_')
          tmp += "__";
        else
          tmp += c;
      }
      data = tmp;
    }
    if (!haveRet)
    {
      ret = data;
      haveRet = true;
    }
    else
    {
      ret += separator + data;
    }
  }
  if (!haveRet)
    return std::string();
  return ret + (endingSeparator ? separator : std::string());
}

std::string GetPropertyReturnSwigType(Node* method)
{
  const std::string* decl = method->Attr("decl");
  const std::string prefix = (decl != nullptr && *decl == "p.") ? "p." : "";
  const std::string* type = method->Attr("type");
  return type != nullptr ? prefix + *type : "void";
}

std::string GetReturnSwigType(Node* method)
{
  const std::string* decl = method->Attr("decl");
  const std::string prefix = (decl != nullptr && EndsWith(*decl, ".p.")) ? "p." : "";
  const std::string* type = method->Attr("type");
  return type != nullptr ? prefix + *type : "void";
}

std::string CallingName(Node* method)
{
  bool hv = false;
  const std::string clazz = FindFullClassName(method, hv);
  if (!hv)
  {
    const std::string* nm = method->Attr("name");
    return nm ? *nm : std::string();
  }
  if (method->Name() == "constructor")
  {
    const std::string* sn = method->Attr("sym_name");
    return "new " + FindNamespace(method) + (sn ? *sn : std::string());
  }
  if (method->Name() == "destructor")
    return "delete";
  const std::string* nm = method->Attr("name");
  return nm ? *nm : std::string();
}

std::vector<Node*> GetInsertNodes(Node* module, const std::string& section)
{
  std::vector<Node*> out;
  for (Node* it : module->Kids("insert"))
  {
    const std::string* s = it->Attr("section");
    if ((s != nullptr && section == *s) || (section == "header" && s == nullptr))
      out.push_back(it);
  }
  return out;
}

// Minimal HTML-entity unescape. On the build path the XML parser (tinyxml2) has
// already decoded the standard XML entities, so this is a pass-through for the
// generated bindings; it still decodes the common named/numeric entities for
// parity with commons-text unescapeHtml4 applied on top.
std::string Unescape(const std::string& s)
{
  std::string out;
  out.reserve(s.size());
  size_t i = 0;
  while (i < s.size())
  {
    if (s[i] != '&')
    {
      out.push_back(s[i]);
      ++i;
      continue;
    }
    const size_t semi = s.find(';', i);
    if (semi == std::string::npos || semi - i > 12)
    {
      out.push_back(s[i]);
      ++i;
      continue;
    }
    const std::string ent = s.substr(i + 1, semi - i - 1);
    if (ent == "lt")
      out.push_back('<');
    else if (ent == "gt")
      out.push_back('>');
    else if (ent == "amp")
      out.push_back('&');
    else if (ent == "quot")
      out.push_back('"');
    else if (ent == "apos")
      out.push_back('\'');
    else if (!ent.empty() && ent[0] == '#')
    {
      long code = 0;
      if (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X'))
        code = std::strtol(ent.c_str() + 2, nullptr, 16);
      else
        code = std::strtol(ent.c_str() + 1, nullptr, 10);
      if (code >= 0 && code < 0x80)
        out.push_back(static_cast<char>(code));
      else
        out.append(s.substr(i, semi - i + 1)); // out of ASCII: keep literal
    }
    else
    {
      // unknown entity: keep literally
      out.push_back('&');
      out.append(ent);
      out.push_back(';');
      i = semi + 1;
      continue;
    }
    i = semi + 1;
  }
  return out;
}

std::string UnescapeNode(Node* insertSection)
{
  const std::string* code = insertSection->Attr("code");
  return Unescape(code ? *code : std::string());
}

bool IsDirector(Node* method)
{
  Node* clazz = FindClassNode(method);
  if (clazz == nullptr || clazz->Attr("feature_director") == nullptr)
    return false;
  if (method->Name() == "destructor")
    return false;
  if (method->Name() == "constructor")
    return false;
  const std::string* storage = method->Attr("storage");
  return storage != nullptr && *storage == "virtual";
}

bool IsKnownBaseType(const std::string& typ, Node* searchFrom)
{
  // hasFeatureSetting(typ, searchFrom, "feature_knownbasetypes",
  //   lambda attr: any(x.strip() == typ for x in attr.split(",")))
  Node* node = searchFrom;
  while (node != nullptr)
  {
    const std::string* attr = node->Attr("feature_knownbasetypes");
    if (attr != nullptr && !attr->empty())
    {
      for (const std::string& x : SplitComma(*attr))
        if (Strip(x) == typ)
          return true;
    }
    node = node->Parent();
  }
  return false;
}

std::string IsKnownApiType(const std::string& typ, Node* searchFrom, bool& matched)
{
  const std::string rootType = GetRootType(typ);
  std::string namespaceStr = FindNamespace(searchFrom, "::", false);
  std::string lastMatch;
  bool haveMatch = false;

  // hasFeatureSetting walks up the parent chain; the predicate mutates
  // namespaceStr/lastMatch across calls exactly as the Python closure did.
  Node* node = searchFrom;
  while (node != nullptr && !haveMatch)
  {
    const std::string* attr = node->Attr("feature_knownapitypes");
    if (attr != nullptr && !attr->empty())
    {
      bool predTrue = false;
      for (const std::string& entryRaw : SplitComma(*attr))
      {
        const std::string entry = entryRaw;
        if (Strip(entry) == rootType)
        {
          lastMatch = rootType;
          haveMatch = true;
          predTrue = true;
          break;
        }
        // assume 'type' is defined within namespace; walk up appending the type.
        while (namespaceStr != "")
        {
          if ((namespaceStr + "::" + rootType) == Strip(entry))
          {
            lastMatch = Strip(entry);
            haveMatch = true;
            predTrue = true;
            break;
          }
          const long chop = RFindSep(namespaceStr);
          namespaceStr = chop > 0 ? namespaceStr.substr(0, static_cast<size_t>(chop)) : std::string();
        }
        if (predTrue)
          break;
      }
      if (predTrue)
        break;
    }
    node = node->Parent();
  }

  matched = haveMatch;
  return lastMatch;
}

std::string IsKnownApiType(const std::string& typ, Node* searchFrom)
{
  bool m = false;
  return IsKnownApiType(typ, searchFrom, m);
}

} // namespace swigbindings
