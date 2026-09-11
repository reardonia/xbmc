"""SWIG binding harness.

Run:  RunScript(script.swigharness)
      RunScript(script.swigharness, quiet)   no dialog, log and file only

Results: kodi.log lines tagged [SWIGHARNESS], and
         special://temp/swigharness-result.txt, whose last meaningful line is
         "RESULT: PASS" or "RESULT: FAIL" for a build gate to grep.
"""

import sys

import xbmc
import xbmcgui

from lib import (checks_contracts, checks_directors, checks_lint, checks_scraper,
                 checks_shapes)
from lib.runner import FAIL, Registry, SKIP, TAG


def main():
    quiet = "quiet" in [a.lower() for a in sys.argv[1:]]

    reg = Registry()
    checks_shapes.register(reg)
    checks_contracts.register(reg)
    checks_directors.register(reg)
    checks_scraper.register(reg)
    checks_lint.register(reg)

    xbmc.log("%s starting, %d checks" % (TAG, reg.count()), xbmc.LOGINFO)
    reg.run()
    ok, counts, _text = reg.report()

    if quiet:
        return

    summary = "%d passed, %d failed, %d skipped" % (counts["PASS"], counts[FAIL],
                                                    counts[SKIP])
    if ok:
        xbmcgui.Dialog().ok("SWIG harness: PASS", summary)
    else:
        failures = [r for r in reg.results if r["status"] == FAIL]
        detail = "\n".join("%s: %s" % (r["label"], r["detail"]) for r in failures[:6])
        if len(failures) > 6:
            detail += "\n... and %d more, see the log" % (len(failures) - 6)
        xbmcgui.Dialog().ok("SWIG harness: FAIL", summary + "\n\n" + detail)


if __name__ == "__main__":
    main()
