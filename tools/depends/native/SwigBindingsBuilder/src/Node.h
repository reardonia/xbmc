/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

// Port of node.py (Groovy groovy.util.Node stand-in) for the *transformed*
// model tree. Semantics deliberately match the Python/Groovy Node:
//   - kids(tag) / find / findAll iterate DIRECT CHILDREN
//   - depthFirst iterates DESCENDANTS (self first, pre-order)
//   - breadthFirst iterates DESCENDANTS (self first, BFS)
//   - new Node(parent, name, attrs) auto-appends to parent
//   - attr(x) returns the value or nullptr on miss (tri-state, matches None)
// Attribute *insertion order* is preserved: emit.py iterates
// attributes.keys()/items() and the byte stream depends on that order.

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace swigbindings
{

class Node
{
public:
  Node() = default;
  explicit Node(std::string name) : m_name(std::move(name)) {}

  // identity / attributes ----------------------------------------------------
  const std::string& Name() const { return m_name; }
  void SetName(const std::string& name) { m_name = name; }

  // attr(key): value or nullptr on miss.
  const std::string* Attr(const std::string& key) const
  {
    for (const auto& a : m_attributes)
      if (a.first == key)
        return &a.second;
    return nullptr;
  }

  // SetAttr: insert or overwrite, preserving first-insertion position.
  void SetAttr(const std::string& key, const std::string& value)
  {
    for (auto& a : m_attributes)
    {
      if (a.first == key)
      {
        a.second = value;
        return;
      }
    }
    m_attributes.emplace_back(key, value);
  }

  bool HasAttr(const std::string& key) const { return Attr(key) != nullptr; }

  const std::vector<std::pair<std::string, std::string>>& Attributes() const
  {
    return m_attributes;
  }

  // structure ----------------------------------------------------------------
  Node* Parent() const { return m_parent; }

  const std::vector<std::unique_ptr<Node>>& Children() const { return m_children; }

  // Append takes ownership; returns the raw pointer for chaining/reads.
  Node* Append(std::unique_ptr<Node> child)
  {
    child->m_parent = this;
    Node* raw = child.get();
    m_children.push_back(std::move(child));
    return raw;
  }

  // Detach a child and return ownership of it (so callers can re-append it
  // elsewhere, matching Groovy parent.remove(child) + parent.append(child)).
  std::unique_ptr<Node> Detach(Node* child)
  {
    for (size_t i = 0; i < m_children.size(); ++i)
    {
      if (m_children[i].get() == child)
      {
        std::unique_ptr<Node> owned = std::move(m_children[i]);
        m_children.erase(m_children.begin() + static_cast<long>(i));
        owned->m_parent = nullptr;
        return owned;
      }
    }
    return nullptr;
  }

  // children-scoped iteration ------------------------------------------------
  std::vector<Node*> Kids(const std::string& tag) const
  {
    std::vector<Node*> out;
    for (const auto& c : m_children)
      if (c->m_name == tag)
        out.push_back(c.get());
    return out;
  }

  Node* Find(const std::function<bool(Node*)>& pred) const
  {
    for (const auto& c : m_children)
      if (pred(c.get()))
        return c.get();
    return nullptr;
  }

  std::vector<Node*> FindAll(const std::function<bool(Node*)>& pred) const
  {
    std::vector<Node*> out;
    for (const auto& c : m_children)
      if (pred(c.get()))
        out.push_back(c.get());
    return out;
  }

  // descendant iteration -----------------------------------------------------
  std::vector<Node*> DepthFirst()
  {
    std::vector<Node*> out;
    DepthFirstInto(out);
    return out;
  }

  std::vector<Node*> BreadthFirst()
  {
    std::vector<Node*> out;
    std::vector<Node*> q{this};
    size_t head = 0;
    while (head < q.size())
    {
      Node* n = q[head++];
      out.push_back(n);
      for (const auto& c : n->m_children)
        q.push_back(c.get());
    }
    return out;
  }

private:
  void DepthFirstInto(std::vector<Node*>& out)
  {
    out.push_back(this);
    for (const auto& c : m_children)
      c->DepthFirstInto(out);
  }

  std::string m_name;
  std::vector<std::pair<std::string, std::string>> m_attributes;
  std::vector<std::unique_ptr<Node>> m_children;
  Node* m_parent = nullptr;
};

// Helper.parents: ancestors ordered top-down (root-most ... immediate parent),
// optionally filtered. Recursive, to match the Groovy ordering.
std::vector<Node*> Parents(Node* node, const std::function<bool(Node*)>& filt = nullptr);

// Raw pointers to a node's direct children, in order. Used where the transform
// needs to walk-and-detach children (Groovy iterated list(children())).
std::vector<Node*> MutableChildrenPtrs(Node* node);

} // namespace swigbindings
