/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Typemaps.h"

#include <string>
#include <vector>

#include "SwigTypeParser.h"

namespace swigbindings
{

namespace
{

std::string MethodName(Node* method)
{
  const std::string* n = method->Attr("name");
  return n ? *n : std::string();
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

// The raw in: XbmcCommons::Buffer template (typemaps.py _BUFFER_IN), verbatim.
const char* const kBufferIn = R"DELIM(
    if (PyUnicode_Check(${slarg}))
    {
      Py_ssize_t pysize;
      const char* str = PyUnicode_AsUTF8AndSize(${slarg}, &pysize);
      size_t size = static_cast<size_t>(pysize);
      ${api}.allocate(size);
      ${api}.put(str, size);
      ${api}.flip(); // prepare the buffer for reading from
    }
    else if (PyBytes_Check(${slarg}))
    {
      Py_ssize_t pysize = PyBytes_GET_SIZE(${slarg});
      const char* str = PyBytes_AS_STRING(${slarg});
      size_t size = static_cast<size_t>(pysize);
      ${api}.allocate(size);
      ${api}.put(str, size);
      ${api}.flip(); // prepare the buffer for reading from
    }
    else if (PyByteArray_Check(${slarg}))
    {
      size_t size = PyByteArray_Size(${slarg});
      ${api}.allocate(size);
      ${api}.put(PyByteArray_AsString(${slarg}),size);
      ${api}.flip(); // prepare the buffer for reading from
    }
    else
    {
      throw XBMCAddon::WrongTypeException("argument \"%s\" for \"%s\" must be a string, bytes or a bytearray", "${api}", "${mname}");
    })DELIM";

} // namespace

// --- out: std::string ------------------------------------------------------
std::string StringOut(const Bindings& b)
{
  const std::string enc =
      (b.method->Attr("feature_python_strictUnicode") != nullptr) ? "strict" : "surrogateescape";
  const std::string& api = b.api;
  return b.result + " = PyUnicode_DecodeUTF8(" + api + ".c_str()," + api + ".size(),\"" + enc +
         "\");\n";
}

// --- out: XbmcCommons::Buffer ----------------------------------------------
std::string BufferOut(const Bindings& b)
{
  const std::string accessor = SwigType_ispointer(b.type) ? "->" : ".";
  const std::string& api = b.api;
  return b.result + " = PyByteArray_FromStringAndSize((char*)" + api + accessor + "curPosition()," +
         api + accessor + "remaining());";
}

// --- in: XbmcCommons::Buffer -----------------------------------------------
std::string BufferIn(const Bindings& b)
{
  std::string out = kBufferIn;
  out = Replace(out, "${slarg}", b.slarg);
  out = Replace(out, "${api}", b.api);
  out = Replace(out, "${mname}", MethodName(b.method));
  return out;
}

// --- in: std::map<(K,V)> ---------------------------------------------------
std::string MapIn(const Bindings& b)
{
  const std::vector<std::string> targs = SwigType_templateparmlist(b.ltype);
  const std::string keytype = targs[0];
  const std::string valtype = targs[1];
  const std::string keyltype = SwigType_str(SwigType_ltype(keytype));
  const std::string valltype = SwigType_str(SwigType_ltype(valtype));
  Node* method = b.method;
  const std::string keyconv = GetInConversion(keytype, "key", "pykey", method);
  const std::string valconv = GetInConversion(valtype, "value", "pyvalue", method);
  const std::string& slarg = b.slarg;
  const std::string& api = b.api;
  return std::string("\n") +
         "    {\n"
         "      PyObject *pykey, *pyvalue;\n"
         "      Py_ssize_t pos = 0;\n"
         "      while(PyDict_Next(" + slarg + ", &pos, &pykey, &pyvalue))\n"
         "      {\n"
         "        " + keyltype + " key;\n"
         "        " + valltype + " value;\n"
         "        " + keyconv + "\n"
         "        " + valconv + "\n"
         "        " + api + ".emplace(std::move(key), std::move(value));\n"
         "      }\n"
         "    }";
}

// --- in: XBMCAddon::Dictionary<(V)> ----------------------------------------
std::string DictIn(const Bindings& b)
{
  const std::vector<std::string> targs = SwigType_templateparmlist(b.ltype);
  const std::string valtype = targs[0];
  const std::string valltype = SwigType_str(SwigType_ltype(valtype));
  Node* method = b.method;
  const std::string valconv = GetInConversion(valtype, "value", "pyvalue", method);
  const std::string& slarg = b.slarg;
  const std::string& api = b.api;
  const std::string mname = MethodName(method);
  return std::string("\n") +
         "    {\n"
         "      PyObject *pykey, *pyvalue;\n"
         "      Py_ssize_t pos = 0;\n"
         "      while(PyDict_Next(" + slarg + ", &pos, &pykey, &pyvalue))\n"
         "      {\n"
         "        std::string key;\n"
         "        PyXBMCGetUnicodeString(key,pykey,false,\"" + api + "\",\"" + mname + "\");\n"
         "        " + valltype + " value;\n"
         "        " + valconv + "\n"
         "        " + api + ".emplace(std::move(key), std::move(value));\n"
         "      }\n"
         "    }";
}

// --- in: std::vector<(T)> --------------------------------------------------
std::string VectorIn(const Bindings& b)
{
  const std::string& ltype = b.ltype;
  const std::string& typ = b.type;
  const std::string& slarg = b.slarg;
  const std::string& api = b.api;
  Node* method = b.method;
  Sequence* sequence = b.sequence;
  const std::string vectype = SwigType_templateparmlist(ltype)[0];
  const bool ispointer = SwigType_ispointer(typ);
  const std::string accessor = ispointer ? "->" : ".";
  const std::string seq = std::to_string(sequence->Increment());
  const std::string entryltype = SwigType_str(SwigType_ltype(vectype));

  OverrideBindings ov;
  ov.type = vectype;
  ov.ltype = SwigType_ltype(vectype);
  ov.sequence = sequence;
  const std::string nested =
      GetInConversion(vectype, "entry" + seq, "pyentry" + seq, method, &ov);

  std::string pushval;
  if (SwigType_ispointer(vectype) || vectype == "bool" || vectype == "double" || vectype == "int")
    pushval = "entry" + seq;
  else
    pushval = "std::move(entry" + seq + ")";

  std::string line21 = "      ";
  if (ispointer)
    line21 += api + " = new std::vector<" + SwigType_str(vectype) + ">();";

  return std::string("\n") +
         "    if (" + slarg + ")\n"
         "    {\n"
         "      bool isTuple = PyObject_TypeCheck(" + slarg + ",&PyTuple_Type);\n"
         "      if (!isTuple && !PyObject_TypeCheck(" + slarg + ",&PyList_Type))\n"
         "        throw WrongTypeException(\"The parameter \\\"" + api + "\\\" must be either a Tuple or a List.\");\n"
         "\n" +
         line21 + "\n"
         "      PyObject *pyentry" + seq + " = NULL;\n"
         "      Py_ssize_t vecSize = (isTuple ? PyTuple_Size(" + slarg + ") : PyList_Size(" + slarg + "));\n"
         "      " + api + accessor + "reserve(vecSize);\n"
         "      for(Py_ssize_t i = 0; i < vecSize; i++)\n"
         "      {\n"
         "        pyentry" + seq + " = (isTuple ? PyTuple_GetItem(" + slarg + ", i) : PyList_GetItem(" + slarg + ", i));\n"
         "        " + entryltype + " entry" + seq + ";\n"
         "        " + nested + "\n"
         "        " + api + accessor + "push_back(" + pushval + ");\n"
         "      }\n"
         "    }\n";
}

// --- out: std::vector<(T)> -------------------------------------------------
std::string VectorOut(const Bindings& b)
{
  const std::string& typ = b.type;
  const std::string& api = b.api;
  const std::string& result = b.result;
  Node* method = b.method;
  Sequence* sequence = b.sequence;
  const std::string vectype = SwigType_templateparmlist(typ)[0];
  const bool ispointer = SwigType_ispointer(typ);
  const std::string accessor = ispointer ? "->" : ".";
  const std::string seq = std::to_string(sequence->Increment());

  OverrideBindings ov;
  ov.result = "pyentry" + seq;
  ov.api = "(*iter)";
  ov.sequence = sequence;
  const std::string nested = GetOutConversion(vectype, "result", method, &ov);

  std::string out;
  if (ispointer)
    out += std::string("\n    if (") + api + " != NULL)\n    {\n";
  out += std::string("\n      ") + result + " = PyList_New(0);\n"
         "\n"
         "      for (std::vector<" + SwigType_str(vectype) + ">::iterator iter = " + api + accessor + "begin(); iter != " + api + accessor + "end(); ++iter)\n"
         "      {\n"
         "        PyObject* pyentry" + seq + ";\n"
         "        " + nested + "\n"
         "        PyList_Append(" + result + ", pyentry" + seq + ");\n"
         "        Py_DECREF(pyentry" + seq + ");\n"
         "      }\n";
  if (ispointer)
    out += "\n    }\n";
  out += "\n";
  return out;
}

// --- in: Tuple<(...)> ------------------------------------------------------
std::string TupleIn(const Bindings& b)
{
  const std::string& ltype = b.ltype;
  const std::string& typ = b.type;
  const std::string& slarg = b.slarg;
  const std::string& api = b.api;
  Node* method = b.method;
  Sequence* sequence = b.sequence;
  const std::vector<std::string> types = SwigType_templateparmlist(ltype);
  const bool ispointer = SwigType_ispointer(typ);
  const std::string accessor = ispointer ? "->" : ".";
  const std::string seq = std::to_string(sequence->Increment());
  const std::vector<std::string> tupleAccess = {"first", "second", "third", "fourth"};

  std::string out = std::string("\n") +
                    "    if(" + slarg + ")\n"
                    "    {\n"
                    "      bool isTuple = PyObject_TypeCheck(" + slarg + ",&PyTuple_Type);\n"
                    "      if (!isTuple && !PyObject_TypeCheck(" + slarg + ",&PyList_Type))\n"
                    "        throw WrongTypeException(\"The parameter \\\"" + api + "\\\" must be either a Tuple or a List.\");\n"
                    "      Py_ssize_t vecSize = (isTuple ? PyTuple_Size(" + slarg + ") : PyList_Size(" + slarg + "));\n";
  for (size_t entryIndex = 0; entryIndex < types.size(); ++entryIndex)
  {
    const std::string& curType = types[entryIndex];
    const std::string ei = std::to_string(entryIndex);
    const std::string entryltype = SwigType_str(SwigType_ltype(curType));
    OverrideBindings ov;
    ov.sequence = sequence;
    const std::string nested =
        GetInConversion(curType, "entry" + ei + "_" + seq, "pyentry" + ei + "_" + seq, method, &ov);
    out += std::string("\n") +
           "      if (vecSize > " + ei + ")\n"
           "      {\n"
           "        PyObject *pyentry" + ei + "_" + seq + " = NULL;\n"
           "        pyentry" + ei + "_" + seq + " = (isTuple ? PyTuple_GetItem(" + slarg + ", " + ei + ") : PyList_GetItem(" + slarg + ", " + ei + "));\n"
           "        " + entryltype + " entry" + ei + "_" + seq + ";\n"
           "        " + nested + "\n"
           "        " + api + accessor + tupleAccess[entryIndex] + "() = entry" + ei + "_" + seq + ";\n"
           "      }\n";
  }
  out += "\n    }\n";
  return out;
}

// --- out: Tuple<(...)> -----------------------------------------------------
std::string TupleOut(const Bindings& b)
{
  const std::string& typ = b.type;
  const std::string& api = b.api;
  const std::string& result = b.result;
  Node* method = b.method;
  Sequence* sequence = b.sequence;
  const std::vector<std::string> types = SwigType_templateparmlist(typ);
  const bool ispointer = SwigType_ispointer(typ);
  const std::string accessor = ispointer ? "->" : ".";
  const std::string seq = std::to_string(sequence->Increment());
  const std::vector<std::string> tupleAccess = {"first", "second", "third", "fourth"};

  std::string out = std::string("\n") +
                    "    int vecSize = " + api + accessor + "GetNumValuesSet();\n"
                    "    " + result + " = PyTuple_New(vecSize);\n";
  if (ispointer)
    out += std::string("\n    if (") + api + " != NULL)\n";
  out += "    {\n      PyObject* pyentry" + seq + "; ";
  for (size_t entryIndex = 0; entryIndex < types.size(); ++entryIndex)
  {
    const std::string& curType = types[entryIndex];
    const std::string ei = std::to_string(entryIndex);
    const std::string lrt = SwigType_str(SwigType_lrtype(curType));
    OverrideBindings ov;
    ov.result = "pyentry" + seq;
    ov.api = "entry" + seq;
    ov.sequence = sequence;
    const std::string nested = GetOutConversion(curType, "result", method, &ov);
    out += std::string("\n") +
           "\n"
           "      if (vecSize > " + ei + ")\n"
           "      {\n"
           "        " + lrt + " entry" + seq + " = " + api + accessor + tupleAccess[entryIndex] + "();\n"
           "        {\n"
           "          " + nested + "\n"
           "        }\n"
           "        PyTuple_SetItem(" + result + ", " + ei + ", pyentry" + seq + ");\n"
           "      }\n";
  }
  out += "\n    }";
  return out;
}

// --- in: Alternative<(A,B)> ------------------------------------------------
std::string AlternativeIn(const Bindings& b)
{
  const std::string& ltype = b.ltype;
  const std::string& api = b.api;
  Node* method = b.method;
  Sequence* sequence = b.sequence;
  const bool ispointer = SwigType_ispointer(ltype);
  const std::string accessor = ispointer ? "->" : ".";
  const std::string seq = std::to_string(sequence->Increment());
  const std::vector<std::string> altAccess = {"former", "later"};
  const std::vector<std::string> types = SwigType_templateparmlist(ltype);
  const std::string lt0 = SwigType_str(SwigType_ltype(types[0]));
  const std::string lt1 = SwigType_str(SwigType_ltype(types[1]));
  OverrideBindings ov0;
  ov0.sequence = sequence;
  const std::string conv0 = GetInConversion(types[0], "entry0_" + seq, "pyentry_" + seq, method, &ov0);
  OverrideBindings ov1;
  ov1.sequence = sequence;
  const std::string conv1 = GetInConversion(types[1], "entry1_" + seq, "pyentry_" + seq, method, &ov1);
  const std::string msg0 = SwigType_ltype(types[0]);
  const std::string msg1 = SwigType_ltype(types[1]);
  return std::string("\n") +
         "    {\n"
         "      // we need to check the parameter type and see if it matches\n"
         "      PyObject *pyentry_" + seq + " = " + b.slarg + ";\n"
         "      try\n"
         "      {\n"
         "        " + lt0 + " entry0_" + seq + ";\n"
         "        " + conv0 + "\n"
         "        " + api + accessor + altAccess[0] + "() = entry0_" + seq + ";\n"
         "      }\n"
         "      catch (const XBMCAddon::WrongTypeException&)\n"
         "      {\n"
         "        try\n"
         "        {\n"
         "          " + lt1 + " entry1_" + seq + ";\n"
         "          " + conv1 + "\n"
         "          " + api + accessor + altAccess[1] + "() = entry1_" + seq + ";\n"
         "        }\n"
         "        catch (const XBMCAddon::WrongTypeException&)\n"
         "        {\n"
         "          throw XBMCAddon::WrongTypeException(\"Failed to convert to input type to either a \"\n"
         "                                              \"" + msg0 + " or a \"\n"
         "                                              \"" + msg1 + "\" );\n"
         "        }\n"
         "      }\n"
         "    }";
}

// --- out: Alternative<(A,B)> -----------------------------------------------
std::string AlternativeOut(const Bindings& b)
{
  const std::string& typ = b.type;
  const std::string& api = b.api;
  const std::string& result = b.result;
  Node* method = b.method;
  Sequence* sequence = b.sequence;
  const std::vector<std::string> types = SwigType_templateparmlist(typ);
  const bool ispointer = SwigType_ispointer(typ);
  const std::string accessor = ispointer ? "->" : ".";
  const std::string seq = std::to_string(sequence->Increment());
  const std::vector<std::string> altAccess = {"former", "later"};
  const std::vector<std::string> altSwitch = {"first", "second"};
  const std::string nullcheck = ispointer ? (api + " != NULL && ") : "";
  std::string out = std::string("\n") +
                    "    WhichAlternative pos = " + api + accessor + "which();\n"
                    "\n"
                    "    if (" + nullcheck + "pos != XBMCAddon::none)\n"
                    "    { ";
  for (size_t entryIndex = 0; entryIndex < types.size(); ++entryIndex)
  {
    const std::string& curType = types[entryIndex];
    const std::string lrt = SwigType_str(SwigType_lrtype(curType));
    OverrideBindings ov;
    ov.api = "entry" + seq;
    ov.sequence = sequence;
    const std::string nested = GetOutConversion(curType, result, method, &ov);
    out += std::string("\n") +
           "      if (pos == XBMCAddon::" + altSwitch[entryIndex] + ")\n"
           "      {\n"
           "        " + lrt + " entry" + seq + " = " + api + accessor + altAccess[entryIndex] + "();\n"
           "        {\n"
           "          " + nested + "\n"
           "        }\n"
           "      }\n";
  }
  out += std::string("\n    }\n") +
         "    else\n"
         "      " + result + " = Py_None;";
  return out;
}

// --- out: shared_ptr / unique_ptr ------------------------------------------
std::string SmartPtrOut(const Bindings& b)
{
  const std::string& typ = b.type;
  const std::string& api = b.api;
  Node* method = b.method;
  Sequence* sequence = b.sequence;
  const std::string itype = SwigType_templateparmlist(typ)[0];
  const std::string pointertype = SwigType_makepointer(itype);
  const std::string seq = std::to_string(sequence->Increment());
  const std::string ltype = SwigType_str(SwigType_ltype(pointertype));
  OverrideBindings ov;
  ov.api = "entry" + seq;
  ov.sequence = sequence;
  const std::string nested = GetOutConversion(pointertype, "result", method, &ov);
  return std::string("\n    ") + ltype + " entry" + seq + " = " + api + ".get();\n    " + nested +
         "\n";
}

} // namespace swigbindings
