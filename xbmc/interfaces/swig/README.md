# Python bindings

The seven Python modules addons import (xbmc, xbmcgui, xbmcplugin, xbmcaddon,
xbmcvfs, xbmcdrm, xbmcwsgi) are generated here by stock SWIG from the
AddonModule*.i files, wrapping the C++ API in xbmc/interfaces/legacy/. The
kodi_*.i files carry the typemaps and policies shared by all modules; start
with kodi_common.i, which includes the rest in dependency order and documents
each one.

## Requirements

SWIG 4.5.0 or newer (CMakeLists.txt enforces the floor).

