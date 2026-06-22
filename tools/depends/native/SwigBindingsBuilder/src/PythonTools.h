/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

// Port of python_tools.py (PythonTools.groovy). Python-specific helpers used by
// the emitter: PyArg format-char mapping, the PyArg_ParseTupleAndKeywords
// format string, class-name-as-variable mangling, the generated CPython method
// names, docstrings, and base-class resolution.

#include <string>

#include "Node.h"
#include "MethodType.h"

namespace swigbindings
{

bool ParameterCanBeUsedDirectly(Node* param);
std::string MakeFormatStringFromParameters(Node* method);
std::string GetClassNameAsVariable(Node* clazz);
std::string GetPyMethodName(Node* method, MethodType methodType);
std::string MakeDocString(Node* docnode);
// FindValidBaseClass returns nullptr when there is no known base class.
Node* FindValidBaseClass(Node* clazz, Node* module, bool warn = false);

} // namespace swigbindings
