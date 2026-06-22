/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Transform.h"

#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "tinyxml2.h"

using tinyxml2::XMLAttribute;
using tinyxml2::XMLElement;

namespace swigbindings
{

namespace
{

const std::set<std::string> kIgnoreAttributes = {
    "classes",          "symtab",
    "sym_symtab",       "sym_overname",
    "options",          "sym_nextSibling",
    "csym_nextSibling", "sym_previousSibling",
};

bool EndsWith(const std::string& s, const std::string& suffix)
{
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool StartsWith(const std::string& s, const std::string& prefix)
{
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
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

std::string Tag(const XMLElement* el)
{
  const char* n = el->Name();
  return n ? std::string(n) : std::string();
}

// value of <attribute name=.. value=..> directly under el's attributelist(s)
const char* ElAttr(const XMLElement* el, const std::string& name)
{
  for (const XMLElement* al = el->FirstChildElement(); al != nullptr; al = al->NextSiblingElement())
  {
    if (Tag(al) == "attributelist")
    {
      for (const XMLElement* a = al->FirstChildElement(); a != nullptr; a = a->NextSiblingElement())
      {
        if (Tag(a) == "attribute")
        {
          const char* an = a->Attribute("name");
          if (an != nullptr && name == an)
            return a->Attribute("value");
        }
      }
    }
  }
  return nullptr;
}

// drop the auto-included swig.swg include subtree and typescope/typetab items
bool KeepNode(const XMLElement* el)
{
  const std::string tag = Tag(el);
  if (tag == "include")
  {
    const char* nm = ElAttr(el, "name");
    if (nm != nullptr && EndsWith(std::string(nm), "swig.swg"))
      return false;
  }
  if (tag == "typescopesitem" || tag == "typetabsitem")
    return false;
  return true;
}

bool KeepAttr(const std::string& key)
{
  return kIgnoreAttributes.find(key) == kIgnoreAttributes.end();
}

// _transform_attribute_list: split <attribute> entries from inner element nodes.
void TransformAttributeList(const XMLElement* attributeList,
                            std::vector<std::pair<std::string, std::string>>& attrs,
                            std::vector<const XMLElement*>& nodes)
{
  for (const XMLElement* it = attributeList->FirstChildElement(); it != nullptr;
       it = it->NextSiblingElement())
  {
    if (Tag(it) == "attribute")
    {
      const char* nm = it->Attribute("name");
      const char* val = it->Attribute("value");
      attrs.emplace_back(nm ? nm : std::string(), val ? val : std::string());
    }
    else
    {
      nodes.push_back(it);
    }
  }
}

std::unique_ptr<Node> Transform(const XMLElement* el)
{
  // attributes accumulated in Python insertion order.
  std::vector<std::pair<std::string, std::string>> attributes;
  auto setAttr = [&attributes](const std::string& k, const std::string& v) {
    for (auto& a : attributes)
    {
      if (a.first == k)
      {
        a.second = v;
        return;
      }
    }
    attributes.emplace_back(k, v);
  };
  auto hasAttr = [&attributes](const std::string& k) {
    for (const auto& a : attributes)
      if (a.first == k)
        return true;
    return false;
  };
  auto getAttr = [&attributes](const std::string& k) -> const std::string* {
    for (const auto& a : attributes)
      if (a.first == k)
        return &a.second;
    return nullptr;
  };

  std::vector<std::unique_ptr<Node>> childNodes;

  for (const XMLElement* it = el->FirstChildElement(); it != nullptr; it = it->NextSiblingElement())
  {
    if (KeepNode(it))
    {
      if (Tag(it) == "attributelist")
      {
        std::vector<std::pair<std::string, std::string>> rawAttrs;
        std::vector<const XMLElement*> inner;
        TransformAttributeList(it, rawAttrs, inner);
        for (const auto& kv : rawAttrs)
          if (KeepAttr(kv.first))
            setAttr(kv.first, kv.second);
        for (const XMLElement* c : inner)
          if (KeepNode(c))
            childNodes.push_back(Transform(c));
      }
      else
      {
        childNodes.push_back(Transform(it));
      }
    }
  }

  // transfer element addr -> id (raw swig nodes carry id/addr as XML attrs)
  const char* addr = el->Attribute("addr");
  if (addr != nullptr && addr[0] != '\0')
  {
    for (const XMLAttribute* a = el->FirstAttribute(); a != nullptr; a = a->Next())
    {
      const std::string k = a->Name();
      if (k != "addr" && k != "id")
        setAttr(k, a->Value() ? a->Value() : std::string());
    }
    setAttr("id", addr);
  }

  // cdecl gets renamed to its 'kind'
  std::unique_ptr<Node> ret;
  if (Tag(el) == "cdecl" && hasAttr("kind"))
  {
    const std::string kind = *getAttr("kind");
    ret = std::make_unique<Node>(kind);
    for (const auto& a : attributes)
      if (a.first != "kind")
        ret->SetAttr(a.first, a.second);
  }
  else
  {
    ret = std::make_unique<Node>(Tag(el));
    for (const auto& a : attributes)
      ret->SetAttr(a.first, a.second);
  }

  for (auto& c : childNodes)
    ret->Append(std::move(c));
  return ret;
}

void Flatten(Node* node, const std::set<std::string>& remove)
{
  bool done = false;
  while (!done)
  {
    done = true;
    for (Node* child : node->BreadthFirst())
    {
      if (remove.find(child->Name()) != remove.end())
      {
        Node* parent = child->Parent();
        // Groovy: parent.remove(child); child.children().each { parent.append(it) }.
        // Detach the wrapper, then re-home its children onto the parent in order.
        std::unique_ptr<Node> owned = parent->Detach(child);
        for (Node* gc : MutableChildrenPtrs(owned.get()))
          parent->Append(owned->Detach(gc));
        done = false;
        break;
      }
    }
  }
}

std::string FindNamespace(Node* node)
{
  // _find_namespace with defaults: separator "::", endingSeparator True.
  const std::string separator = "::";
  std::string ret;
  bool have = false;
  for (Node* p : Parents(node, [](Node* n) { return n->Name() == "namespace"; }))
  {
    const std::string* data = p->Attr("name");
    const std::string d = data ? *data : std::string();
    if (!have)
    {
      ret = d;
      have = true;
    }
    else
    {
      ret = ret + separator + d;
    }
  }
  if (!have)
    return std::string();
  return ret + separator;
}

void FunctionNodesByOverloadsAssert(Node* module)
{
  // build map key -> count; assert every list has exactly one entry.
  std::vector<std::pair<std::string, int>> counts;
  for (Node* it : module->DepthFirst())
  {
    const std::string& nm = it->Name();
    if (nm == "function" || nm == "constructor" || nm == "destructor")
    {
      const std::string* ov = it->Attr("sym_overloaded");
      const std::string* idp = it->Attr("id");
      const std::string key = ov ? *ov : (idp ? *idp : std::string());
      bool found = false;
      for (auto& c : counts)
      {
        if (c.first == key)
        {
          c.second += 1;
          found = true;
          break;
        }
      }
      if (!found)
        counts.emplace_back(key, 1);
    }
  }
  for (const auto& c : counts)
  {
    if (c.second != 1)
      throw std::runtime_error(
          "Cannot handle overloaded methods unless simply using defaulting");
  }
}

} // namespace

std::unique_ptr<Node> TransformSwigXml(const XMLElement* swigxml)
{
  std::unique_ptr<Node> node = Transform(swigxml);

  std::vector<Node*> includes = node->Kids("include");
  if (!(includes.size() == 1 && includes[0]->Kids("module").size() == 1 &&
        includes[0]->Kids("module")[0]->Attr("name") != nullptr))
  {
    throw std::runtime_error(
        "Invalid xml: expected a single 'include' child with a single 'module' child");
  }

  const std::string moduleName = *includes[0]->Kids("module")[0]->Attr("name");
  auto ret = std::make_unique<Node>("module");
  ret->SetAttr("name", moduleName);

  // move every non-module child of the include up into the module node.
  for (Node* c : MutableChildrenPtrs(includes[0]))
  {
    if (c->Name() != "module")
      ret->Append(includes[0]->Detach(c));
  }

  Flatten(ret.get(), {"include", "parmlist", "typescope"});

  // Detached subtrees are kept alive here until the end of transform. The
  // Python keeps removed nodes alive (GC) so the snapshot loops below may still
  // hold pointers into a removed subtree; graveyarding preserves that safety.
  std::vector<std::unique_ptr<Node>> graveyard;

  // remove function/constructor nodes with default arguments
  for (Node* cur : ret->DepthFirst())
  {
    const std::string& nm = cur->Name();
    if ((nm == "function" || nm == "constructor") && cur->Attr("defaultargs") != nullptr)
      graveyard.push_back(cur->Parent()->Detach(cur));
  }

  // no remaining overloads may exist (defaulting is the only allowed form)
  FunctionNodesByOverloadsAssert(ret.get());

  // collapse all typetabs into a single deduplicated <typetab> of <entry>
  std::vector<Node*> allTypetabs;
  for (Node* n : ret->DepthFirst())
    if (n->Name() == "typetab")
      allTypetabs.push_back(n);

  Node* typenode = ret->Append(std::make_unique<Node>("typetab"));
  for (Node* tt : allTypetabs)
  {
    // iterate a snapshot of (key,value) attribute pairs
    const std::vector<std::pair<std::string, std::string>> attrPairs = tt->Attributes();
    for (const auto& kv : attrPairs)
    {
      const std::string& key = kv.first;
      const std::string& value = kv.second;
      if (key != "id" && key != value)
      {
        const std::string namespaceStr = FindNamespace(tt);
        auto entry = std::make_unique<Node>("entry");
        entry->SetAttr("namespace", Strip(namespaceStr));
        entry->SetAttr("type", key);
        entry->SetAttr("basetype", value);
        const std::string* eb = entry->Attr("basetype");
        const std::string* en = entry->Attr("namespace");
        Node* existing = typenode->Find([&](Node* e) {
          const std::string* ebt = e->Attr("basetype");
          const std::string* ens = e->Attr("namespace");
          const bool bteq = (ebt == nullptr && eb == nullptr) || (ebt && eb && *ebt == *eb);
          const bool nseq = (ens == nullptr && en == nullptr) || (ens && en && *ens == *en);
          return bteq && nseq;
        });
        if (existing == nullptr)
          typenode->Append(std::move(entry));
      }
    }
    if (tt->Parent() != nullptr)
      graveyard.push_back(tt->Parent()->Detach(tt));
  }

  // remove non-public functions/destructors (keep constructors); doc omitted
  {
    std::vector<Node*> targets;
    for (Node* n : ret->DepthFirst())
    {
      const std::string& nm = n->Name();
      if (nm == "function" || nm == "destructor" || nm == "constructor")
        targets.push_back(n);
    }
    for (Node* it : targets)
    {
      const std::string* access = it->Attr("access");
      if (access != nullptr && *access != "public" && it->Name() != "constructor")
        graveyard.push_back(it->Parent()->Detach(it));
    }
  }

  // remove non-public variables
  {
    std::vector<Node*> targets;
    for (Node* n : ret->DepthFirst())
      if (n->Name() == "variable")
        targets.push_back(n);
    for (Node* it : targets)
    {
      const std::string* access = it->Attr("access");
      if (access != nullptr && *access != "public")
        graveyard.push_back(it->Parent()->Detach(it));
    }
  }

  return ret;
}

} // namespace swigbindings
