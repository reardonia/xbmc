/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "SwigTypeParser.h"

#include <map>

namespace swigbindings
{

namespace
{

// --- Python string-op emulation -------------------------------------------
// Python t[a:b] clamps a and b into [0,len] and yields "" when a>=b. Negative
// indices do not occur in this port, so only the non-negative clamp is modeled.
std::string PySlice(const std::string& t, long a, long b)
{
  const long n = static_cast<long>(t.size());
  if (a < 0)
    a = 0;
  if (b > n)
    b = n;
  if (a > b || a >= n)
    return std::string();
  return t.substr(static_cast<size_t>(a), static_cast<size_t>(b - a));
}

std::string PySlice(const std::string& t, long a)
{
  return PySlice(t, a, static_cast<long>(t.size()));
}

// Python str.find / Java indexOf: index of first occurrence, or -1.
long PyFind(const std::string& t, const std::string& sub)
{
  const size_t p = t.find(sub);
  return p == std::string::npos ? -1 : static_cast<long>(p);
}

long PyFind(const std::string& t, const std::string& sub, long from)
{
  if (from < 0)
    from = 0;
  if (from > static_cast<long>(t.size()))
    return -1;
  const size_t p = t.find(sub, static_cast<size_t>(from));
  return p == std::string::npos ? -1 : static_cast<long>(p);
}

long PyFind(const std::string& t, char ch)
{
  const size_t p = t.find(ch);
  return p == std::string::npos ? -1 : static_cast<long>(p);
}

long PyRFind(const std::string& t, char ch)
{
  const size_t p = t.rfind(ch);
  return p == std::string::npos ? -1 : static_cast<long>(p);
}

bool StartsWith(const std::string& t, const std::string& prefix)
{
  return t.size() >= prefix.size() && t.compare(0, prefix.size(), prefix) == 0;
}

std::string ReplaceAll(std::string s, const std::string& from, const std::string& to)
{
  if (from.empty())
    return s;
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos)
  {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
  return s;
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

// --- typedef table ---------------------------------------------------------
std::map<std::string, std::string> g_typeTable;

} // namespace

void ResetTypeTable()
{
  g_typeTable.clear();
}

void AppendTypeTable(const std::vector<TypeTableEntry>& entries)
{
  for (const auto& e : entries)
    g_typeTable[e.namespaceStr + e.type] = e.basetype;
}

// --- public conversions ----------------------------------------------------
std::string ConvertTypeToLTypeForParam(const std::string& tyIn)
{
  const std::string ty = Strip(tyIn);
  // an r.* parameter is assumed to be passed by value on the stack
  if (StartsWith(ty, "r."))
    return SwigType_ltype(PySlice(ty, 2));
  return SwigType_ltype(ty);
}

std::string GetRootType(const std::string& ty)
{
  const long li = PyRFind(ty, '.');
  return li >= 0 ? PySlice(ty, li + 1) : ty;
}

std::string SwigType_str(const std::string& ty, const std::string& id)
{
  std::string result = id;
  std::vector<std::string> elements = SwigType_split(ty);
  const size_t nelements = elements.size();
  std::string element = nelements > 0 ? elements[0] : std::string();

  for (size_t i = 0; i < nelements; ++i)
  {
    std::string nextelement;
    std::string forwardelement;
    bool haveNext = false;
    bool haveForward = false;
    if (i < (nelements - 1))
    {
      nextelement = elements[i + 1];
      haveNext = true;
      forwardelement = nextelement;
      haveForward = true;
      if (StartsWith(nextelement, "q("))
      {
        if (i < (nelements - 2))
          forwardelement = elements[i + 2];
      }
    }
    else
    {
      haveNext = false;
      haveForward = false;
    }

    if (StartsWith(element, "q("))
    {
      const std::string q = SwigType_parm(element);
      result = q + " " + result;
    }
    else if (SwigType_ispointer(element))
    {
      result = "*" + result;
      if (haveForward && (SwigType_isfunction(forwardelement) || SwigType_isarray(forwardelement)))
        result = "(" + result + ")";
    }
    else if (SwigType_ismemberpointer(element))
    {
      const std::string q = SwigType_parm(element);
      result = q + "::*" + result;
      if (haveForward && (SwigType_isfunction(forwardelement) || SwigType_isarray(forwardelement)))
        result = "(" + result + ")";
    }
    else if (SwigType_isreference(element))
    {
      result = "&" + result;
      if (haveForward && (SwigType_isfunction(forwardelement) || SwigType_isarray(forwardelement)))
        result = "(" + result + ")";
    }
    else if (SwigType_isarray(element))
    {
      result += "[" + SwigType_parm(element) + "]";
    }
    else if (SwigType_isfunction(element))
    {
      result += "(";
      const std::vector<std::string> parms = SwigType_parmlist(element);
      bool didOne = false;
      for (const auto& cur : parms)
      {
        const std::string p = SwigType_str(cur);
        result += (didOne ? "," : "") + p;
        didOne = true;
      }
      result += ")";
    }
    else
    {
      if (StartsWith(element, "v(...)"))
      {
        result = result + "...";
      }
      else
      {
        const std::string bs = SwigType_namestr(element);
        result = bs + " " + result;
      }
    }

    if (haveNext)
      element = nextelement;
    else
      element.clear();
  }

  // convert template parameters: '<(' -> '<', ')>' -> '>'
  result = ReplaceAll(result, "<(", "<");
  result = ReplaceAll(result, ")>", ">");
  return result;
}

std::string SwigType_str(const std::string& ty)
{
  return SwigType_str(ty, std::string());
}

std::string SwigType_typedef_resolve(const std::string& t)
{
  const auto it = g_typeTable.find(t);
  return it == g_typeTable.end() ? t : it->second;
}

std::string SwigType_resolve_all_typedefs(const std::string& s)
{
  std::string result;
  std::string tc = s;

  // nuke all leading qualifiers, appending them to the result
  while (SwigType_isqualifier(tc))
  {
    auto popped = SwigType_pop(tc);
    result += popped.first;
    tc = popped.second;
  }

  if (SwigType_issimple(tc))
  {
    // resolve any typedef definitions
    std::string tt = tc;
    std::string td = tt;
    while ((td = SwigType_typedef_resolve(tt)) != tt)
    {
      if (td != tt)
      {
        tt = td;
        break;
      }
      // the elif branch in the Python is unreachable; preserved as a no-op
    }
    tc = td;
    return tc;
  }

  auto popped = SwigType_pop(tc);
  result += popped.first;
  result += SwigType_resolve_all_typedefs(popped.second);
  return result;
}

std::string SwigType_ltype(const std::string& s)
{
  std::string result;
  std::string tc = s;

  // nuke all leading qualifiers
  while (SwigType_isqualifier(tc))
    tc = SwigType_pop(tc).second;

  if (SwigType_issimple(tc))
  {
    // resolve any typedef definitions
    std::string tt = tc;
    std::string td = tt;
    while ((td = SwigType_typedef_resolve(tt)) != tt)
    {
      if ((td != tt) && (SwigType_isconst(td) || SwigType_isarray(td) || SwigType_isreference(td)))
      {
        tt = td;
        break;
      }
      else if (td != tt)
      {
        tt = td;
      }
    }
    tc = td;
  }

  std::vector<std::string> elements = SwigType_split(tc);
  const size_t nelements = elements.size();

  bool notypeconv = false;
  bool firstarray = true;
  for (size_t i = 0; i < nelements; ++i)
  {
    const std::string& element = elements[i];
    // when we see a function, preserve the following types
    if (SwigType_isfunction(element))
      notypeconv = true;
    if (SwigType_isqualifier(element))
    {
      // ignore
    }
    else if (SwigType_ispointer(element))
    {
      result += element;
      // short circuit: collapse the rest of the list and recurse
      std::string tmps;
      for (size_t j = i + 1; j < nelements; ++j)
        tmps += elements[j];
      return result + SwigType_ltype(tmps);
    }
    else if (SwigType_ismemberpointer(element))
    {
      result += element;
      firstarray = false;
    }
    else if (SwigType_isreference(element))
    {
      if (notypeconv)
        result += element;
      else
        result += "p.";
      firstarray = false;
    }
    else if (SwigType_isarray(element) && firstarray)
    {
      if (notypeconv)
        result += element;
      else
        result += "p.";
      firstarray = false;
    }
    else if (SwigType_isenum(element))
    {
      const bool anonymous_enum = (element == "enum ");
      if (notypeconv || !anonymous_enum)
        result += element;
      else
        result += "int";
    }
    else
    {
      result += element;
    }
  }

  return result;
}

std::string SwigType_lrtype(const std::string& s)
{
  const std::string ltype = SwigType_ltype(s);
  if (SwigType_ispointer(s))
    return ltype;
  return "r." + ltype;
}

std::string SwigType_lstr(const std::string& type)
{
  return SwigType_str(ConvertTypeToLTypeForParam(type));
}

// --- predicates / small accessors ------------------------------------------
bool SwigType_ispointer(const std::string& tIn)
{
  std::string t = tIn;
  if (StartsWith(t, "q("))
    t = PySlice(t, PyFind(t, '.') + 1);
  return StartsWith(t, "p.");
}

std::string SwigType_makepointer(const std::string& t)
{
  std::string prefix;
  std::string remainder;
  if (StartsWith(t, "q("))
  {
    prefix = PySlice(t, 0, PyFind(t, '.') + 1);
    remainder = PySlice(t, PyFind(t, '.') + 1);
  }
  else
  {
    prefix = "";
    remainder = t;
  }
  return prefix + "p." + remainder;
}

bool SwigType_isarray(const std::string& t)
{
  return StartsWith(t, "a(");
}

bool SwigType_ismemberpointer(const std::string& t)
{
  return !t.empty() && StartsWith(t, "m(");
}

bool SwigType_isqualifier(const std::string& t)
{
  return !t.empty() && StartsWith(t, "q(");
}

bool SwigType_isreference(const std::string& t)
{
  return StartsWith(t, "r.");
}

bool SwigType_isenum(const std::string& t)
{
  return StartsWith(t, "enum");
}

bool SwigType_istemplate(const std::string& t)
{
  const long c = PyFind(t, "<(");
  return c >= 0 && PyFind(t, ")>", c + 2) >= 0;
}

bool SwigType_isfunction(const std::string& tIn)
{
  std::string t = tIn;
  if (StartsWith(t, "q("))
    t = PySlice(t, PyFind(t, '.') + 1);
  return StartsWith(t, "f(");
}

bool SwigType_isconst(const std::string& t)
{
  // Python: t is None -> False. The empty string maps to the same falsy path
  // here; callers never depend on a const-empty-string result.
  if (StartsWith(t, "q("))
  {
    bool hasValue = false;
    const std::string q = SwigType_parm(t, hasValue);
    if (hasValue && PyFind(q, "const") >= 0)
      return true;
  }
  // might be const through a typedef
  if (SwigType_issimple(t))
  {
    const std::string td = SwigType_typedef_resolve(t);
    if (td != t)
      return SwigType_isconst(td);
  }
  return false;
}

// --- internal splitters / parsers ------------------------------------------
std::string SwigType_parm(const std::string& t, bool& hasValue)
{
  long start = PyFind(t, '(');
  if (start < 0)
  {
    hasValue = false;
    return std::string();
  }
  hasValue = true;
  start += 1;
  int nparens = 0;
  long c = start;
  const long len = static_cast<long>(t.size());
  while (c < len)
  {
    if (t[static_cast<size_t>(c)] == ')')
    {
      if (nparens == 0)
        break;
      nparens -= 1;
    }
    else if (t[static_cast<size_t>(c)] == '(')
    {
      nparens += 1;
    }
    c += 1;
  }
  return PySlice(t, start, c);
}

std::string SwigType_parm(const std::string& t)
{
  bool hasValue = false;
  return SwigType_parm(t, hasValue);
}

std::vector<std::string> SwigType_templateparmlist(const std::string& t)
{
  const long i = PyFind(t, '<');
  return SwigType_parmlist(PySlice(t, i));
}

std::vector<std::string> SwigType_parmlist(const std::string& p)
{
  std::vector<std::string> lst;
  // assert p: a null/empty p never reaches here on the build path.
  long itemstart = PyFind(p, '(');
  // assert dot == -1 or dot > itemstart (not enforced; never triggered)
  itemstart += 1;
  long c = itemstart;
  const long len = static_cast<long>(p.size());
  while (c < len)
  {
    if (p[static_cast<size_t>(c)] == ',')
    {
      lst.push_back(PySlice(p, itemstart, c));
      itemstart = c + 1;
    }
    else if (p[static_cast<size_t>(c)] == '(')
    {
      int nparens = 1;
      c += 1;
      while (c < len)
      {
        if (p[static_cast<size_t>(c)] == '(')
          nparens += 1;
        if (p[static_cast<size_t>(c)] == ')')
        {
          nparens -= 1;
          if (nparens == 0)
            break;
        }
        c += 1;
      }
    }
    else if (p[static_cast<size_t>(c)] == ')')
    {
      break;
    }
    if (c < len)
      c += 1;
  }

  if (c != itemstart)
    lst.push_back(PySlice(p, itemstart, c));
  return lst;
}

std::string SwigType_namestr(const std::string& t)
{
  const long c = PyFind(t, "<(");
  if (c < 0 || PyFind(t, ")>", c + 2) < 0)
    return t;

  std::string r = PySlice(t, 0, c);
  if (t[static_cast<size_t>(c - 1)] == '<')
    r += " ";
  r += "<";

  const std::vector<std::string> p = SwigType_parmlist(PySlice(t, c + 1));
  for (size_t i = 0; i < p.size(); ++i)
  {
    const std::string s = SwigType_str(p[i], std::string());
    // avoid creating a '<:' token (same as '[' in C++): space after '<'
    if (i == 0 && s.size() > 0)
      r += " ";
    r += s;
    if ((i + 1) < p.size())
      r += ",";
  }
  r += " >";
  const std::string suffix = SwigType_templatesuffix(t);
  if (suffix.size() > 0)
    r += SwigType_namestr(suffix);
  else
    r += suffix;
  return r;
}

std::string SwigType_templatesuffix(const std::string& t)
{
  long c = 0;
  const long len = static_cast<long>(t.size());
  while (c < len)
  {
    if ((t[static_cast<size_t>(c)] == '<') && (c + 1 < len) && (t[static_cast<size_t>(c + 1)] == '('))
    {
      int nest = 1;
      c += 1;
      while (c < len && nest != 0)
      {
        if (t[static_cast<size_t>(c)] == '<')
          nest += 1;
        if (t[static_cast<size_t>(c)] == '>')
          nest -= 1;
        c += 1;
      }
      return PySlice(t, c);
    }
    c += 1;
  }
  return std::string();
}

std::vector<std::string> SwigType_split(const std::string& t)
{
  std::vector<std::string> lst;
  long c = 0;
  const long len = static_cast<long>(t.size());
  while (c < len)
  {
    const int length = ElementSize(PySlice(t, c));
    const std::string item = PySlice(t, c, c + length);
    lst.push_back(item);
    c = c + length;
    if (c < len && t[static_cast<size_t>(c)] == '.')
      c += 1;
  }
  return lst;
}

int ElementSize(const std::string& s)
{
  long c = 0;
  const long len = static_cast<long>(s.size());
  while (c < len)
  {
    if (s[static_cast<size_t>(c)] == '.')
    {
      c += 1;
      return static_cast<int>(c);
    }
    else if (s[static_cast<size_t>(c)] == '(')
    {
      int nparen = 1;
      c += 1;
      while (c < len)
      {
        if (s[static_cast<size_t>(c)] == '(')
          nparen += 1;
        if (s[static_cast<size_t>(c)] == ')')
        {
          nparen -= 1;
          if (nparen == 0)
            break;
        }
        c += 1;
      }
    }
    if (c < len)
      c += 1;
  }
  return static_cast<int>(c);
}

std::pair<std::string, std::string> SwigType_pop(const std::string& t)
{
  // Python returns None for None input; the empty string never reaches here in
  // a way that matters (callers loop on isqualifier/issimple first).
  const int sz = ElementSize(t);
  return {PySlice(t, 0, sz), PySlice(t, sz)};
}

bool SwigType_issimple(const std::string& t)
{
  long c = 0;
  if (t.empty())
    return false;
  const long len = static_cast<long>(t.size());
  while (c < len)
  {
    if (t[static_cast<size_t>(c)] == '<')
    {
      int nest = 1;
      c += 1;
      while (c < len && nest != 0)
      {
        if (t[static_cast<size_t>(c)] == '<')
          nest += 1;
        if (t[static_cast<size_t>(c)] == '>')
          nest -= 1;
        c += 1;
      }
      c -= 1;
    }
    if (c < len && t[static_cast<size_t>(c)] == '.')
      return false;
    c += 1;
  }
  return true;
}

} // namespace swigbindings
