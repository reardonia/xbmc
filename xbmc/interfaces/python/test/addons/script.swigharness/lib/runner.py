"""Check registry, execution and reporting.

Each check is independent: a failure records and moves on. A check may declare
itself inapplicable by raising Skip, which is reported separately and never
counts as a failure.
"""

import time
import traceback

import xbmc
import xbmcvfs

TAG = "[SWIGHARNESS]"

PASS = "PASS"
FAIL = "FAIL"
SKIP = "SKIP"


class Skip(Exception):
    """Raised by a check that cannot run in this environment."""


class Registry(object):
    def __init__(self):
        self._checks = []
        self.results = []

    def add(self, shape, label, fn):
        """shape: inventory id(s) this check covers, or '-' for a contract check."""
        self._checks.append((shape, label, fn))

    def check(self, shape, label):
        def deco(fn):
            self.add(shape, label, fn)
            return fn
        return deco

    def count(self):
        return len(self._checks)

    def run(self):
        for shape, label, fn in self._checks:
            started = time.time()
            try:
                fn()
                status, detail = PASS, ""
            except Skip as exc:
                status, detail = SKIP, str(exc)
            except Exception as exc:  # noqa: BLE001 - a check may fail any way
                status = FAIL
                detail = "%s: %s" % (type(exc).__name__, exc)
                xbmc.log("%s traceback for %s\n%s" % (TAG, label, traceback.format_exc()),
                         xbmc.LOGERROR)
            self.results.append({
                "shape": shape,
                "label": label,
                "status": status,
                "detail": detail,
                "ms": int((time.time() - started) * 1000),
            })

    def tally(self):
        out = {PASS: 0, FAIL: 0, SKIP: 0}
        for r in self.results:
            out[r["status"]] += 1
        return out

    def report(self, path="special://temp/swigharness-result.txt"):
        counts = self.tally()
        ok = counts[FAIL] == 0
        lines = []
        lines.append("SWIG binding harness")
        lines.append("=" * 78)
        lines.append("%-6s %-8s %-44s %s" % ("SHAPE", "STATUS", "CHECK", "DETAIL"))
        lines.append("-" * 78)
        for r in self.results:
            lines.append("%-6s %-8s %-44s %s" % (r["shape"], r["status"], r["label"],
                                                 r["detail"]))
        lines.append("-" * 78)
        lines.append("passed %d   failed %d   skipped %d   total %d"
                     % (counts[PASS], counts[FAIL], counts[SKIP], len(self.results)))
        lines.append("RESULT: %s" % ("PASS" if ok else "FAIL"))
        lines.append("")
        lines.append(self.coverage_table())
        text = "\n".join(lines)

        for r in self.results:
            level = xbmc.LOGINFO if r["status"] != FAIL else xbmc.LOGERROR
            xbmc.log("%s %-6s %-8s %-44s %s" % (TAG, r["shape"], r["status"],
                                                r["label"], r["detail"]), level)
        xbmc.log("%s RESULT: %s (%d passed, %d failed, %d skipped)"
                 % (TAG, "PASS" if ok else "FAIL", counts[PASS], counts[FAIL],
                    counts[SKIP]),
                 xbmc.LOGINFO if ok else xbmc.LOGERROR)

        try:
            handle = xbmcvfs.File(path, "w")
            handle.write(bytearray(text.encode("utf-8")))
            handle.close()
        except Exception as exc:  # noqa: BLE001 - reporting must not mask results
            xbmc.log("%s could not write %s: %s" % (TAG, path, exc), xbmc.LOGERROR)

        return ok, counts, text

    def coverage_table(self):
        """Per-inventory-shape coverage, so a gap is visible rather than implied."""
        covered = {}
        for r in self.results:
            for shape in r["shape"].split("+"):
                shape = shape.strip()
                if not shape or shape == "-":
                    continue
                prev = covered.get(shape)
                # FAIL beats SKIP beats PASS when summarising a shape
                order = {PASS: 0, SKIP: 1, FAIL: 2}
                if prev is None or order[r["status"]] > order[prev]:
                    covered[shape] = r["status"]
        lines = ["Coverage against the type-shape inventory", "-" * 78]
        for num in range(1, 27):
            key = str(num)
            lines.append("  shape %-3s %s" % (key, covered.get(key, "NOT COVERED")))
        return "\n".join(lines)
