/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

// Port of the template-facing half of helper.py (Helper.groovy). Holds the
// in/out type-conversion dispatch (GetOutConversion / GetInConversion), the
// Sequence counter that names recursive-typemap temporaries, the typemap
// registry (set via Setup), and the Node-walking helpers the emitter and
// typemaps call.

#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "Node.h"

namespace swigbindings
{

// Sequence: names recursive-typemap temporaries (entryN / pyentryN). A shared
// instance is threaded through one top-level conversion so nested typemaps see
// a monotonically increasing counter, exactly as the Groovy Sequence did.
class Sequence
{
public:
  int Increment() { return ++m_cur; }

private:
  int m_cur = 0;
};

// The binding values a typemap callable reads. Mirrors the Python `bindings`
// dict. Only the fields a given typemap touches are populated; the rest keep
// their defaults. `sequence` points at the active shared Sequence.
struct Bindings
{
  std::string result;
  std::string api;
  std::string type;
  std::string ltype;
  std::string slarg;
  std::string param;
  std::string classname;
  Node* method = nullptr;
  Sequence* sequence = nullptr;
};

using TypemapFn = std::function<std::string(const Bindings&)>;

// The optional per-call override the Python passed as a dict and merged with
// `bindings.update(...)`. Only the keys actually used across the typemaps and
// the emitter are modeled: result, api, type, ltype, and the shared sequence.
struct OverrideBindings
{
  std::optional<std::string> result;
  std::optional<std::string> api;
  std::optional<std::string> type;
  std::optional<std::string> ltype;
  Sequence* sequence = nullptr; // non-null means "override the sequence"
};

struct PatternEntry
{
  std::regex rx;
  TypemapFn fn;
};

void Setup(const std::vector<Node*>& classes,
           const std::vector<std::pair<std::string, TypemapFn>>& outExact,
           const std::vector<PatternEntry>& outPatterns,
           TypemapFn defaultOut,
           const std::vector<std::pair<std::string, TypemapFn>>& inExact,
           const std::vector<PatternEntry>& inPatterns,
           TypemapFn defaultIn);

// out conversion. overrideBindings carries the optional per-call overrides the
// Python passed as a dict; nullptr means "no overrides".
std::string GetOutConversion(const std::string& apiType,
                             const std::string& apiName,
                             Node* method,
                             const OverrideBindings* overrideBindings = nullptr,
                             bool recurse = true);

// in conversion. paramName == apiName for every call site used by the emitter.
std::string GetInConversion(const std::string& apiType,
                            const std::string& apiName,
                            const std::string& slName,
                            Node* method,
                            const OverrideBindings* overrideBindings = nullptr);

// node helpers
bool HasDefinedConstructor(Node* clazz);
bool HasDoc(Node* methodOrClass);
bool HasHiddenConstructor(Node* clazz);
Node* FindClassNodeByName(Node* module, const std::string& classname, Node* referenceNode = nullptr);
Node* FindClassNode(Node* node);
// FindFullClassName returns whether a name exists (the Python returned None on
// miss). `hasValue` is set false when there is no class context.
std::string FindFullClassName(Node* node, bool& hasValue);
std::string FindFullClassName(Node* node); // "" when absent
std::string FindNamespace(Node* node,
                          const std::string& separator = "::",
                          bool endingSeparator = true,
                          bool filename = false);
std::string GetPropertyReturnSwigType(Node* method);
std::string GetReturnSwigType(Node* method);
std::string CallingName(Node* method);
std::vector<Node*> GetInsertNodes(Node* module, const std::string& section);
std::string Unescape(const std::string& insertSection);
std::string UnescapeNode(Node* insertSection);
bool IsDirector(Node* method);
bool IsKnownBaseType(const std::string& typ, Node* searchFrom);
// IsKnownApiType returns the matched type string, or empty (Python None/"" was
// falsy and both behave the same at every call site).
std::string IsKnownApiType(const std::string& typ, Node* searchFrom, bool& matched);
std::string IsKnownApiType(const std::string& typ, Node* searchFrom);

} // namespace swigbindings
