/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

// Port of emit.py (xbmc/interfaces/python/PythonSwig.cpp.template). Reproduces
// the raw (un-clang-formatted) byte stream the Groovy SimpleTemplateEngine
// produced. Whitespace is significant; literal spans are copied verbatim from
// the template and computed values spliced in at the same points.

#include <string>

#include "Node.h"

namespace swigbindings
{

std::string Generate(Node* module);

} // namespace swigbindings
