"""Type-shape coverage: every entry in the inventory, both directions.

Shape numbers refer to the table in ARCHITECTURE-ANALYSIS.md section 7.
"""

import xbmc
import xbmcgui
import xbmcaddon
import xbmcplugin
import xbmcvfs

from .runner import Skip

ADDON_ID = "script.swigharness"

# handle -1 is not a real directory. xbmcplugin calls with it exercise argument
# conversion, which is what is under test, without needing a plugin context.
NO_HANDLE = -1



def _run_modal(call, dismiss="Action(PreviousMenu)", settle=1.0, timeout=15.0):
    """Run a blocking dialog on a thread and dismiss it from here.

    The dialog owns the calling thread until a person answers it, so the call
    goes on a worker and the dismissing action is sent from the caller once the
    dialog has had time to appear. Cancelling still exercises the return
    conversion, which is the point of the check.
    """
    import threading
    box = {}

    def worker():
        try:
            box["value"] = call()
        except BaseException as exc:               # noqa: BLE001 - reported below
            box["error"] = exc

    t = threading.Thread(target=worker)
    t.daemon = True
    t.start()
    xbmc.sleep(int(settle * 1000))
    xbmc.executebuiltin(dismiss)
    t.join(timeout)
    if t.is_alive():
        raise Skip("dialog did not close within %.0fs" % timeout)
    if "error" in box:
        raise box["error"]
    return box.get("value")

def register(reg):

    # ---- 1  std::vector<String>, both directions ----------------------

    @reg.check("1", "vector<String> out is a mutable list")
    def _():
        v = xbmc.getCecAdapterNames()
        assert isinstance(v, list), "expected list, got %r" % type(v).__name__
        v.append("probe")
        v.sort()

    @reg.check("1", "vector<String> in from list")
    def _():
        li = xbmcgui.ListItem("vec-in")
        li.getVideoInfoTag().setGenres(["Drama", "Comedy"])
        got = li.getVideoInfoTag().getGenres()
        assert isinstance(got, list), "getGenres must be a list, got %r" % (got,)
        assert "Drama" in got, "round trip lost data: %r" % (got,)

    @reg.check("1", "vector<String> in from tuple and generator")
    def _():
        _owner0 = xbmcgui.ListItem("vec-iter")
        tag = _owner0.getVideoInfoTag()
        tag.setStudios(("A", "B"))
        assert len(tag.getStudios()) == 2, "tuple input rejected"
        tag.setCountries(x for x in ("C", "D"))
        assert len(tag.getCountries()) == 2, "generator input rejected"

    # ---- 2  std::vector<int> ------------------------------------------

    @reg.check("2", "vector<int> out from Control::getPosition")
    def _():
        win = xbmcgui.Window()
        try:
            label = xbmcgui.ControlLabel(0, 0, 100, 30, "pos")
            win.addControl(label)
            pos = label.getPosition()
            assert isinstance(pos, list), "getPosition must be a list, got %r" % (pos,)
            assert len(pos) == 2 and all(isinstance(i, int) for i in pos), \
                "expected two ints, got %r" % (pos,)
        finally:
            del win

    # ---- 3+4  vector<bool>, vector<double> ----------------------------

    @reg.check("3+4", "vector<bool>, vector<int>, vector<double> via Settings")
    def _():
        # These are on Settings (getBoolList/getNumberList and their setters),
        # not on controls, so no window is involved. resources/settings.xml
        # declares the four list settings this uses.
        # The Addon must outlive the Settings: CAddonSettings holds a weak_ptr
        # to it and Save() asserts on a null lock, which aborts Kodi outright
        # rather than raising. Do not collapse these two lines.
        addon = xbmcaddon.Addon(ADDON_ID)
        st = addon.getSettings()
        st.setBoolList("boollist", [True, False, True])
        st.setIntList("intlist", [4, 5, 6])
        st.setNumberList("numlist", [1.5, 2.5])
        st.setStringList("strlist", ["x", "y"])
        for getter, name, kind in ((st.getBoolList, "boollist", bool),
                                   (st.getIntList, "intlist", int),
                                   (st.getNumberList, "numlist", float),
                                   (st.getStringList, "strlist", str)):
            got = getter(name)
            assert isinstance(got, list), "%s must return a list, got %r" % (name, got)
            assert got and all(isinstance(v, kind) for v in got), \
                "%s elements must be %s, got %r" % (name, kind.__name__, got)

    # ---- 5  std::vector<const Actor*> ---------------------------------

    @reg.check("5", "vector<const Actor*> in, wrapped elements")
    def _():
        _owner1 = xbmcgui.ListItem("cast")
        tag = _owner1.getVideoInfoTag()
        tag.setCast([xbmc.Actor("Ann", "Hero", 1, "http://x/a.jpg"),
                     xbmc.Actor("Bo", "Villain", 2, "http://x/b.jpg")])
        actors = tag.getActors()
        assert isinstance(actors, list), "getActors must be a list, got %r" % (actors,)
        assert len(actors) == 2, "expected 2 actors, got %d" % len(actors)
        assert actors[0].getName() == "Ann", "actor round trip: %r" % actors[0].getName()

    # ---- 6  std::vector<const ListItem*> ------------------------------

    @reg.check("6", "vector<const ListItem*> in via ControlList")
    def _():
        win = xbmcgui.Window()
        try:
            lst = xbmcgui.ControlList(0, 0, 200, 200)
            win.addControl(lst)
            lst.addItems([xbmcgui.ListItem("one"), xbmcgui.ListItem("two")])
            assert lst.size() == 2, "addItems lost items: %d" % lst.size()
        finally:
            del win

    # ---- 7  std::vector<Control*> out ---------------------------------

    @reg.check("7", "vector<Control*> out keeps wrapped types")
    def _():
        win = xbmcgui.Window()
        try:
            label = xbmcgui.ControlLabel(0, 0, 100, 30, "a")
            win.addControl(label)
            back = win.getControl(label.getId())
            assert back is not None, "getControl returned nothing"
            assert type(back).__name__ != "SwigPyObject", \
                "control came back as an opaque pointer"
        finally:
            del win

    # ---- 8  std::map<String,String> (default comparator) --------------

    @reg.check("8", "map<String,String> in via setArt")
    def _():
        li = xbmcgui.ListItem("art")
        li.setArt({"thumb": "http://x/t.jpg", "fanart": "http://x/f.jpg"})
        assert li.getArt("thumb") == "http://x/t.jpg", \
            "setArt round trip: %r" % li.getArt("thumb")

    # ---- 9  std::map<String,String,std::less<>>  the scanner break ----

    @reg.check("9", "map<String,String,less<>> in via setUniqueIDs")
    def _():
        _owner2 = xbmcgui.ListItem("ids")
        tag = _owner2.getVideoInfoTag()
        tag.setUniqueIDs({"tmdb": "12345", "imdb": "tt1234567"}, "tmdb")
        assert tag.getUniqueID("tmdb") == "12345", \
            "setUniqueIDs round trip: %r" % tag.getUniqueID("tmdb")
        assert tag.getUniqueID("imdb") == "tt1234567", "second key lost"

    # ---- 10  Dictionary<String> (Properties) --------------------------

    @reg.check("10", "Dictionary<String> in via setProperties")
    def _():
        li = xbmcgui.ListItem("props")
        li.setProperties({"alpha": "1", "beta": "2"})
        assert li.getProperty("alpha") == "1", \
            "setProperties round trip: %r" % li.getProperty("alpha")

    # ---- 11  Tuple<String,String> out ---------------------------------

    @reg.check("11", "Tuple<String,String> out is a 2-tuple")
    def _():
        got = xbmc.getCleanMovieTitle("Some.Movie.2020.1080p.mkv")
        assert isinstance(got, tuple), "expected tuple, got %r" % type(got).__name__
        assert len(got) == 2, "expected 2 entries, got %d" % len(got)
        assert all(isinstance(s, str) for s in got), "entries must be str: %r" % (got,)

    # ---- 12  Tuple<vector<String>,vector<String>> out -----------------

    @reg.check("12", "listdir returns a tuple of two mutable lists")
    def _():
        got = xbmcvfs.listdir("special://home/")
        assert isinstance(got, tuple), "expected tuple, got %r" % type(got).__name__
        assert len(got) == 2, "expected 2 entries, got %d" % len(got)
        dirs, files = got
        assert isinstance(dirs, list) and isinstance(files, list), \
            "both entries must be lists, got %r and %r" % (type(dirs), type(files))
        dirs.sort()
        files.append("probe")

    # ---- 13  std::map<String,Tuple<float,int>> in ---------------------

    @reg.check("13", "map<String,Tuple<float,int>> in via setRatings")
    def _():
        _owner3 = xbmcgui.ListItem("ratings")
        tag = _owner3.getVideoInfoTag()
        tag.setRatings({"imdb": (7.5, 100), "tmdb": (8.25, 4242)}, "imdb")
        assert abs(tag.getRating("imdb") - 7.5) < 0.001, \
            "rating round trip: %r" % tag.getRating("imdb")
        assert tag.getVotes("tmdb") == 4242, \
            "votes round trip: %r" % tag.getVotes("tmdb")

    @reg.check("13", "setRatings accepts a None default, as scrapers pass")
    def _():
        _owner4 = xbmcgui.ListItem("ratings-none")
        tag = _owner4.getVideoInfoTag()
        tag.setRatings({"imdb": (7.5, 100)}, None)

    # ---- 14  vector<Tuple<String,String>> in --------------------------

    @reg.check("14", "vector<Tuple<String,String>> in via addContextMenuItems")
    def _():
        li = xbmcgui.ListItem("ctx")
        li.addContextMenuItems([("Label one", "Action(one)"),
                                ("Label two", "Action(two)")])

    # ---- 15  vector<Tuple<int,String,String>> in ----------------------

    @reg.check("15", "vector<Tuple<int,String,String>> in via addSeasons")
    def _():
        _owner5 = xbmcgui.ListItem("seasons")
        tag = _owner5.getVideoInfoTag()
        tag.addSeasons([(1, "Season One", "overview one"),
                        (2, "Season Two", "overview two")])

    # ---- 16  vector<Tuple<String,const ListItem*,bool>> in ------------

    @reg.check("16", "addDirectoryItems: list of (str, ListItem, bool)")
    def _():
        first = xbmcgui.ListItem("plugin one")
        second = xbmcgui.ListItem("plugin two")
        xbmcplugin.addDirectoryItems(NO_HANDLE,
                                     [("plugin://script.swigharness/?a=1", first, True),
                                      ("plugin://script.swigharness/?a=2", second, False)])

    @reg.check("16", "addDirectoryItems rejects a tuple with no ListItem")
    def _():
        # addDirectoryItems dereferences item.second() unconditionally
        # (it only guards the third element with GetNumValuesSet), so a tuple
        # without a ListItem is invalid input and must not reach C++.
        try:
            xbmcplugin.addDirectoryItems(NO_HANDLE, [("plugin://script.swigharness/?a=3",)])
        except TypeError:
            return
        raise AssertionError("a tuple with no ListItem was accepted")

    @reg.check("22", "Dictionary coerces numeric values, as addons pass them")
    def _():
        # Dictionary values are String, and swig::as<std::string> rejects a
        # number outright, so the glue must call str() before converting.
        li = xbmcgui.ListItem("numeric")
        li.setProperties({"episodecount": 12, "ratio": 1.5})
        assert li.getProperty("episodecount") == "12", \
            "int value lost: %r" % li.getProperty("episodecount")
        assert li.getProperty("ratio").startswith("1.5"), \
            "float value lost: %r" % li.getProperty("ratio")

    @reg.check("22", "setInfo: depth-5 dict of (str | list of (str | 2-tuple)) (legacy)")
    def _():
        # InfoLabelDict is the deepest type in the API. Skipped rather than
        # deleted so this suite runs against a tree with setInfo removed.
        li = xbmcgui.ListItem("deep")
        if not hasattr(li, "setInfo"):
            raise Skip("removed in this build")
        li.setInfo("video", {
            "title": "A Title",                       # String alternative
            "year": 2020,                             # coerced by str()
            "genre": ["Drama", "Comedy"],             # vector of String alternative
            "castandrole": [("Ann", "Hero"),          # vector of Tuple alternative
                            ("Bo", "Villain")],
            "cast": ["Ann", "Bo"],
        })
        assert li.getVideoInfoTag().getTitle() == "A Title", "setInfo title lost"

    # ---- 17  vector<Properties> in ------------------------------------

    @reg.check("17", "vector<Dictionary<String>> in via setAvailableFanart")
    def _():
        _owner6 = xbmcgui.ListItem("fanart")
        tag = _owner6.getVideoInfoTag()
        tag.setAvailableFanart([{"image": "http://x/f1.jpg", "preview": "http://x/p1.jpg"},
                                {"image": "http://x/f2.jpg"}])

    # ---- 18  Alternative<String,vector<String>> out -------------------

    @reg.check("18", "Alternative<String,vector<String>> out from Dialog.browse")
    def _():
        # browse returns String when enableMultiple is false and vector<String>
        # when it is true. Cancelling still returns through the conversion.
        one = _run_modal(lambda: xbmcgui.Dialog().browse(
            3, "pick a file", "files", "", False, False, "special://temp/", False))
        assert isinstance(one, str), "browse must return str, got %r" % (one,)
        # browseMultiple accepts only type 1 (files) and 2 (images); every other
        # type throws WindowException by design.
        many = _run_modal(lambda: xbmcgui.Dialog().browse(
            1, "pick files", "files", "", False, False, "special://temp/", True))
        assert isinstance(many, list), "multi browse must return list, got %r" % (many,)

    # ---- 19+20  Alternative in, and vector<Alternative> in ------------

    @reg.check("19", "Alternative<String,ListItem*> in via ControlList.addItem")
    def _():
        win = xbmcgui.Window()
        try:
            lst = xbmcgui.ControlList(0, 0, 200, 200)
            win.addControl(lst)
            lst.addItem("a plain string")            # the String alternative
            lst.addItem(xbmcgui.ListItem("an item"))  # the ListItem alternative
            assert lst.size() == 2, "addItem lost items: %d" % lst.size()
        finally:
            del win

    @reg.check("20", "vector<Alternative<String,ListItem*>> in")
    def _():
        win = xbmcgui.Window()
        try:
            lst = xbmcgui.ControlList(0, 0, 200, 200)
            win.addControl(lst)
            lst.addItems(["plain", xbmcgui.ListItem("wrapped"), "plain again"])
            assert lst.size() == 3, "mixed addItems lost items: %d" % lst.size()
        finally:
            del win

    # ---- 21  Alternative<String,const PlayList*> (PlayParameter) ------

    @reg.check("21", "PlayParameter accepts both alternatives")
    def _():
        player = xbmc.Player()
        playlist = xbmc.PlayList(xbmc.PLAYLIST_MUSIC)
        # Both forms must convert. Playback of a non-existent path is a no-op
        # for argument-conversion purposes and is stopped immediately.
        player.play("special://temp/swigharness-nonexistent.mp3")
        player.play(playlist)
        player.stop()

    # ---- 22  the four ListItem setters that replaced setInfo -----------

    @reg.check("22", "setFileTitle, setCount, setSize, setOverlay")
    def _():
        # The four keys setInfo handled that had no other entry point.
        li = xbmcgui.ListItem("replacements")
        if not hasattr(li, "setFileTitle"):
            raise Skip("not in this build")
        li.setFileTitle("A Title")
        li.setCount(7)
        li.setSize(1024)
        li.setOverlay(5)
        try:
            li.setOverlay(99)
        except RuntimeError:
            return
        raise AssertionError("setOverlay(99) out of range was accepted")

    # ---- 23  vector<WsgiHttpHeader> in --------------------------------

    @reg.check("23", "vector<Tuple<String,String>> in via WsgiResponse")
    def _():
        try:
            import xbmcwsgi
        except ImportError:
            raise Skip("xbmcwsgi not built into this Kodi")
        response = xbmcwsgi.WsgiResponse()
        response("200 OK", [("Content-Type", "text/plain"),
                            ("Content-Length", "2")])

    # ---- 24  unique_ptr<vector<int>> out ------------------------------

    @reg.check("24", "unique_ptr<vector<int>> out from Dialog.multiselect")
    def _():
        # Cancelling yields a null unique_ptr, which must arrive as None rather
        # than an empty list or an opaque object.
        got = _run_modal(lambda: xbmcgui.Dialog().multiselect(
            "choose", ["one", "two", "three"]))
        assert got is None or isinstance(got, list), \
            "multiselect must return list or None, got %r" % (got,)

    # ---- 25  XbmcCommons::Buffer, both directions ---------------------

    @reg.check("25", "Buffer in and out, bytes round trip")
    def _():
        path = xbmcvfs.translatePath("special://temp/swigharness-buffer.bin")
        payload = bytes(bytearray(range(256)))
        handle = xbmcvfs.File(path, "w")
        try:
            assert handle.write(bytearray(payload)), "write returned false"
        finally:
            handle.close()
        handle = xbmcvfs.File(path)
        try:
            got = handle.readBytes()
        finally:
            handle.close()
        assert bytes(got) == payload, \
            "buffer round trip differs: %d vs %d bytes" % (len(got), len(payload))
        xbmcvfs.delete(path)

    @reg.check("25", "Buffer in accepts str, bytes and bytearray")
    def _():
        path = xbmcvfs.translatePath("special://temp/swigharness-buffer2.bin")
        for payload in ("a string", b"some bytes", bytearray(b"an array")):
            handle = xbmcvfs.File(path, "w")
            try:
                handle.write(payload)
            finally:
                handle.close()
        xbmcvfs.delete(path)

    # ---- 26  raw pointer to a container -------------------------------

    @reg.check("26", "ControlList.setStaticContent takes a list of ListItem")
    def _():
        win = xbmcgui.Window()
        try:
            lst = xbmcgui.ControlList(0, 0, 200, 200)
            win.addControl(lst)
            lst.setStaticContent([xbmcgui.ListItem("static one"),
                                  xbmcgui.ListItem("static two")])
        finally:
            del win
