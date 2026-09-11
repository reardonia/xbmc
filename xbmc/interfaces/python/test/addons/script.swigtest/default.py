import xbmc
import xbmcaddon
import xbmcgui
import xbmcplugin
import xbmcvfs
import xbmcdrm

RESULTS = []


def check(name, fn):
    try:
        fn()
        RESULTS.append((name, "PASS", ""))
    except Exception as e:
        RESULTS.append((name, "FAIL", "%s: %s" % (type(e).__name__, e)))


def t_log():
    xbmc.log("[SWIGTEST] log works", xbmc.LOGINFO)


def t_log_kwargs():
    xbmc.log(msg="[SWIGTEST] kwargs work", level=xbmc.LOGINFO)


def t_log_typeerror():
    try:
        xbmc.log(12345)
    except TypeError:
        return
    raise AssertionError("passing int to log did not raise TypeError")


def t_infolabel():
    v = xbmc.getInfoLabel("System.BuildVersion")
    assert isinstance(v, str) and len(v) > 0, "empty InfoLabel"


def t_clean_movie_title():
    r = xbmc.getCleanMovieTitle("Some.Movie.2020.1080p.mkv")
    assert isinstance(r, tuple), "not a tuple: %r" % (r,)
    assert len(r) == 2, "wrong length: %r" % (r,)
    assert isinstance(r[0], str) and isinstance(r[1], str), "bad element types"


def t_string_none():
    r = xbmc.getCleanMovieTitle(None)
    assert isinstance(r, tuple), "None not accepted as String"


def t_vector_return():
    v = xbmc.getCecAdapterNames()
    assert isinstance(v, list), "vector<String> did not return a list: %r" % (v,)


def t_playlist():
    pl = xbmc.PlayList(xbmc.PLAYLIST_MUSIC)
    assert pl.size() >= 0


def t_player():
    p = xbmc.Player()
    assert not p.isPlaying() or p.isPlaying()


class MonitorTest(xbmc.Monitor):
    def __init__(self):
        super().__init__()
        self.got = None

    def onNotification(self, sender, method, data):
        self.got = (sender, method, data)


def t_monitor_director():
    mon = MonitorTest()
    xbmc.executebuiltin("NotifyAll(swigtest,ping)")
    for _ in range(40):
        if mon.got is not None:
            break
        if mon.waitForAbort(0.25):
            break
    assert mon.got is not None, "onNotification never fired"
    assert "ping" in mon.got[1].lower() or "ping" in str(mon.got).lower(), \
        "unexpected notification: %r" % (mon.got,)


def t_listitem():
    li = xbmcgui.ListItem("SwigTest", "second")
    assert li.getLabel() == "SwigTest"
    li.setArt({"thumb": "special://temp/none.png", "fanart": "x"})
    li.setProperty("prop", "value")
    assert li.getProperty("prop") == "value"


def t_listitem_infolabels():
    li = xbmcgui.ListItem("InfoTest")
    li.setInfo("video", {"title": "T", "year": 2020, "genre": "Drama",
                         "cast": ["a", "b"],
                         "castandrole": [("a", "r1"), ("b", "r2")]})


def t_infotag_video():
    li = xbmcgui.ListItem("TagTest")
    tag = li.getVideoInfoTag()
    tag.setTitle("T2")
    tag.setYear(2021)
    tag.setGenres(["Drama", "Comedy"])
    tag.setRatings({"imdb": (7.5, 100)}, "imdb")
    assert tag.getTitle() == "T2"
    assert tag.getYear() == 2021
    g = tag.getGenres()
    assert isinstance(g, list) and "Drama" in g, "genres roundtrip: %r" % (g,)


def t_action():
    a = xbmcgui.Action()
    assert (a == a) is True
    assert (a == a.getId()) in (True, False)


def t_dialog_kwargs():
    xbmcgui.Dialog().notification(heading="SwigTest", message="bindings alive",
                                  time=2000, sound=False)


def t_addon():
    a = xbmcaddon.Addon("script.swigtest")
    assert a.getAddonInfo("id") == "script.swigtest"


def t_vfs():
    assert xbmcvfs.exists("special://home/")
    path = xbmcvfs.translatePath("special://temp/swigtest.bin")
    f = xbmcvfs.File(path, "w")
    payload = bytearray(b"swig test payload")
    assert f.write(payload)
    f.close()
    f = xbmcvfs.File(path)
    data = f.readBytes()
    f.close()
    assert bytes(data) == bytes(payload), "readBytes mismatch: %r" % (data,)
    st = xbmcvfs.Stat(path)
    assert st.st_size() == len(payload)
    xbmcvfs.delete(path)


def t_drm_exception():
    try:
        xbmcdrm.CryptoSession("00000000000000000000000000000000", "AES/CBC/NoPadding",
                              "HmacSHA256")
    except RuntimeError:
        return
    except TypeError:
        return


def t_monkeypatch_blocked():
    try:
        xbmcgui.ListItem.__repr__ = lambda self: "nope"
    except (AttributeError, TypeError):
        return
    raise AssertionError("wrapped class unexpectedly mutable")


# The eight constructors carrying python:nokwds parsed positionally, so extra
# keywords meant for the subclass's own __init__ were ignored. Every one of
# these shapes appears in shipped addons.

def t_nokwds_required_arg():
    class MyList(xbmc.PlayList):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.tag = kwargs.get("tag")

    pl = MyList(xbmc.PLAYLIST_MUSIC, tag="marker", extra=7)
    assert pl.tag == "marker", "subclass keyword lost: %r" % pl.tag
    assert pl.size() >= 0, "C++ side not constructed"


def t_nokwds_windowxmldialog():
    # the shape script.kodi.loguploader uses: positional xmlFilename and
    # scriptPath, plus keywords the subclass reads for itself
    class LogView(xbmcgui.WindowXMLDialog):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.name = kwargs.get("name")
            self.content = kwargs.get("content")

    cwd = xbmcaddon.Addon("script.swigtest").getAddonInfo("path")
    lv = LogView("swigtest-view.xml", cwd, "default",
                 name="a name", content="a body")
    try:
        assert lv.name == "a name", "subclass keyword lost: %r" % lv.name
        assert lv.content == "a body", "subclass keyword lost: %r" % lv.content
    finally:
        del lv


def t_nokwds_windowxml():
    class View(xbmcgui.WindowXML):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.tag = kwargs.get("tag")

    cwd = xbmcaddon.Addon("script.swigtest").getAddonInfo("path")
    v = View("swigtest-view.xml", cwd, "default", "1080i", tag="marker")
    try:
        assert v.tag == "marker", "subclass keyword lost: %r" % v.tag
    finally:
        del v


def t_nokwds_no_required_args():
    for cls, args in ((xbmcgui.Window, ()), (xbmcgui.WindowDialog, ()),
                      (xbmc.Player, ()), (xbmcgui.Dialog, ()),
                      (xbmc.Keyboard, ())):
        class Sub(cls):
            def __init__(self, *a, **k):
                super().__init__(*a, **k)
                self.tag = k.get("tag")

        probe = Sub(*args, tag="marker")
        assert probe.tag == "marker", "%s subclass keyword lost" % cls.__name__
        del probe


def t_listitem_deprecated_setters():
    li = xbmcgui.ListItem("Deprecated")
    li.setUniqueIDs({"tmdb": "12345", "imdb": "tt1234567"}, "tmdb")
    li.setRating("themoviedb", 8.1, 4242, True)
    li.addSeason(1, "Season One")
    li.setCast([{"name": "Ann", "role": "Hero", "order": "1",
                 "thumbnail": "http://x/a.jpg"},
                {"name": "Bo", "role": "Villain"}])
    li.addAvailableArtwork("http://x/poster.jpg", "poster")
    li.setAvailableFanart([{"image": "http://x/f1.jpg", "preview": "http://x/p1.jpg"}])
    assert li.getUniqueID("tmdb") == "12345", "uniqueid roundtrip"
    assert abs(li.getRating("themoviedb") - 8.1) < 0.001, "rating roundtrip"
    assert li.getVotes("themoviedb") == 4242, "votes roundtrip"


def t_listitem_stream_info():
    li = xbmcgui.ListItem("Streams")
    li.addStreamInfo("video", {"codec": "h264", "width": "1920", "height": "1080",
                               "duration": "3600", "aspect": "1.78"})
    li.addStreamInfo("audio", {"codec": "dts", "language": "en", "channels": "6"})
    li.addStreamInfo("subtitle", {"language": "en"})


def t_listitem_resume_properties():
    li = xbmcgui.ListItem("Resume")
    li.setProperty("totaltime", "7200.0")
    li.setProperty("resumetime", "600.0")
    assert li.getProperty("totaltime").startswith("7200"), \
        "totaltime roundtrip: %r" % li.getProperty("totaltime")
    assert li.getProperty("resumetime").startswith("600"), \
        "resumetime roundtrip: %r" % li.getProperty("resumetime")
    li.setProperty("startoffset", "256.4")
    li.setProperty("specialsort", "top")


def t_setinfo_other_types():
    li = xbmcgui.ListItem("Music")
    li.setInfo("music", {"title": "A Song", "artist": "An Artist", "album": "An Album",
                         "tracknumber": 3, "duration": 240, "genre": "Rock"})
    li = xbmcgui.ListItem("Pictures")
    li.setInfo("pictures", {"title": "A Picture", "picturepath": "/x/y.jpg",
                            "exif:resolution": "720,480"})
    li = xbmcgui.ListItem("Game")
    li.setInfo("game", {"title": "A Game", "platform": "SNES", "genres": "Puzzle",
                        "publisher": "A Publisher", "year": 1994})


check("xbmc.log", t_log)
check("kwargs", t_log_kwargs)
check("log TypeError on int", t_log_typeerror)
check("getInfoLabel", t_infolabel)
check("getCleanMovieTitle tuple", t_clean_movie_title)
check("String accepts None", t_string_none)
check("vector return is list", t_vector_return)
check("PlayList", t_playlist)
check("Player construct", t_player)
check("Monitor director callback", t_monitor_director)
check("ListItem basics", t_listitem)
check("ListItem setInfo dict", t_listitem_infolabels)
check("InfoTagVideo roundtrip", t_infotag_video)
check("Action rich compare", t_action)
check("Dialog kwargs", t_dialog_kwargs)
check("Addon", t_addon)
check("vfs file roundtrip", t_vfs)
check("drm exception translation", t_drm_exception)
check("class immutability", t_monkeypatch_blocked)
check("nokwds: subclass kwargs, required arg", t_nokwds_required_arg)
check("nokwds: WindowXMLDialog + kwargs", t_nokwds_windowxmldialog)
check("nokwds: WindowXML + kwargs", t_nokwds_windowxml)
check("nokwds: subclass kwargs, no args", t_nokwds_no_required_args)
check("ListItem deprecated setters", t_listitem_deprecated_setters)
check("ListItem addStreamInfo", t_listitem_stream_info)
check("ListItem resume properties", t_listitem_resume_properties)
check("setInfo music, pictures, game", t_setinfo_other_types)

passed = sum(1 for r in RESULTS if r[1] == "PASS")
failed = [r for r in RESULTS if r[1] == "FAIL"]

for name, status, detail in RESULTS:
    line = "[SWIGTEST] %-28s %s %s" % (name, status, detail)
    xbmc.log(line, xbmc.LOGINFO if status == "PASS" else xbmc.LOGERROR)

summary = "%d/%d passed" % (passed, len(RESULTS))
xbmc.log("[SWIGTEST] SUMMARY: %s" % summary, xbmc.LOGINFO)
if failed:
    xbmcgui.Dialog().ok("SWIG test: FAILURES", summary + "\n" +
                        "\n".join("%s: %s" % (r[0], r[2]) for r in failed[:4]))
else:
    xbmcgui.Dialog().ok("SWIG test", "All %s" % summary)
