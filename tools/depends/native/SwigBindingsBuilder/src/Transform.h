/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

// Port of transform.py (Helper.transformSwigXml + helpers). Input is the raw
// `swig -xml` DOM as parsed by tinyxml2 (Kodi's own XML library); output is a
// model Node tree rooted at <module>, normalized exactly the way the reference
// generator normalizes it. tinyxml2 decodes XML entities (&lt; &#10; etc.) the
// same way Python's xml.etree did, so attribute byte streams match.

#include <memory>

#include "Node.h"

namespace tinyxml2
{
class XMLElement;
}

namespace swigbindings
{

std::unique_ptr<Node> TransformSwigXml(const tinyxml2::XMLElement* root);

} // namespace swigbindings
