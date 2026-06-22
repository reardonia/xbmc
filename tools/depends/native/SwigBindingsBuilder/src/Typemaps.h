/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

// Port of typemaps.py (the 12 xbmc/interfaces/python/typemaps/*.{in,out}tm
// mini-templates). Each produces the EXACT byte stream the Groovy
// SimpleTemplateEngine produced (leading/trailing whitespace included), since
// the goldens are raw (un-clang-formatted) output. Recursive typemaps call back
// into Helper::GetIn/GetOutConversion and thread the shared sequence so the
// generated temporary names (entryN / pyentryN) match the reference exactly.

#include <string>

#include "Helper.h"

namespace swigbindings
{

std::string StringOut(const Bindings& b);
std::string BufferOut(const Bindings& b);
std::string BufferIn(const Bindings& b);
std::string MapIn(const Bindings& b);
std::string DictIn(const Bindings& b);
std::string VectorIn(const Bindings& b);
std::string VectorOut(const Bindings& b);
std::string TupleIn(const Bindings& b);
std::string TupleOut(const Bindings& b);
std::string AlternativeIn(const Bindings& b);
std::string AlternativeOut(const Bindings& b);
std::string SmartPtrOut(const Bindings& b);

} // namespace swigbindings
