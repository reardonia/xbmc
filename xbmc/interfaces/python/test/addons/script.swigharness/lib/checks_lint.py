"""Lint the suite's own source for the borrowed-return mistake.

Several getters return an object that holds a C++ raw pointer or weak_ptr to its
owner and no python reference to it. Chaining one off a constructor destroys the
owner at the end of the expression, leaving the child dangling. For the
settings case Kodi asserts on the expired weak_ptr and aborts outright.

This was written wrongly three times while building these suites, so it is
checked mechanically rather than remembered.
"""
import os
import re

# getters whose result does not keep its owner alive
BORROWED = (
    "getVideoInfoTag", "getMusicInfoTag", "getPictureInfoTag", "getGameInfoTag",
    "getSettings", "getPlayingItem", "getControl", "getSelectedItem", "getListItem",
)

# a call, then immediately one of those getters: foo(...).getSettings()
CHAINED = re.compile(r"\)\s*\.\s*(" + "|".join(BORROWED) + r")\s*\(")


def register(reg):

    @reg.check("-", "suite source never chains a borrowed getter off a constructor")
    def _():
        here = os.path.dirname(os.path.abspath(__file__))
        offenders = []
        for name in sorted(os.listdir(here)):
            if not name.endswith(".py"):
                continue
            path = os.path.join(here, name)
            with open(path, encoding="utf-8") as handle:
                for number, line in enumerate(handle, 1):
                    if line.lstrip().startswith("#"):
                        continue
                    match = CHAINED.search(line)
                    if match:
                        offenders.append("%s:%d %s" % (name, number, match.group(1)))
        assert not offenders, (
            "borrowed getter chained off a temporary owner; bind the owner to a "
            "name first: " + "; ".join(offenders))
