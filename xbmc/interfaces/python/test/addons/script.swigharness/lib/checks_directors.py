"""Directors: C++ calling back into a python subclass.

This is the area with the least documented support (SWIG's manual says nothing
about -builtin combined with directors), so it gets the most coverage here.

Every wait is bounded. A callback that never fires fails the check rather than
hanging the harness.
"""

import os
import time

import xbmc
import xbmcaddon
import xbmcgui

from .runner import Skip

ADDON_ID = "script.swigharness"
TIMEOUT = 10.0


def _wait_for(predicate, timeout=TIMEOUT, step=0.1):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        xbmc.sleep(int(step * 1000))
    return False


def register(reg):

    # ---- Monitor: the reliable, non-visual director path --------------

    @reg.check("-", "Monitor.onNotification fires in a python subclass")
    def _():
        class Probe(xbmc.Monitor):
            def __init__(self):
                super().__init__()
                self.seen = None

            def onNotification(self, sender, method, data):
                self.seen = (sender, method, data)

        probe = Probe()
        xbmc.executebuiltin("NotifyAll(swigharness,director-ping)")
        assert _wait_for(lambda: probe.seen is not None), \
            "onNotification never fired within %.0fs" % TIMEOUT
        assert "director-ping" in str(probe.seen), \
            "wrong notification delivered: %r" % (probe.seen,)

    @reg.check("-", "director callback arguments arrive as str")
    def _():
        class Probe(xbmc.Monitor):
            def __init__(self):
                super().__init__()
                self.types = None

            def onNotification(self, sender, method, data):
                self.types = (type(sender).__name__, type(method).__name__,
                              type(data).__name__)

        probe = Probe()
        xbmc.executebuiltin("NotifyAll(swigharness,types)")
        assert _wait_for(lambda: probe.types is not None), \
            "onNotification never fired"
        assert probe.types == ("str", "str", "str"), \
            "callback argument types: %r" % (probe.types,)

    @reg.check("-", "an exception in a director callback does not kill Kodi")
    def _():
        class Boom(xbmc.Monitor):
            def __init__(self):
                super().__init__()
                self.entered = False

            def onNotification(self, sender, method, data):
                self.entered = True
                raise ValueError("[SWIGHARNESS] deliberate callback error, expected in log")

        boom = Boom()
        xbmc.executebuiltin("NotifyAll(swigharness,boom)")
        assert _wait_for(lambda: boom.entered), "the raising callback never ran"
        # if the process survives to here and can still call in, the contract holds
        xbmc.log("[SWIGHARNESS] still alive after a raising director callback",
                 xbmc.LOGINFO)
        assert xbmc.getInfoLabel("System.BuildVersion"), \
            "the bindings stopped working after a callback exception"

    @reg.check("-", "a director subclass that never calls super().__init__()")
    def _():
        class NoSuper(xbmc.Monitor):
            def __init__(self):
                self.marker = "constructed without super"

        probe = NoSuper()
        assert probe.marker == "constructed without super", "python __init__ did not run"
        # the C++ object must still exist, so a wrapped method must work
        probe.abortRequested()

    @reg.check("-", "director subclass keeps its own constructor arguments")
    def _():
        class WithArgs(xbmc.Player):
            def __init__(self, config):
                self.config = config

        probe = WithArgs({"key": "value"})
        assert probe.config["key"] == "value", "subclass constructor argument lost"
        probe.isPlaying()

    @reg.check("-", "constructor arguments come from the outer call")
    def _():
        class Sub(xbmcgui.ListItem):
            def __init__(self, label):
                super().__init__(label)
                self.saw = label

        item = Sub("OuterLabel")
        assert item.saw == "OuterLabel", "python saw the wrong label"
        assert item.getLabel() == "OuterLabel", \
            "C++ did not receive the constructor argument: %r" % item.getLabel()

    # ---- Window and WindowDialog --------------------------------------

    @reg.check("-", "Window subclass receives onInit")
    def _():
        class Probe(xbmcgui.Window):
            def __init__(self):
                super().__init__()
                self.inited = False

            def onInit(self):
                self.inited = True

        probe = Probe()
        try:
            probe.show()
            assert _wait_for(lambda: probe.inited), \
                "onInit never fired within %.0fs" % TIMEOUT
        finally:
            probe.close()
            del probe

    @reg.check("-", "WindowDialog subclass receives onInit")
    def _():
        class Probe(xbmcgui.WindowDialog):
            def __init__(self):
                super().__init__()
                self.inited = False

            def onInit(self):
                self.inited = True

        probe = Probe()
        try:
            probe.show()
            assert _wait_for(lambda: probe.inited), \
                "onInit never fired within %.0fs" % TIMEOUT
        finally:
            probe.close()
            del probe

    @reg.check("-", "Window subclass receives onAction with a wrapped Action")
    def _():
        class Probe(xbmcgui.Window):
            def __init__(self):
                super().__init__()
                self.action_type = None

            def onAction(self, action):
                self.action_type = type(action).__name__
                self.close()

        probe = Probe()
        try:
            probe.show()
            xbmc.sleep(500)
            xbmc.executebuiltin("Action(Back)")
            if not _wait_for(lambda: probe.action_type is not None, timeout=5.0):
                raise Skip("no action was delivered; needs an interactive front end")
            assert probe.action_type == "Action", \
                "onAction received %r, not a wrapped Action" % probe.action_type
        finally:
            probe.close()
            del probe

    @reg.check("-", "Window subclass receives onClick for a control")
    def _():
        class Probe(xbmcgui.Window):
            def __init__(self):
                super().__init__()
                self.clicked = None

            def onClick(self, control_id):
                self.clicked = control_id

        probe = Probe()
        try:
            button = xbmcgui.ControlButton(10, 10, 120, 40, "click me")
            probe.addControl(button)
            probe.show()
            # Window exposes no id accessor to python, so the active window
            # cannot be identified directly. Confirming focus landed on our
            # button is the available evidence that the window is live and
            # receiving input.
            probe.setFocus(button)
            if not _wait_for(lambda: probe.getFocusId() == button.getId(), timeout=5.0):
                raise Skip("focus never reached the control; window is not taking input")
            for _ in range(3):
                xbmc.executebuiltin("Action(Select)", True)
                if _wait_for(lambda: probe.clicked is not None, timeout=2.0):
                    break
            if probe.clicked is None:
                raise Skip("no click was delivered; needs an interactive front end")
            assert probe.clicked == button.getId(), \
                "onClick reported %r, expected %r" % (probe.clicked, button.getId())
        finally:
            probe.close()
            del probe

    # ---- WindowXML and WindowXMLDialog --------------------------------

    def _skin_dir():
        path = xbmcaddon.Addon(ADDON_ID).getAddonInfo("path")
        return os.path.join(path, "resources", "skins", "default", "1080i")

    @reg.check("-", "WindowXML subclass loads and receives onInit")
    def _():
        if not os.path.exists(os.path.join(_skin_dir(), "harness.xml")):
            raise Skip("harness.xml skin file is not installed")

        class Probe(xbmcgui.WindowXML):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, **kwargs)
                self.inited = False

            def onInit(self):
                self.inited = True

        addon_path = xbmcaddon.Addon(ADDON_ID).getAddonInfo("path")
        probe = Probe("harness.xml", addon_path, "default", "1080i")
        try:
            probe.show()
            assert _wait_for(lambda: probe.inited), \
                "WindowXML onInit never fired within %.0fs" % TIMEOUT
        finally:
            probe.close()
            del probe

    @reg.check("-", "WindowXMLDialog subclass loads and receives onInit")
    def _():
        if not os.path.exists(os.path.join(_skin_dir(), "harness.xml")):
            raise Skip("harness.xml skin file is not installed")

        class Probe(xbmcgui.WindowXMLDialog):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, **kwargs)
                self.inited = False

            def onInit(self):
                self.inited = True

        addon_path = xbmcaddon.Addon(ADDON_ID).getAddonInfo("path")
        probe = Probe("harness.xml", addon_path, "default", "1080i")
        try:
            probe.show()
            assert _wait_for(lambda: probe.inited), \
                "WindowXMLDialog onInit never fired within %.0fs" % TIMEOUT
        finally:
            probe.close()
            del probe

    @reg.check("-", "WindowXMLDialog subclass with its own keyword arguments")
    def _():
        # %feature("nokwds") made the shipped bindings parse these constructors
        # with PyArg_ParseTuple, which never inspects the keywords dict, so extra
        # keywords for the subclass's own __init__ were ignored. script.kodi.
        # loguploader calls exactly this shape and broke when they became errors.
        if not os.path.exists(os.path.join(_skin_dir(), "harness.xml")):
            raise Skip("harness.xml skin file is not installed")

        class Probe(xbmcgui.WindowXMLDialog):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, **kwargs)
                self.name = kwargs.get("name")
                self.content = kwargs.get("content")

        addon_path = xbmcaddon.Addon(ADDON_ID).getAddonInfo("path")
        probe = Probe("harness.xml", addon_path, "default",
                      name="a name", content="a body")
        try:
            assert probe.name == "a name", "subclass keyword lost: %r" % probe.name
            assert probe.content == "a body", "subclass keyword lost: %r" % probe.content
        finally:
            del probe

    @reg.check("-", "Window subclass with its own keyword arguments")
    def _():
        class Probe(xbmcgui.Window):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, **kwargs)
                self.tag = kwargs.get("tag")

        probe = Probe(tag="marker")
        try:
            assert probe.tag == "marker", "subclass keyword lost: %r" % probe.tag
        finally:
            del probe

    @reg.check("-", "a director object survives being dropped and recreated")
    def _():
        for _i in range(3):
            class Probe(xbmc.Monitor):
                def __init__(self):
                    super().__init__()
                    self.seen = False

                def onNotification(self, sender, method, data):
                    self.seen = True

            probe = Probe()
            xbmc.executebuiltin("NotifyAll(swigharness,cycle)")
            assert _wait_for(lambda: probe.seen, timeout=5.0), \
                "callback lost on iteration %d" % _i
            del probe
