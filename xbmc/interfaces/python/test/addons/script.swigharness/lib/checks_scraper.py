"""The exact call sequences the bundled scrapers make.

Transcribed from addons/metadata.themoviedb.org.python/python/scraper.py and
addons/metadata.tvshows.themoviedb.org.python/libs/data_utils.py. If these
pass, the scrapers that broke the previous attempt will run.
"""

import xbmc
import xbmcaddon
import xbmcgui
import xbmcplugin


def register(reg):

    @reg.check("-", "movie scraper: search result to ListItem")
    def _():
        # _searchresult_to_listitem
        movie = {"title": "A Movie", "release_date": "2020-05-01",
                 "poster_path": "http://x/p.jpg", "id": 12345}
        label = "%s (%s)" % (movie["title"], movie["release_date"].split("-")[0])
        item = xbmcgui.ListItem(label, offscreen=True)
        tag = item.getVideoInfoTag()
        tag.setTitle(movie["title"])
        tag.setYear(int(movie["release_date"].split("-")[0]))
        item.setArt({"thumb": movie["poster_path"]})
        xbmcplugin.addDirectoryItem(handle=-1, url='{"tmdb": "12345"}',
                                    listitem=item, isFolder=True)

    @reg.check("-", "movie scraper: full get_details sequence")
    def _():
        info = {
            "title": "A Movie", "originaltitle": "Le Movie",
            "plot": "A plot.", "tagline": "A tagline.",
            "studio": ["Studio One", "Studio Two"],
            "genre": ["Drama", "Comedy"],
            "country": ["Somewhere"],
            "credits": ["A Writer"],
            "director": ["A Director"],
            "premiered": "2020-05-01",
            "tag": ["a tag"], "mpaa": "PG-13",
            "trailer": "plugin://plugin.video.youtube/play/?video_id=x",
            "set": "A Set", "setoverview": "Set overview",
            "duration": 7200, "top250": 42,
        }
        item = xbmcgui.ListItem(info["title"], offscreen=True)
        tag = item.getVideoInfoTag()

        # set_info()
        tag.setTitle(info["title"])
        tag.setOriginalTitle(info["originaltitle"])
        tag.setPlot(info["plot"])
        tag.setTagLine(info["tagline"])
        tag.setStudios(info["studio"])
        tag.setGenres(info["genre"])
        tag.setCountries(info["country"])
        tag.setWriters(info["credits"])
        tag.setDirectors(info["director"])
        tag.setPremiered(info["premiered"])
        tag.setTags(info["tag"])
        tag.setMpaa(info["mpaa"])
        tag.setTrailer(info["trailer"])
        tag.setSet(info["set"])
        tag.setSetOverview(info["setoverview"])
        tag.setDuration(info["duration"])
        tag.setTop250(info["top250"])

        # build_cast()
        cast = [{"name": "Ann", "role": "Hero", "order": 1, "thumbnail": "http://x/a.jpg"},
                {"name": "Bo", "role": "Villain", "order": 2, "thumbnail": "http://x/b.jpg"}]
        tag.setCast([xbmc.Actor(c["name"], c["role"], c["order"], c["thumbnail"])
                     for c in cast])

        # setUniqueIDs, the call that broke the scanner
        tag.setUniqueIDs({"tmdb": "12345", "imdb": "tt1234567"}, "tmdb")

        # build_ratings() plus find_defaultrating(), which can return None
        ratings = {"themoviedb": {"rating": 8.1, "votes": 4242, "default": True},
                   "imdb": {"rating": 7.9, "votes": 999, "default": False}}
        built = {k: (v["rating"], v.get("votes", 0)) for k, v in ratings.items()}
        default = next((k for k, v in ratings.items() if v["default"]), None)
        tag.setRatings(built, default)

        # add_artworks()
        artworks = {"poster": [{"url": "http://x/p1.jpg"}, {"url": "http://x/p2.jpg"}],
                    "fanart": [{"url": "http://x/f1.jpg", "preview": "http://x/fp1.jpg"}]}
        for arttype, artlist in artworks.items():
            if arttype == "fanart":
                continue
            for image in artlist[:10]:
                tag.addAvailableArtwork(image["url"], arttype)
        tag.setAvailableFanart([{"image": i["url"], "preview": i["preview"]}
                                for i in artworks.get("fanart", ())[:10]])

        xbmcplugin.setResolvedUrl(handle=-1, succeeded=True, listitem=item)

        assert tag.getUniqueID("tmdb") == "12345", "uniqueids did not stick"
        assert abs(tag.getRating("themoviedb") - 8.1) < 0.001, "ratings did not stick"
        assert tag.getActors()[0].getName() == "Ann", "cast did not stick"

    @reg.check("-", "tv scraper: add_main_show_info sequence")
    def _():
        item = xbmcgui.ListItem("A Show", offscreen=True)
        tag = item.getVideoInfoTag()
        tag.setTitle("A Show")
        tag.setOriginalTitle("Le Show")
        tag.setOriginalLanguage("fr")
        tag.setTvShowTitle("A Show")
        tag.setPlot("A plot.")
        tag.setPlotOutline("A plot.")
        tag.setTagLine("A tagline.")
        tag.setMediaType("tvshow")

        # _set_unique_ids uses the kwargs form
        tag.setUniqueID("12345", type="tmdb", isdefault=True)
        tag.setUniqueID("tt1234567", type="imdb", isdefault=False)
        tag.setEpisodeGuide('{"tmdb": "12345"}')

        tag.setYear(2020)
        tag.setPremiered("2020-05-01")
        tag.setTvShowStatus("Continuing")
        tag.setGenres(["Drama"])
        tag.setTags(["a tag"])
        tag.setStudios(["A Network (US)"])
        tag.setCountries(["US"])
        tag.setMpaa("TV-14")
        tag.setWriters(["A Writer"])
        tag.setTrailer("plugin://plugin.video.youtube/play/?video_id=x")

        # _add_season_info, with the kwargs form of addAvailableArtwork
        tag.addSeason(1, "Season One", "overview one")
        tag.addAvailableArtwork("http://x/s1.jpg", arttype="poster",
                                preview="http://x/s1p.jpg", season=1)

        # set_show_artwork
        tag.addAvailableArtwork("http://x/land.jpg", arttype="landscape",
                                preview="http://x/landp.jpg")
        tag.setAvailableFanart([{"image": "http://x/f1.jpg"}])

        # _set_cast, _set_rating
        tag.setCast([xbmc.Actor("Ann", "Hero", 1, "http://x/a.jpg")])
        tag.setRating(8.4, votes=1234, type="tmdb", isdefault=True)

        assert tag.getUniqueID("tmdb") == "12345", "tv uniqueids did not stick"

    @reg.check("-", "tv scraper: add_episode_info sequence")
    def _():
        item = xbmcgui.ListItem("An Episode", offscreen=True)
        tag = item.getVideoInfoTag()
        tag.setTitle("An Episode")
        tag.setSeason(1)
        tag.setEpisode(2)
        tag.setMediaType("episode")
        tag.setFirstAired("2020-05-08")
        tag.setPlot("Episode plot.")
        tag.setPlotOutline("Episode plot.")
        tag.setPremiered("2020-05-08")
        tag.setDuration(45 * 60)
        tag.setCast([xbmc.Actor("Bo", "Guest", 1, "http://x/b.jpg")])
        tag.setUniqueID("999", type="tmdb", isdefault=True)
        tag.addAvailableArtwork("http://x/still.jpg", arttype="thumb",
                                preview="http://x/stillp.jpg")
        tag.setWriters(["A Writer"])
        tag.setDirectors(["A Director"])
        assert tag.getSeason() == 1 and tag.getEpisode() == 2, "episode numbers lost"

    @reg.check("-", "music scraper: addDirectoryItems with (url, item, bool)")
    def _():
        items = []
        for name in ("Album One", "Album Two"):
            item = xbmcgui.ListItem(name, offscreen=True)
            item.setArt({"thumb": "http://x/%s.jpg" % name})
            item.setProperty("album.artist", "An Artist")
            item.setProperty("relevance", "0.9")
            items.append(('{"album": "%s"}' % name, item, True))
        xbmcplugin.addDirectoryItems(handle=-1, items=items)

    @reg.check("-", "scraper settings access through Addon")
    def _():
        addon = xbmcaddon.Addon()
        # the four typed getters PathSpecificSettings shims
        for getter, kind in ((addon.getSettingBool, bool),
                             (addon.getSettingInt, int),
                             (addon.getSettingNumber, float),
                             (addon.getSettingString, str)):
            try:
                value = getter("a_setting_that_does_not_exist")
            except Exception:
                continue  # a missing setting may legitimately raise
            assert isinstance(value, kind), \
                "%s returned %r" % (getter.__name__, type(value).__name__)

    @reg.check("-", "Dialog.notification with keyword arguments")
    def _():
        xbmcgui.Dialog().notification(heading="SWIG harness",
                                      message="bindings alive",
                                      icon=xbmcgui.NOTIFICATION_INFO,
                                      time=1500, sound=False)
