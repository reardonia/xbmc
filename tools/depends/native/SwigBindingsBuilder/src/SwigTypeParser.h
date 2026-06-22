/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

// Port of swig_type_parser.py (itself a port of SwigTypeParser.groovy / SWIG's
// Source/Swig/stype.c). These transliterate SWIG's type-mangling routines. The
// goal is behavioural identity with the reference generator, not idiomatic C++:
// the control flow (including parts that look redundant) is kept as close to the
// original as the languages allow. Do not "clean this up"; the only oracle that
// matters is byte-identical output.
//
// Python string-op mapping reproduced here:
//   t[a:b]      -> PySlice(t, a, b)   (clamps a/b into [0,len], a>b => "")
//   t[a:]       -> PySlice(t, a)
//   t.find(x)   -> PyFind(t, x)       (-1 when absent, same as Java indexOf)
//   t.startswith(x) -> StartsWith(t, x)

#include <string>
#include <vector>

namespace swigbindings
{

// An "entry" of the typedef table: (namespace + type) -> basetype.
struct TypeTableEntry
{
  std::string namespaceStr;
  std::string type;
  std::string basetype;
};

// typedef table -------------------------------------------------------------
void ResetTypeTable();
void AppendTypeTable(const std::vector<TypeTableEntry>& entries);

// public conversions --------------------------------------------------------
std::string ConvertTypeToLTypeForParam(const std::string& ty);
std::string GetRootType(const std::string& ty);
std::string SwigType_str(const std::string& ty, const std::string& id);
std::string SwigType_str(const std::string& ty); // id defaults to ""
std::string SwigType_typedef_resolve(const std::string& t);
std::string SwigType_resolve_all_typedefs(const std::string& s);
std::string SwigType_ltype(const std::string& s);
std::string SwigType_lrtype(const std::string& s);
std::string SwigType_lstr(const std::string& type);

// predicates / accessors ----------------------------------------------------
bool SwigType_ispointer(const std::string& t);
std::string SwigType_makepointer(const std::string& t);
bool SwigType_isarray(const std::string& t);
bool SwigType_ismemberpointer(const std::string& t);
bool SwigType_isqualifier(const std::string& t);
bool SwigType_isreference(const std::string& t);
bool SwigType_isenum(const std::string& t);
bool SwigType_istemplate(const std::string& t);
bool SwigType_isfunction(const std::string& t);
bool SwigType_isconst(const std::string& t);

// internal splitters / parsers ----------------------------------------------
// SwigType_parm returns the qualifier/parm text; HasValue is false when the
// Python returned None (no '(' found).
std::string SwigType_parm(const std::string& t, bool& hasValue);
std::string SwigType_parm(const std::string& t); // None -> "" (callers guard)
std::vector<std::string> SwigType_templateparmlist(const std::string& t);
std::vector<std::string> SwigType_parmlist(const std::string& p);
std::string SwigType_namestr(const std::string& t);
std::string SwigType_templatesuffix(const std::string& t);
std::vector<std::string> SwigType_split(const std::string& t);
int ElementSize(const std::string& s);
// SwigType_pop returns (element, remainder).
std::pair<std::string, std::string> SwigType_pop(const std::string& t);
bool SwigType_issimple(const std::string& t);

} // namespace swigbindings
