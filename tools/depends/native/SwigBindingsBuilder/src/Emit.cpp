/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Emit.h"

#include <cstdint>
#include <iostream>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "Helper.h"
#include "MethodType.h"
#include "PythonTools.h"
#include "SwigTypeParser.h"
#include "Typemaps.h"

namespace swigbindings
{

namespace
{

const std::string SEP = std::string("  //") + std::string(73, '=');

bool StartsWith(const std::string& s, const std::string& prefix)
{
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
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

std::string Replace(std::string s, const std::string& from, const std::string& to)
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

std::vector<std::string> SplitComma(const std::string& s)
{
  std::vector<std::string> out;
  size_t start = 0;
  for (;;)
  {
    const size_t p = s.find(',', start);
    if (p == std::string::npos)
    {
      out.push_back(s.substr(start));
      break;
    }
    out.push_back(s.substr(start, p - start));
    start = p + 1;
  }
  return out;
}

// Java String.hashCode emulation (emit.py _java_string_hashcode).
uint32_t JavaStringHashcode(const std::string& s)
{
  uint32_t h = 0;
  for (unsigned char ch : s)
    h = h * 31u + ch;
  return h;
}

// Java HashSet iteration order (emit.py _java_hashset_order). The extern
// declaration order is Java hash-bucket order, NOT insertion order: that is the
// determinism detail the reference depends on. Reproduce it exactly.
std::vector<std::string> JavaHashsetOrder(const std::vector<std::string>& items)
{
  int cap = 16;
  while (static_cast<double>(items.size()) > static_cast<double>(cap) * 0.75)
    cap <<= 1;
  std::vector<std::vector<std::string>> buckets(static_cast<size_t>(cap));
  for (const std::string& it : items)
  {
    uint32_t h = JavaStringHashcode(it);
    h ^= (h >> 16);
    buckets[static_cast<size_t>(h & static_cast<uint32_t>(cap - 1))].push_back(it);
  }
  std::vector<std::string> out;
  for (const auto& bk : buckets)
    for (const std::string& s : bk)
      out.push_back(s);
  return out;
}

// --- inline typemap registry (was the Helper.setup(...) call) --------------
std::string DefaultOut(const Bindings& b)
{
  return b.result + " = makePythonInstance(" + b.api + ",true);";
}

std::string DefaultIn(const Bindings& b)
{
  return b.api + " = (" + SwigType_str(b.ltype) + ")retrieveApiInstance(" + b.slarg + ",\"" +
         b.ltype + "\",\"" + FindNamespace(b.method) + "\",\"" + CallingName(b.method) + "\");";
}

std::vector<std::pair<std::string, TypemapFn>> BuildOutExact()
{
  return {
      {"void", [](const Bindings& b) { return std::string("Py_INCREF(Py_None);\n    ") + b.result + " = Py_None;"; }},
      {"long", [](const Bindings& b) { return b.result + " = PyLong_FromLong(" + b.api + ");"; }},
      {"unsigned long", [](const Bindings& b) { return b.result + " = PyLong_FromLong(" + b.api + ");"; }},
      {"bool", [](const Bindings& b) { return b.result + " = " + b.api + " ? Py_True : Py_False; Py_INCREF(" + b.result + ");"; }},
      {"long long", [](const Bindings& b) { return b.result + " = Py_BuildValue(\"L\", " + b.api + ");"; }},
      {"int", [](const Bindings& b) { return b.result + " = Py_BuildValue(\"i\", " + b.api + ");"; }},
      {"unsigned int", [](const Bindings& b) { return b.result + " = Py_BuildValue(\"I\", " + b.api + ");"; }},
      {"double", [](const Bindings& b) { return b.result + " = PyFloat_FromDouble(" + b.api + ");"; }},
      {"float", [](const Bindings& b) { return b.result + " = Py_BuildValue(\"f\", static_cast<double>(" + b.api + "));"; }},
      {"std::string", StringOut},
      {"p.q(const).char", [](const Bindings& b) { return b.result + " = PyUnicode_FromString(" + b.api + ");"; }},
  };
}

std::vector<PatternEntry> BuildOutPatterns()
{
  std::vector<PatternEntry> v;
  v.push_back({std::regex(R"((p.){0,1}XbmcCommons::Buffer)"), BufferOut});
  v.push_back({std::regex(R"(std::shared_ptr<\(.*\)>)"), SmartPtrOut});
  v.push_back({std::regex(R"(std::unique_ptr<\(.*\)>)"), SmartPtrOut});
  v.push_back({std::regex(R"((p.){0,1}std::vector<\(.*\)>)"), VectorOut});
  v.push_back({std::regex(R"((p.){0,1}Tuple<\(.*\)>)"), TupleOut});
  v.push_back({std::regex(R"((p.){0,1}Alternative<\(.*\)>)"), AlternativeOut});
  return v;
}

std::vector<std::pair<std::string, TypemapFn>> BuildInExact()
{
  return {
      {"std::string", [](const Bindings& b) {
         const std::string* nm = b.method->Attr("name");
         return std::string("if (") + b.slarg + ") PyXBMCGetUnicodeString(" + b.api + "," + b.slarg +
                ",false,\"" + b.api + "\",\"" + (nm ? *nm : std::string()) + "\");";
       }},
      {"bool", [](const Bindings& b) { return b.api + " = (PyLong_AsLong(" + b.slarg + ") == 0L ? false : true);"; }},
      {"long", [](const Bindings& b) { return b.api + " = PyLong_AsLong(" + b.slarg + ");"; }},
      {"unsigned long", [](const Bindings& b) { return b.api + " = PyLong_AsUnsignedLong(" + b.slarg + ");"; }},
      {"long long", [](const Bindings& b) { return b.api + " = PyLong_AsLongLong(" + b.slarg + ");"; }},
      {"unsigned long long", [](const Bindings& b) { return b.api + " = PyLong_AsUnsignedLongLong(" + b.slarg + ");"; }},
      {"int", [](const Bindings& b) { return b.api + " = (int)PyLong_AsLong(" + b.slarg + ");"; }},
      {"double", [](const Bindings& b) { return b.api + " = PyFloat_AsDouble(" + b.slarg + ");"; }},
      {"float", [](const Bindings& b) { return b.api + " = (float)PyFloat_AsDouble(" + b.slarg + ");"; }},
      {"XBMCAddon::StringOrInt", [](const Bindings& b) {
         const std::string* nm = b.method->Attr("name");
         return std::string("if (") + b.slarg + ") PyXBMCGetUnicodeString(" + b.api + "," + b.slarg +
                ",PyLong_Check(" + b.slarg + ") || PyFloat_Check(" + b.slarg + "),\"" + b.api +
                "\",\"" + (nm ? *nm : std::string()) + "\");";
       }},
  };
}

std::vector<PatternEntry> BuildInPatterns()
{
  std::vector<PatternEntry> v;
  v.push_back({std::regex(R"((p.){0,1}std::vector<\(.*\)>)"), VectorIn});
  v.push_back({std::regex(R"((p.){0,1}Tuple(3){0,1}<\(.*\)>)"), TupleIn});
  v.push_back({std::regex(R"((p.){0,1}Alternative<\(.*\)>)"), AlternativeIn});
  v.push_back({std::regex(R"((r.){0,1}XbmcCommons::Buffer)"), BufferIn});
  v.push_back({std::regex(R"((p.){0,1}std::map<\(.*\)>)"), MapIn});
  v.push_back({std::regex(R"((r.){0,1}XBMCAddon::Dictionary<\(.*\)>)"), DictIn});
  v.push_back({std::regex(R"(p.void)"), [](const Bindings& b) { return b.api + " = (void*)" + b.slarg + ";"; }});
  return v;
}

class Gen
{
public:
  explicit Gen(Node* module) : m_module(module)
  {
    const std::string* nm = module->Attr("name");
    m_mod = nm ? *nm : std::string();
    for (Node* n : module->DepthFirst())
      if (n->Name() == "class")
        m_classes.push_back(n);
    for (Node* n : module->DepthFirst())
    {
      const std::string& name = n->Name();
      if (name == "function" || name == "constructor" || name == "destructor")
        m_methods.push_back(n);
    }
  }

  std::string Run();

private:
  void E(const std::string& s) { m_out += s; }

  void DoMethod(Node* method, MethodType methodType);
  void DoClassTypeInfo(Node* clazz, std::vector<std::string>* classNameAsVariables);
  void DoExternClassTypeInfo(const std::string& knownType);
  std::vector<Node*> GetAllVirtualMethods(Node* clazz);
  void DoClassMethodInfo(Node* clazz, std::vector<std::string>* initTypeCalls);
  void DoDirectors();

  Node* m_module;
  std::string m_mod;
  std::string m_out;
  std::vector<Node*> m_classes;
  std::vector<Node*> m_methods;
  std::vector<std::string> m_initTypeCalls;
  std::vector<std::string> m_classNameAsVariables;
};

std::string AttrOr(Node* n, const std::string& key, const std::string& dflt = std::string())
{
  const std::string* v = n->Attr(key);
  return v ? *v : dflt;
}

// --- doMethod --------------------------------------------------------------
void Gen::DoMethod(Node* method, MethodType methodType)
{
  const std::string& mod = m_mod;
  const std::string name = AttrOr(method, "name");
  const bool isOperator = StartsWith(name, "operator ");
  bool doAsMappingIndex = false;
  bool doAsCallable = false;
  if (isOperator)
  {
    const std::string tail = name.substr(9);
    if (tail == "[]")
      doAsMappingIndex = true;
    else if (tail == "()")
      doAsCallable = true;
    else
      return;
  }
  (void)doAsCallable;

  const bool constructor = methodType == MethodType::Constructor;
  if (constructor)
  {
    const std::string* access = method->Attr("access");
    if (access != nullptr && *access != "public")
      return;
  }
  const bool destructor = methodType == MethodType::Destructor;
  std::vector<Node*> params = method->Kids("parm");
  const size_t numParams = params.size();
  bool clazzHas = false;
  const std::string clazz = FindFullClassName(method, clazzHas);
  std::string returns;
  if (constructor)
    returns = "p." + clazz;
  else if (destructor)
    returns = "void";
  else
    returns = GetReturnSwigType(method);
  Node* classnode = FindClassNode(method);
  std::string classNameAsVariable;
  if (clazzHas)
    classNameAsVariable = GetClassNameAsVariable(classnode);
  const bool useKeywordParsing =
      !((classnode != nullptr && AttrOr(classnode, "feature_python_nokwds") == "true") ||
        AttrOr(method, "feature_python_nokwds") == "true");

  if (!constructor && !destructor)
  {
    if (HasDoc(method))
      E("\n  PyDoc_STRVAR(" + GetPyMethodName(method, methodType) + "__doc__,\n               " +
        MakeDocString(method->Kids("doc")[0]) + ");\n");
  }

  // signature
  const std::string rettype = destructor ? "void" : "PyObject*";
  const std::string selfty = !clazzHas ? "PyObject" : (constructor ? "PyTypeObject" : "PyHolder");
  const std::string selfname = constructor ? "pytype" : "self";
  std::string sig = "\n  static " + rettype + " " + mod + "_" + GetPyMethodName(method, methodType) +
                    " (" + selfty + "* " + selfname + " ";
  if (doAsMappingIndex)
    sig += ", PyObject* py" + AttrOr(params[0], "name");
  else if (!destructor)
    sig += " , PyObject *args, PyObject *kwds ";
  sig += " )";
  E(sig);
  E("\n  {\n    XBMC_TRACE;\n");

  if (numParams > 0)
  {
    if (useKeywordParsing && !doAsMappingIndex)
    {
      E("\n    static const char *keywords[] = {");
      for (Node* it : params)
        E("\n          \"" + AttrOr(it, "name") + "\",");
      E("\n          NULL};\n");
    }
    for (Node* it : params)
    {
      const std::string* value = it->Attr("value");
      std::string valpart;
      if (value != nullptr)
        valpart = " = " + *value;
      else if (SwigType_ispointer(AttrOr(it, "type")))
        valpart = " = nullptr";
      else
        valpart = "";
      E("\n    " + SwigType_str(ConvertTypeToLTypeForParam(AttrOr(it, "type"))) + " " +
        AttrOr(it, "name") + " " + valpart + ";");
      if (!ParameterCanBeUsedDirectly(it) && !doAsMappingIndex)
        E("\n    PyObject* py" + AttrOr(it, "name") + " = NULL;");
    }
    if (!doAsMappingIndex)
    {
      E(std::string("\n    if (!") + (useKeywordParsing ? "PyArg_ParseTupleAndKeywords" : "PyArg_ParseTuple") +
        "(\n       args,\n       ");
      if (useKeywordParsing)
        E("kwds,");
      E("\n       \"" + MakeFormatStringFromParameters(method) + "\",\n       ");
      if (useKeywordParsing)
        E("const_cast<char**>(keywords),");
      for (size_t i = 0; i < params.size(); ++i)
      {
        Node* param = params[i];
        E(std::string("\n         &") + (ParameterCanBeUsedDirectly(param) ? "" : "py") +
          AttrOr(param, "name") + (i < params.size() - 1 ? "," : ""));
      }
      E("\n       ))\n    {\n      return NULL;\n    }\n\n");
    }
  }

  if (returns != "void")
    E("    " + SwigType_str(returns) + " apiResult;");
  E("\n    try\n    {\n");

  // input conversions
  for (Node* it : params)
  {
    if ((!ParameterCanBeUsedDirectly(it)) || doAsMappingIndex)
      E("      " + GetInConversion(AttrOr(it, "type"), AttrOr(it, "name"), "py" + AttrOr(it, "name"), method) + " \n");
  }
  E("\n");

  const bool isDirectorCall = IsDirector(method);
  if (isDirectorCall)
    E("      // This is a director call coming from python so it explicitly calls the base class method.\n");

  if (!destructor)
  {
    if (constructor || !clazzHas)
      E("      XBMCAddon::SetLanguageHookGuard slhg(XBMCAddon::Python::PythonLanguageHook::GetIfExists(PyThreadState_Get()->interp).get());\n");
    E("      ");
    if (returns != "void")
      E("apiResult = ");
    if (clazzHas && !constructor)
      E("((" + clazz + "*)retrieveApiInstance((PyObject*)self,&Ty" + classNameAsVariable +
        "_Type,\"" + CallingName(method) + "\",\"" + clazz + "\"))-> ");
    if (constructor && classnode->Attr("feature_director") != nullptr)
    {
      E("(&(Ty" + classNameAsVariable + "_Type.pythonType) != pytype) ? new " + classNameAsVariable +
        "_Director(");
      for (size_t i = 0; i < params.size(); ++i)
        E(" " + AttrOr(params[i], "name") + (i < params.size() - 1 ? "," : "") + " ");
      E(") : ");
    }
    if (isDirectorCall)
      E(clazz + "::");
    E(CallingName(method) + "( ");
    for (size_t i = 0; i < params.size(); ++i)
      E(" " + AttrOr(params[i], "name") + (i < params.size() - 1 ? "," : "") + " ");
    E(" );\n");
    if (constructor)
      E("      prepareForReturn(apiResult);");
  }
  else
  {
    E("\n      " + clazz + "* theObj = (" + clazz + "*)retrieveApiInstance((PyObject*)self,&Ty" +
      classNameAsVariable + "_Type,\"~" + CallingName(method) + "\",\"" + clazz +
      "\");\n      cleanForDealloc(theObj);\n");
  }

  // catch blocks
  E("\n    }\n    catch (const XBMCAddon::WrongTypeException& e)\n    {\n"
    "      CLog::Log(LOGERROR,\"EXCEPTION: {}\",e.GetExMessage());\n"
    "      PyErr_SetString(PyExc_TypeError, e.GetExMessage()); ");
  if (!destructor)
    E("\n      return NULL; ");
  E("\n    }\n    catch (const XbmcCommons::Exception& e)\n    {\n"
    "      CLog::Log(LOGERROR,\"EXCEPTION: {}\",e.GetExMessage());\n"
    "      PyErr_SetString(PyExc_RuntimeError, e.GetExMessage()); ");
  if (!destructor)
    E("\n      return NULL; ");
  E("\n    }\n    catch (...)\n    {\n"
    "      CLog::Log(LOGERROR,\"EXCEPTION: Unknown exception thrown from the call \\\"" +
    CallingName(method) + "\\\"\");\n"
    "      PyErr_SetString(PyExc_RuntimeError, \"Unknown exception thrown from the call \\\"" +
    CallingName(method) + "\\\"\"); ");
  if (!destructor)
    E("\n      return NULL; ");
  E("\n    }\n");

  // return section
  if (!destructor)
  {
    E("\n    PyObject* result = Py_None;\n\n    // transform the result\n");
    if (constructor)
      E("    result = makePythonInstance(apiResult,pytype,false);");
    else
      E("    " + GetOutConversion(returns, "result", method));
    if (constructor && method->Attr("feature_director") != nullptr)
      E("\n    if (&(Ty" + classNameAsVariable + "_Type.pythonType) != pytype)\n      ((" +
        classNameAsVariable + "_Director*)apiResult)->setPyObjectForDirector(result);");
    E("\n\n    return result; ");
  }
  else
  {
    E("\n    (((PyObject*)(self))->ob_type)->tp_free((PyObject*)self);\n    ");
  }
  E("\n  } ");
}

// --- doClassTypeInfo -------------------------------------------------------
void Gen::DoClassTypeInfo(Node* clazz, std::vector<std::string>* classNameAsVariables)
{
  const std::string cnav = GetClassNameAsVariable(clazz);
  const std::string full = FindFullClassName(clazz);
  if (classNameAsVariables != nullptr)
    classNameAsVariables->push_back(cnav);
  E("\n" + SEP + "\n  // These variables will hold the Python Type information for " + full +
    "\n  TypeInfo Ty" + cnav + "_Type(typeid(" + full + "));\n" + SEP + "\n");
}

// --- doExternClassTypeInfo -------------------------------------------------
void Gen::DoExternClassTypeInfo(const std::string& knownType)
{
  const std::string cnav = Replace(knownType, "::", "_");
  E("\n" + SEP + "\n  // These variables define the type " + knownType +
    " from another module\n  extern TypeInfo Ty" + cnav + "_Type;\n" + SEP + "\n");
}

// --- getAllVirtualMethods --------------------------------------------------
std::vector<Node*> Gen::GetAllVirtualMethods(Node* clazz)
{
  std::vector<Node*> ret;
  for (Node* c : MutableChildrenPtrs(clazz))
    if (c->Name() == "function" && AttrOr(c, "storage") == "virtual")
      ret.push_back(c);
  std::vector<Node*> baselists = clazz->Kids("baselist");
  if (!baselists.empty())
  {
    for (Node* b : baselists[0]->Kids("base"))
    {
      Node* bcn = FindClassNodeByName(m_module, AttrOr(b, "name"), clazz);
      if (bcn != nullptr && bcn->Attr("feature_director") != nullptr)
      {
        std::vector<Node*> more = GetAllVirtualMethods(bcn);
        ret.insert(ret.end(), more.begin(), more.end());
      }
    }
  }
  return ret;
}

// --- doClassMethodInfo -----------------------------------------------------
void Gen::DoClassMethodInfo(Node* clazz, std::vector<std::string>* initTypeCalls)
{
  const std::string& mod = m_mod;
  const std::string cnav = GetClassNameAsVariable(clazz);
  const std::string full = FindFullClassName(clazz);
  const std::string initTypeCall = "initPy" + cnav + "_Type";
  if (initTypeCalls != nullptr)
    initTypeCalls->push_back(initTypeCall);

  bool doComparator = false;
  bool doAsMapping = false;
  bool hasEquivalenceOp = false;
  bool hasLtOp = false;
  bool hasGtOp = false;
  Node* indexOp = nullptr;
  Node* callableOp = nullptr;
  Node* sizeNode = nullptr;

  std::vector<Node*> normalMethods;
  std::vector<Node*> operators;
  for (Node* it : clazz->Kids("function"))
  {
    if (StartsWith(AttrOr(it, "name"), "operator "))
      operators.push_back(it);
    else
      normalMethods.push_back(it);
  }
  std::vector<Node*> properties;
  for (Node* it : clazz->Kids("variable"))
  {
    const std::string* access = it->Attr("access");
    if (access != nullptr && *access == "public")
      properties.push_back(it);
  }
  std::vector<Node*> propertiesSet;
  for (Node* it : properties)
  {
    const std::string* imm = it->Attr("feature_immutable");
    if (imm == nullptr || *imm == "0")
      propertiesSet.push_back(it);
  }

  for (Node* it : operators)
  {
    const std::string tail = AttrOr(it, "name").substr(9);
    if (StartsWith(tail, "=="))
      hasEquivalenceOp = true;
    else if (tail == "<")
      hasLtOp = true;
    else if (tail == ">")
      hasGtOp = true;
    else if (tail == "[]")
      indexOp = it;
    else if (tail == "()")
      callableOp = it;
    else
      std::cerr << "Warning: class " << full << " has an operator \"" << AttrOr(it, "name")
                << "\" that is being ignored.\n";
  }

  if (hasGtOp || hasLtOp || hasEquivalenceOp)
  {
    if (!(hasLtOp && hasGtOp && hasEquivalenceOp))
      std::cerr << "Warning: class " << full << " has an inconsistent operator set.\n";
    else
      doComparator = true;
  }
  (void)doComparator;

  if (indexOp != nullptr)
  {
    sizeNode = nullptr;
    for (Node* f : clazz->Kids("function"))
    {
      if (AttrOr(f, "name") == "size")
      {
        sizeNode = f;
        break;
      }
    }
    if (sizeNode != nullptr)
      doAsMapping = true;
    else
      std::cerr << "Warning: class " << full
                << " has an inconsistent operator set (need size + operator[]).\n";
  }

  if (doAsMapping)
  {
    E("\n  static Py_ssize_t " + mod + "_" + cnav + "_size_(PyObject* self)\n  {\n"
      "    return (Py_ssize_t)((" + full + "*)retrieveApiInstance(self,&Ty" + cnav + "_Type,\"" +
      CallingName(indexOp) + "\",\"" + full + "\"))-> size();\n  }\n\n" + SEP +
      "\n  // tp_as_mapping struct for " + full + "\n" + SEP + "\n  PyMappingMethods " + mod + "_" +
      cnav + "_as_mapping = {\n    " + mod + "_" + cnav +
      "_size_,    /* inquiry mp_length;                  __len__ */\n    (PyCFunction)" + mod + "_" +
      cnav + ",   /* binaryfunc mp_subscript             __getitem__ */\n    0,                  /* objargproc mp_ass_subscript;     __setitem__ */\n  };\n");
  }

  if (clazz->Attr("feature_python_rcmp") != nullptr)
    E("\n  static PyObject* " + mod + "_" + cnav +
      "_rcmp(PyObject* obj1, PyObject *obj2, int method)\n  " +
      Unescape(AttrOr(clazz, "feature_python_rcmp")) + "\n");

  E("\n" + SEP + "\n  // This section contains the initialization for the\n"
    "  // Python extension for the Api class " + full + "\n" + SEP +
    "\n  // All of the methods on this class\n  static PyMethodDef " + cnav + "_methods[] = { ");
  for (Node* it : normalMethods)
    E("\n    {\"" + AttrOr(it, "sym_name") + "\", (PyCFunction)" + mod + "_" +
      GetPyMethodName(it, MethodType::Method) + ", METH_VARARGS|METH_KEYWORDS, " +
      (HasDoc(it) ? (GetPyMethodName(it, MethodType::Method) + "__doc__") : "NULL") + " }, ");
  for (const auto& kv : clazz->Attributes())
  {
    const std::string& key = kv.first;
    if (StartsWith(key, "feature_python_method_"))
    {
      const std::string methodName = key.substr(std::string("feature_python_method_").size());
      E("\n    {\"" + methodName + "\", (PyCFunction)" + mod + "_" + GetClassNameAsVariable(clazz) +
        "_" + methodName + ", METH_VARARGS|METH_KEYWORDS, NULL},\n");
    }
  }
  E("\n    {NULL, NULL, 0, NULL}\n  };\n\n");

  if (!properties.empty())
  {
    const std::string clazzName = FindFullClassName(properties[0]);
    E("  static PyObject* " + cnav +
      "_getMember(PyHolder *self, void *name)\n  {\n    if (self == NULL)\n      return NULL;\n");
    E("\n    try\n    {\n      " + clazzName + "* theObj = (" + clazzName +
      "*)retrieveApiInstance((PyObject*)self, &Ty" + cnav + "_Type, \"" + cnav + "_getMember()\", \"" +
      clazzName + "\");\n\n      PyObject* result = NULL;\n   ");
    for (Node* it : properties)
    {
      const std::string returns = GetPropertyReturnSwigType(it);
      E(" if (strcmp((char*)name, \"" + AttrOr(it, "sym_name") + "\") == 0)\n      {\n        " +
        SwigType_lstr(returns) + " apiResult = theObj->" + AttrOr(it, "sym_name") + ";\n        " +
        GetOutConversion(returns, "result", it) + "\n      }\n      else");
    }
    E("\n      {\n        Py_INCREF(Py_None);\n        return Py_None;\n      }\n\n      return result;\n    }\n"
      "    catch (const XBMCAddon::WrongTypeException& e)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: {}\",e.GetExMessage());\n      PyErr_SetString(PyExc_TypeError, e.GetExMessage());\n      return NULL;\n    }\n"
      "    catch (const XbmcCommons::Exception& e)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: {}\",e.GetExMessage());\n      PyErr_SetString(PyExc_RuntimeError, e.GetExMessage());\n      return NULL;\n    }\n"
      "    catch (...)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: Unknown exception thrown from the call \\\"" + cnav +
      "_getMember()\\\"\");\n      PyErr_SetString(PyExc_RuntimeError, \"Unknown exception thrown from the call \\\"" + cnav +
      "_getMember()\\\"\");\n      return NULL;\n    }\n\n    return NULL;\n  }\n\n");
    if (!propertiesSet.empty())
    {
      E("  int " + cnav +
        "_setMember(PyHolder *self, PyObject *value, void *name)\n  {\n    if (self == NULL)\n      return -1;\n\n    " +
        clazzName + "* theObj = NULL;\n    try\n    {\n      theObj = (" + clazzName +
        "*)retrieveApiInstance((PyObject*)self, &Ty" + cnav + "_Type, \"" + cnav + "_getMember()\", \"" +
        clazzName + "\");\n    }\n");
      E("    catch (const XBMCAddon::WrongTypeException& e)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: {}\",e.GetExMessage());\n      PyErr_SetString(PyExc_TypeError, e.GetExMessage());\n      return -1;\n    }\n"
        "    catch (const XbmcCommons::Exception& e)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: {}\",e.GetExMessage());\n      PyErr_SetString(PyExc_RuntimeError, e.GetExMessage());\n      return -1;\n    }\n"
        "    catch (...)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: Unknown exception thrown from the call \\\"" + cnav +
        "_getMember()\\\"\");\n      PyErr_SetString(PyExc_RuntimeError, \"Unknown exception thrown from the call \\\"" + cnav +
        "_getMember()\\\"\");\n      return -1;\n    }\n\n");
      for (Node* it : propertiesSet)
      {
        const std::string returns = GetPropertyReturnSwigType(it);
        E(" if (strcmp((char*)name, \"" + AttrOr(it, "sym_name") + "\") == 0)\n      {\n        " +
          SwigType_lstr(returns) + " tmp;\n        " + GetInConversion(returns, "tmp", "value", it) +
          "\n        if (PyErr_Occurred())\n          throw PythonBindings::PythonToCppException();\n\n        theObj->" +
          AttrOr(it, "sym_name") + " = tmp;\n      }\n      else");
      }
      E("\n        return -1;\n\n    return 0;\n  } ");
    }
    E("\n\n  // All of the methods on this class\n  static PyGetSetDef " + cnav + "_getsets[] = { ");
    for (Node* it : properties)
    {
      const std::string* imm = it->Attr("feature_immutable");
      const bool immutable = (imm == nullptr || *imm == "0");
      E("\n    {(char*)\"" + AttrOr(it, "sym_name") + "\", (getter)" + cnav + "_getMember, " +
        (immutable ? ("(setter)" + cnav + "_setMember") : "NULL") + ", (char*)" +
        (HasDoc(it) ? MakeDocString(it->Kids("doc")[0]) : "NULL") + ", (char*)\"" +
        AttrOr(it, "sym_name") + "\" }, ");
    }
    E("\n    {NULL}\n  };\n");
  }

  const std::string* featIterPtr = clazz->Attr("feature_iterator");
  const std::string* featIterablePtr = clazz->Attr("feature_iterable");
  const std::string featIter = featIterPtr ? *featIterPtr : std::string();
  const std::string featIterable = featIterablePtr ? *featIterablePtr : std::string();
  const bool hasIter = featIterPtr != nullptr;       // truthy means present
  const bool hasIterable = featIterablePtr != nullptr;
  if ((hasIter && featIter != "") || (hasIterable && featIterable != ""))
  {
    E("\n  static PyObject* " + mod + "_" + cnav + "_iter(PyObject* self)\n  { ");
    if (hasIter && featIter != "")
    {
      E("\n    return self; ");
    }
    else
    {
      E("\n    PyObject* result = NULL;\n    try\n    {\n      " + featIterable + "* apiResult = ((" +
        full + "*)retrieveApiInstance(self,&Ty" + cnav + "_Type,\"" + mod + "_" + cnav +
        "_iternext\",\"" + full + "\"))->begin();\n\n      " +
        GetOutConversion("p." + featIterable, "result", clazz) +
        "\n    }\n"
        "    catch (const XBMCAddon::WrongTypeException& e)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: {}\",e.GetExMessage());\n      PyErr_SetString(PyExc_TypeError, e.GetExMessage());\n      return NULL;\n    }\n"
        "    catch (const XbmcCommons::Exception& e)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: {}\",e.GetExMessage());\n      PyErr_SetString(PyExc_RuntimeError, e.GetExMessage());\n      return NULL;\n    }\n"
        "    catch (...)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: Unknown exception thrown from the call \\\"" + mod + "_" + cnav +
        "_iternext\\\"\");\n      PyErr_SetString(PyExc_RuntimeError, \"Unknown exception thrown from the call \\\"" + mod + "_" + cnav +
        "_iternext\\\"\");\n      return NULL;\n    }\n\n    return result; ");
    }
    E("\n  }\n");
    if (hasIter && featIter != "")
    {
      E("\n  static PyObject* " + mod + "_" + cnav +
        "_iternext(PyObject* self)\n  {\n    PyObject* result = NULL;\n    try\n    {\n      " + full +
        "* iter = (" + full + "*)retrieveApiInstance(self,&Ty" + cnav + "_Type,\"" + mod + "_" + cnav +
        "_iternext\",\"" + full +
        "\");\n\n      // check if we have reached the end\n      if (!iter->end())\n      {\n        ++(*iter);\n\n        " +
        featIter + " apiResult = **iter;\n        " + GetOutConversion(featIter, "result", clazz) +
        "\n      }\n    }\n"
        "    catch (const XBMCAddon::WrongTypeException& e)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: {}\",e.GetExMessage());\n      PyErr_SetString(PyExc_TypeError, e.GetExMessage());\n      return NULL;\n    }\n"
        "    catch (const XbmcCommons::Exception& e)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: {}\",e.GetExMessage());\n      PyErr_SetString(PyExc_RuntimeError, e.GetExMessage());\n      return NULL;\n    }\n"
        "    catch (...)\n    {\n      CLog::Log(LOGERROR,\"EXCEPTION: Unknown exception thrown from the call \\\"" + mod + "_" + cnav +
        "_iternext\\\"\");\n      PyErr_SetString(PyExc_RuntimeError, \"Unknown exception thrown from the call \\\"" + mod + "_" + cnav +
        "_iternext\\\"\");\n      return NULL;\n    }\n\n    return result;\n  }\n");
    }
  }
  E("\n");

  // init method
  E("\n  // This method initializes the above mentioned Python Type structure\n  static void " +
    initTypeCall + "()\n  {\n");
  if (HasDoc(clazz))
    E("\n    PyDoc_STRVAR(" + cnav + "__doc__,\n                 " +
      MakeDocString(clazz->Kids("doc")[0]) + "\n                );\n");
  E("\n\n    PyTypeObject& pythonType = Ty" + cnav + "_Type.pythonType;\n    pythonType.tp_name = \"" +
    mod + "." + AttrOr(clazz, "sym_name") + "\";\n    pythonType.tp_basicsize = sizeof(PyHolder);\n    pythonType.tp_dealloc = (destructor)" +
    mod + "_" + cnav + "_Dealloc; ");
  if (clazz->Attr("feature_python_rcmp") != nullptr)
    E("\n    pythonType.tp_richcompare=(richcmpfunc)" + mod + "_" + cnav + "_rcmp;");
  E("\n\n    pythonType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;\n\n    pythonType.tp_doc = " +
    (HasDoc(clazz) ? (cnav + "__doc__") : "NULL") + ";\n    pythonType.tp_methods = " + cnav +
    "_methods; ");
  if (!properties.empty())
    E("\n    pythonType.tp_getset = " + cnav + "_getsets;\n");
  if (callableOp != nullptr)
    E("\n    pythonType.tp_call = (ternaryfunc)" + mod + "_" +
      GetPyMethodName(callableOp, MethodType::Method) + ";\n");
  if (doAsMapping)
    E("\n    pythonType.tp_as_mapping = &" + mod + "_" + cnav + "_as_mapping;\n");
  if (hasIter && featIter != "")
    E("\n    pythonType.tp_iter = (getiterfunc)" + mod + "_" + cnav +
      "_iter;\n    pythonType.tp_iternext = (iternextfunc)" + mod + "_" + cnav + "_iternext;\n");
  else if (hasIterable && featIterable != "")
    E("\n    pythonType.tp_iter = (getiterfunc)" + mod + "_" + cnav + "_iter;\n");
  Node* baseclass = FindValidBaseClass(clazz, m_module);
  E("\n\n    pythonType.tp_base = " +
    (baseclass != nullptr ? ("&(Ty" + GetClassNameAsVariable(baseclass) + "_Type.pythonType)") : "NULL") +
    ";\n    pythonType.tp_new = ");
  E((!HasDefinedConstructor(clazz) || HasHiddenConstructor(clazz)) ? "NULL"
                                                                   : (mod + "_" + cnav + "_New"));
  E(";\n    pythonType.tp_init = dummy_tp_init;\n\n    Ty" + cnav + "_Type.swigType=\"p." + full +
    "\";");
  if (baseclass != nullptr)
    E("\n    Ty" + cnav + "_Type.parentType=&Ty" + GetClassNameAsVariable(baseclass) + "_Type;\n");
  if (!HasHiddenConstructor(clazz))
    E("\n    registerAddonClassTypeInformation(&Ty" + cnav + "_Type);\n");
  E("\n  }\n" + SEP + "\n");
}

// --- directors -------------------------------------------------------------
void Gen::DoDirectors()
{
  for (Node* clazz : m_classes)
  {
    if (clazz->Attr("feature_director") == nullptr)
      continue;
    const std::string cnav = GetClassNameAsVariable(clazz);
    std::vector<Node*> ctors = clazz->Kids("constructor");
    Node* constructor = ctors.empty() ? nullptr : ctors[0];
    E("\n" + SEP + "\n  // This class is the Director for " + FindFullClassName(clazz) +
      ".\n  // It provides the \"reverse bridge\" from C++ to Python to support\n  // cross-language polymorphism.\n" +
      SEP + "\n  class " + cnav + "_Director : public Director, public " + AttrOr(clazz, "name") +
      "\n  {\n    public:\n");
    if (constructor != nullptr)
    {
      E("\n      inline " + cnav + "_Director(");
      std::vector<Node*> params = constructor->Kids("parm");
      for (size_t i = 0; i < params.size(); ++i)
        E(SwigType_str(AttrOr(params[i], "type")) + " " + AttrOr(params[i], "name") +
          (i < params.size() - 1 ? "," : "") + " ");
      E(") : " + FindFullClassName(constructor) + "(");
      for (size_t i = 0; i < params.size(); ++i)
        E(" " + AttrOr(params[i], "name") + (i < params.size() - 1 ? "," : "") + " ");
      E(") { } ");
    }
    E("\n");
    for (Node* it : GetAllVirtualMethods(clazz))
    {
      E("\n      virtual " + SwigType_str(GetReturnSwigType(it)) + " " + CallingName(it) + "( ");
      std::vector<Node*> params = it->Kids("parm");
      std::string paramFormatStr(params.size(), 'O');
      for (size_t i = 0; i < params.size(); ++i)
        E(" " + SwigType_str(AttrOr(params[i], "type")) + " " + AttrOr(params[i], "name") +
          (i < params.size() - 1 ? "," : "") + " ");
      E(" )\n      { ");
      for (Node* param : params)
      {
        OverrideBindings ov;
        ov.result = "py" + AttrOr(param, "name");
        ov.api = AttrOr(param, "name");
        E("\n        PyObject* py" + AttrOr(param, "name") + " = NULL;\n        " +
          GetOutConversion(AttrOr(param, "type"), "result", it, &ov));
      }
      E("\n        XBMCAddon::Python::PyContext pyContext;\n        PyObject_CallMethod(self,\"" +
        CallingName(it) + "\",\"(" + paramFormatStr + ")\"");
      for (Node* param : params)
        E(", py" + AttrOr(param, "name") + " ");
      E(");\n        if (PyErr_Occurred())\n          throw PythonBindings::PythonToCppException();\n      }\n");
    }
    E("\n  };\n");
  }
}

std::string Gen::Run()
{
  const std::string& mod = m_mod;
  Node* module = m_module;

  Setup(m_classes, BuildOutExact(), BuildOutPatterns(), DefaultOut, BuildInExact(),
        BuildInPatterns(), DefaultIn);

  E("\n\n/*\n *  Copyright (C) 2005-2018 Team Kodi\n *  This file is part of Kodi - https://kodi.tv\n *\n *  SPDX-License-Identifier: GPL-2.0-or-later\n *  See LICENSES/README.md for more information.\n */\n\n");
  E("// ************************************************************************\n// This file was generated by xbmc compile process. DO NOT EDIT!!\n//  It was created by running the code generator on the spec file for\n//  the module \"" +
    mod + "\" on the template file PythonSwig.template.cpp\n// ************************************************************************\n\n");
  for (Node* it : GetInsertNodes(module, "begin"))
    E(UnescapeNode(it));
  E("\n\n#include <Python.h>\n#include <string>\n#include \"CompileInfo.h\"\n#include \"interfaces/python/LanguageHook.h\"\n#include \"interfaces/python/swig.h\"\n#include \"interfaces/python/PyContext.h\"\n\n");
  for (Node* it : GetInsertNodes(module, "header"))
    E(UnescapeNode(it));
  E("\n\nnamespace PythonBindings\n{\n");

  for (Node* clazz : m_classes)
    DoClassTypeInfo(clazz, &m_classNameAsVariables);

  // knownApiTypes: collect distinct (insertion-ordered) names, then emit them in
  // Java HashSet bucket order (the determinism detail).
  std::vector<std::string> knownApiTypes;
  std::set<std::string> seen;
  for (Node* it : module->DepthFirst())
  {
    const std::string* attr = it->Attr("feature_knownapitypes");
    if (attr != nullptr)
    {
      for (const std::string& t : SplitComma(Strip(*attr)))
      {
        if (seen.find(t) == seen.end())
        {
          seen.insert(t);
          knownApiTypes.push_back(t);
        }
      }
    }
  }
  for (const std::string& t : JavaHashsetOrder(knownApiTypes))
    DoExternClassTypeInfo(t);

  E("\n\n");
  DoDirectors();

  for (Node* it : m_methods)
  {
    if (it->Name() != "destructor")
    {
      DoMethod(it, it->Name() == "constructor" ? MethodType::Constructor : MethodType::Method);
      E("\n");
    }
  }
  for (Node* clazz : m_classes)
    DoMethod(clazz, MethodType::Destructor);

  for (Node* node : m_classes)
  {
    for (const auto& kv : node->Attributes())
    {
      const std::string& key = kv.first;
      if (StartsWith(key, "feature_python_method_"))
      {
        const std::string methodName = key.substr(std::string("feature_python_method_").size());
        E("\n  static PyObject* " + mod + "_" + GetClassNameAsVariable(node) + "_" + methodName +
          "(PyObject* self, PyObject *args, PyObject *kwds)\n  " + Unescape(kv.second) + "\n");
      }
    }
  }

  for (Node* clazz : m_classes)
    DoClassMethodInfo(clazz, &m_initTypeCalls);

  E("\n\n  static PyMethodDef " + mod + "_methods[] = { ");
  for (Node* it : module->DepthFirst())
  {
    if (it->Name() == "function" &&
        Parents(it, [](Node* n) { return n->Name() == "class"; }).empty())
    {
      E("\n    {\"" + AttrOr(it, "sym_name") + "\", (PyCFunction)" + mod + "_" +
        GetPyMethodName(it, MethodType::Method) + ", METH_VARARGS|METH_KEYWORDS, " +
        (HasDoc(it) ? (GetPyMethodName(it, MethodType::Method) + "__doc__") : "NULL") + " }, ");
    }
  }
  E("\n    {NULL, NULL, 0, NULL}\n  };\n\n"
    "  // This is the call that will call all of the other initializes\n"
    "  //  for all of the classes in this module\n  static void initTypes()\n  {\n"
    "    static bool typesAlreadyInitialized = false;\n    if (!typesAlreadyInitialized)\n    {\n"
    "      typesAlreadyInitialized = true;\n");
  for (const std::string& it : m_initTypeCalls)
    E("\n      " + it + "();");
  for (const std::string& it : m_classNameAsVariables)
    E("\n      if (PyType_Ready(&(Ty" + it + "_Type.pythonType)) < 0)\n        return;");
  E("\n    }\n  }\n\n  static struct PyModuleDef createModule\n  {\n      PyModuleDef_HEAD_INIT,\n      \"" +
    mod + "\",\n      \"\",\n      -1,\n      " + mod +
    "_methods,\n      nullptr,\n      nullptr,\n      nullptr,\n      nullptr,\n  };\n\n  PyObject *PyInit_Module_" +
    mod + "()\n  {\n    initTypes();\n\n    // init general " + mod +
    " modules\n    PyObject* module;\n\n");
  for (const std::string& it : m_classNameAsVariables)
    E("\n    Py_INCREF(&(Ty" + it + "_Type.pythonType));");
  E("\n\n    module = PyModule_Create(&createModule);\n    if (module == NULL) return NULL;\n\n");
  for (Node* clazz : m_classes)
    E("\n    PyModule_AddObject(module, \"" + AttrOr(clazz, "sym_name") + "\", (PyObject*)(&(Ty" +
      GetClassNameAsVariable(clazz) + "_Type.pythonType)));");
  E("\n\n   // constants\n   PyModule_AddStringConstant(module, \"__author__\", \"Team Kodi <http://kodi.tv>\");\n   PyModule_AddStringConstant(module, \"__date__\", CCompileInfo::GetBuildDate().c_str());\n   PyModule_AddStringConstant(module, \"__version__\", \"3.0.2\");\n   PyModule_AddStringConstant(module, \"__credits__\", \"Team Kodi\");\n   PyModule_AddStringConstant(module, \"__platform__\", \"ALL\");\n\n   // need to handle constants\n   // #define constants\n");
  for (Node* it : module->DepthFirst())
  {
    if (it->Name() == "constant")
    {
      const std::string type = AttrOr(it, "type");
      const bool isInt = (type == "int" || type == "long" || type == "unsigned int" ||
                          type == "unsigned long" || type == "bool");
      const std::string pyCall = isInt ? "PyModule_AddIntConstant" : "PyModule_AddStringConstant";
      E("\n   " + pyCall + "(module,\"" + AttrOr(it, "sym_name") + "\"," + AttrOr(it, "value") + "); ");
    }
  }
  E("\n  // constexpr constants\n");
  for (Node* it : module->DepthFirst())
  {
    if (it->Name() == "variable" && AttrOr(it, "storage") == "constexpr" && it->Attr("error") == nullptr)
    {
      const std::string type = AttrOr(it, "type");
      const bool isInt = (type == "q(const).int" || type == "q(const).long" ||
                          type == "q(const).unsigned int" || type == "q(const).unsigned long" ||
                          type == "q(const).bool");
      const std::string pyCall = isInt ? "PyModule_AddIntConstant" : "PyModule_AddStringConstant";
      E("\n   " + pyCall + "(module,\"" + AttrOr(it, "sym_name") + "\"," + AttrOr(it, "value") + "); ");
    }
  }
  E("\n  return module;\n  }\n\n} // end PythonBindings namespace for python type definitions\n\n");
  for (Node* it : GetInsertNodes(module, "footer"))
    E(UnescapeNode(it));
  E("\n");
  return m_out;
}

} // namespace

std::string Generate(Node* module)
{
  Gen g(module);
  return g.Run();
}

} // namespace swigbindings
