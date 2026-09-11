"""Contracts that are not about a single type shape.

Keyword arguments, None handling, object lifetime and identity, most-derived
return types, cross-module types, overloads, operators, properties, constants
and error behaviour.
"""

import gc

import xbmc
import xbmcaddon
import xbmcgui
import xbmcplugin
import xbmcvfs

from .runner import Skip

ADDON_ID = "script.swigharness"


def _raises(fn, exc=Exception):
    try:
        fn()
    except exc:
        return True
    except Exception:
        return False
    return False


def register(reg):

    # ---- keyword arguments, in the exact patterns the scrapers use ----

    @reg.check("-", "kwargs: xbmc.log(msg=, level=)")
    def _():
        xbmc.log(msg="[SWIGHARNESS] kwargs on a module function", level=xbmc.LOGINFO)

    @reg.check("-", "kwargs: ListItem(label, offscreen=True)")
    def _():
        li = xbmcgui.ListItem("kw", offscreen=True)
        assert li.getLabel() == "kw", "label lost: %r" % li.getLabel()

    @reg.check("-", "kwargs: addDirectoryItem(handle=, url=, listitem=, isFolder=)")
    def _():
        xbmcplugin.addDirectoryItem(handle=-1,
                                    url="plugin://script.swigharness/?kw=1",
                                    listitem=xbmcgui.ListItem("kw item"),
                                    isFolder=True)

    @reg.check("-", "kwargs: setResolvedUrl(handle=, succeeded=, listitem=)")
    def _():
        xbmcplugin.setResolvedUrl(handle=-1, succeeded=False,
                                  listitem=xbmcgui.ListItem("kw resolved"))

    @reg.check("-", "kwargs: setUniqueID(value, type=, isdefault=)")
    def _():
        _owner0 = xbmcgui.ListItem("kw-uid")
        tag = _owner0.getVideoInfoTag()
        tag.setUniqueID("12345", type="tmdb", isdefault=True)
        assert tag.getUniqueID("tmdb") == "12345", "kwargs setUniqueID round trip"

    @reg.check("-", "kwargs: setRating(rating, votes=, type=, isdefault=)")
    def _():
        _owner1 = xbmcgui.ListItem("kw-rating")
        tag = _owner1.getVideoInfoTag()
        tag.setRating(8.5, votes=99, type="tmdb", isdefault=True)
        assert abs(tag.getRating("tmdb") - 8.5) < 0.001, "kwargs setRating round trip"

    @reg.check("-", "kwargs: addAvailableArtwork(url, arttype=, preview=, season=)")
    def _():
        _owner2 = xbmcgui.ListItem("kw-art")
        tag = _owner2.getVideoInfoTag()
        tag.addAvailableArtwork("http://x/p.jpg", arttype="poster",
                                preview="http://x/pp.jpg", season=1)

    @reg.check("-", "defaulted arguments may be omitted")
    def _():
        xbmc.getCleanMovieTitle("Movie.2020.mkv")            # usefoldername omitted
        xbmcgui.ListItem()                                    # every argument omitted

    # ---- None where a String is expected ------------------------------

    @reg.check("-", "None accepted where String is expected")
    def _():
        got = xbmc.getCleanMovieTitle(None)
        assert isinstance(got, tuple), "None as String broke the call: %r" % (got,)

    @reg.check("-", "None accepted by a setter, as scrapers pass it")
    def _():
        _owner3 = xbmcgui.ListItem("none-setter")
        tag = _owner3.getVideoInfoTag()
        tag.setOriginalTitle(None)
        tag.setTitle(None)

    @reg.check("-", "None accepted as a dict value")
    def _():
        li = xbmcgui.ListItem("none-dict")
        li.setArt({"thumb": "http://x/t.jpg", "poster": None})
        assert li.getArt("thumb") == "http://x/t.jpg", li.getArt("thumb")
        assert li.getArt("poster") == "", repr(li.getArt("poster"))
        li.setProperties({"alpha": "1", "beta": None})
        assert li.getProperty("beta") == "", repr(li.getProperty("beta"))

    @reg.check("-", "None accepted as a list element")
    def _():
        li = xbmcgui.ListItem("none-list")
        li.getVideoInfoTag().setStudios(["a", None])

    @reg.check("-", "None accepted inside setCast and setInfo")
    def _():
        li = xbmcgui.ListItem("none-nested")
        li.setCast([{"name": "A", "role": "B", "order": 1, "thumbnail": None}])
        li.setInfo("video", {"title": "x", "plot": None})

    # ---- object lifetime and identity ---------------------------------

    @reg.check("-", "getVideoInfoTag mutations stick and are visible again")
    def _():
        li = xbmcgui.ListItem("borrowed")
        first = li.getVideoInfoTag()
        first.setTitle("mutated through the first handle")
        second = li.getVideoInfoTag()
        assert second.getTitle() == "mutated through the first handle", \
            "mutation lost between handles: %r" % second.getTitle()

    @reg.check("-", "borrowed tag survives its python handle being dropped")
    def _():
        li = xbmcgui.ListItem("borrowed-drop")
        tag = li.getVideoInfoTag()
        tag.setYear(1999)
        del tag
        gc.collect()
        assert li.getVideoInfoTag().getYear() == 1999, \
            "owning ListItem lost data after the tag handle was dropped"

    @reg.check("-", "a Window can be created and dropped repeatedly")
    def _():
        for _i in range(5):
            win = xbmcgui.Window()
            assert win.getWidth() >= 0, "window has no geometry"
            del win
            gc.collect()

    # ---- most-derived return type (the RTTI path) ---------------------

    @reg.check("-", "getControl returns the concrete Control subclass")
    def _():
        win = xbmcgui.Window()
        try:
            label = xbmcgui.ControlLabel(0, 0, 100, 30, "typed")
            button = xbmcgui.ControlButton(0, 40, 100, 30, "pressme")
            win.addControl(label)
            win.addControl(button)
            got_label = win.getControl(label.getId())
            got_button = win.getControl(button.getId())
            assert type(got_label).__name__ == "ControlLabel", \
                "expected ControlLabel, got %r" % type(got_label).__name__
            assert type(got_button).__name__ == "ControlButton", \
                "expected ControlButton, got %r" % type(got_button).__name__
            # a method that exists only on the derived type must work
            got_label.getLabel()
        finally:
            del win

    @reg.check("-", "ControlList.getSelectedItem returns a ListItem")
    def _():
        win = xbmcgui.Window()
        try:
            lst = xbmcgui.ControlList(0, 0, 200, 200)
            win.addControl(lst)
            lst.addItem(xbmcgui.ListItem("selectable"))
            got = lst.getSelectedItem()
            if got is None:
                raise Skip("no selection in a window that was never shown")
            assert type(got).__name__ == "ListItem", \
                "expected ListItem, got %r" % type(got).__name__
        finally:
            del win

    # ---- cross-module types -------------------------------------------

    @reg.check("-", "xbmcgui ListItem is accepted by xbmcplugin")
    def _():
        li = xbmcgui.ListItem("cross-module")
        xbmcplugin.addDirectoryItem(-1, "plugin://script.swigharness/?x=1", li, False)
        xbmcplugin.setResolvedUrl(-1, False, li)

    @reg.check("-", "InfoTag types cross from xbmcgui into xbmc")
    def _():
        li = xbmcgui.ListItem("tags")
        pairs = (("InfoTagVideo", li.getVideoInfoTag),
                 ("InfoTagMusic", li.getMusicInfoTag),
                 ("InfoTagPicture", li.getPictureInfoTag),
                 ("InfoTagGame", li.getGameInfoTag))
        for expected, getter in pairs:
            tag = getter()
            name = type(tag).__name__
            assert name != "SwigPyObject", \
                "%s came back as an opaque pointer" % expected
            assert name == expected, \
                "expected %s, got %s" % (expected, name)
            assert isinstance(getattr(xbmc, expected, None), type), \
                "xbmc does not export %s, so the type is not shared" % expected

    @reg.check("-", "an Actor built in xbmc is accepted by xbmcgui")
    def _():
        _owner4 = xbmcgui.ListItem("actor-cross")
        tag = _owner4.getVideoInfoTag()
        tag.setCast([xbmc.Actor("Cross", "Module", 1, "http://x/c.jpg")])
        assert tag.getActors()[0].getName() == "Cross", "cross-module Actor lost"

    # ---- overloads, operators, properties, constants ------------------

    @reg.check("-", "PlayList supports len() and indexing")
    def _():
        pl = xbmc.PlayList(xbmc.PLAYLIST_MUSIC)
        size = len(pl)
        assert size == pl.size(), "len() and size() disagree: %d vs %d" % (size, pl.size())
        if size:
            assert pl[0] is not None, "index access returned nothing"

    @reg.check("-", "Action equals itself")
    def _():
        action = xbmcgui.Action()
        assert (action == action) is True, "an Action must equal itself"

    @reg.check("-", "Action compares equal to its own id (backward compat)")
    def _():
        # The shipped bindings implement this through Kodi's own
        # %feature("python:rcmp") block in AddonModuleXbmcgui.i, which falls
        # back to comparing a1->id when the other operand is not an Action.
        # That feature name is Kodi-invented; stock SWIG stores it and never
        # emits it, so this is the one silent behaviour change found so far.
        action = xbmcgui.Action()
        assert (action == action.getId()) is True, \
            "Action == int is False; the rcmp backward-compatibility branch was lost"

    @reg.check("-", "InfoTagVideo exposes readable properties")
    def _():
        _owner5 = xbmcgui.ListItem("props")
        tag = _owner5.getVideoInfoTag()
        tag.setTitle("Property Probe")
        assert tag.getTitle() == "Property Probe", "property round trip"

    @reg.check("-", "module constants are present and typed")
    def _():
        for name in ("LOGINFO", "LOGERROR", "PLAYLIST_MUSIC", "PLAYLIST_VIDEO"):
            assert hasattr(xbmc, name), "xbmc.%s missing" % name
            assert isinstance(getattr(xbmc, name), int), "xbmc.%s is not an int" % name
        for name in ("ACTION_MOVE_LEFT", "NOTIFICATION_INFO", "INPUT_ALPHANUM"):
            assert hasattr(xbmcgui, name), "xbmcgui.%s missing" % name
        for name in ("SORT_METHOD_LABEL", "SORT_METHOD_VIDEO_YEAR"):
            assert hasattr(xbmcplugin, name), "xbmcplugin.%s missing" % name
        assert xbmc.__version__, "xbmc.__version__ missing"

    @reg.check("-", "Addon and its typed getters")
    def _():
        addon = xbmcaddon.Addon(ADDON_ID)
        assert addon.getAddonInfo("id") == ADDON_ID, "getAddonInfo mismatch"
        assert isinstance(addon.getLocalizedString(24006), str), \
            "getLocalizedString must return str"

    @reg.check("-", "Addon() with no id resolves through the language hook")
    def _():
        assert xbmcaddon.Addon().getAddonInfo("id") == ADDON_ID, \
            "the language hook did not resolve the running addon"

    @reg.check("-", "unicode survives the round trip")
    def _():
        # built with chr() so this source file stays ASCII
        label = "caf" + chr(0xE9) + " " + chr(0x4E2D) + chr(0x6587)
        li = xbmcgui.ListItem(label)
        assert li.getLabel() == label, "unicode round trip: %r" % li.getLabel()

    # ---- error behaviour ----------------------------------------------

    @reg.check("-", "wrong argument type raises TypeError")
    def _():
        assert _raises(lambda: xbmc.log(object()), TypeError), \
            "passing an object where a String is expected must raise TypeError"

    @reg.check("-", "wrong element type in a container raises TypeError")
    def _():
        _owner6 = xbmcgui.ListItem("bad-elem")
        tag = _owner6.getVideoInfoTag()
        assert _raises(lambda: tag.setCast([object(), object()]), TypeError), \
            "a list of the wrong element type must raise TypeError"

    @reg.check("-", "a non-sequence where a sequence is expected raises TypeError")
    def _():
        _owner7 = xbmcgui.ListItem("bad-seq")
        tag = _owner7.getVideoInfoTag()
        assert _raises(lambda: tag.setGenres(42), TypeError), \
            "an int where a list is expected must raise TypeError"

    @reg.check("-", "a C++ exception surfaces as RuntimeError")
    def _():
        try:
            import xbmcdrm
        except ImportError:
            raise Skip("xbmcdrm not built into this Kodi")
        assert _raises(lambda: xbmcdrm.CryptoSession("not-a-uuid", "AES/CBC/NoPadding",
                                                     "HmacSHA256"),
                       (RuntimeError, TypeError)), \
            "a failing C++ constructor must raise, not crash"

    @reg.check("-", "a class with no public constructor cannot be built")
    def _():
        assert _raises(lambda: xbmcgui.Control(), TypeError), \
            "Control has no public constructor and must not be constructible"

    # ---- vfs basics used by the shape checks --------------------------

    @reg.check("-", "vfs exists/mkdir/delete round trip")
    def _():
        base = "special://temp/swigharness-dir"
        if not xbmcvfs.exists(base):
            assert xbmcvfs.mkdir(base), "mkdir failed"
        assert xbmcvfs.exists(base), "directory missing after mkdir"
        xbmcvfs.rmdir(base)

    @reg.check("-", "File works as a context manager, as addons write files")
    def _():
        path = "special://temp/swigharness-ctx.txt"
        with xbmcvfs.File(path, "w") as f:
            f.write("hello")
        with xbmcvfs.File(path) as f:
            assert f.read() == "hello", repr(f.read())
        xbmcvfs.delete(path)
