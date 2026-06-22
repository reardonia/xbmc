/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Node.h"

namespace swigbindings
{

std::vector<Node*> Parents(Node* node, const std::function<bool(Node*)>& filt)
{
  std::vector<Node*> out;
  Node* p = node->Parent();
  if (p != nullptr)
  {
    out = Parents(p, filt);
    if (!filt || filt(p))
      out.push_back(p);
  }
  return out;
}

std::vector<Node*> MutableChildrenPtrs(Node* node)
{
  std::vector<Node*> out;
  for (const auto& c : node->Children())
    out.push_back(c.get());
  return out;
}

} // namespace swigbindings
