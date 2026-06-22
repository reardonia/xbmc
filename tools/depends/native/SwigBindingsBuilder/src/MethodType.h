/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

// Port of method_type.py / MethodType.groovy (enum {constructor, destructor,
// method}). Call sites only compare identity, so a plain enum suffices.

namespace swigbindings
{

enum class MethodType
{
  Constructor,
  Destructor,
  Method,
};

} // namespace swigbindings
