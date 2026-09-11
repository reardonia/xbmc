# SWIG binding harness

Acceptance gate for the python binding layer. Every check is independent and
labelled; a failure records and the run continues.

## Install and run

    cp -r script.swigharness ~/.kodi/addons/
    kodi &
    # then, from another shell
    echo '{"jsonrpc":"2.0","id":1,"method":"Addons.ExecuteAddon","params":{"addonid":"script.swigharness"}}' \
      | nc localhost 9090

or from Kodi's own UI: Add-ons, Program add-ons, SWIG binding harness.

For an unattended run add the `quiet` argument, which suppresses the result
dialog:

    RunScript(script.swigharness, quiet)

## Results

- `kodi.log`, every line tagged `[SWIGHARNESS]`
- `special://temp/swigharness-result.txt`, ending in `RESULT: PASS` or
  `RESULT: FAIL`, followed by a per-shape coverage table

A build gate should grep that file:

    grep -q '^RESULT: PASS' ~/.kodi/temp/swigharness-result.txt

## Layout

    default.py                  runner entry point
    lib/runner.py               registry, execution, reporting, coverage table
    lib/checks_shapes.py        one section per type shape, inventory 1 to 26
    lib/checks_contracts.py     kwargs, None, lifetime, RTTI, cross-module,
                                overloads, operators, constants, errors
    lib/checks_directors.py     C++ calling back into python subclasses
    lib/checks_scraper.py       the literal call sequences of the bundled scrapers
    resources/skins/...         minimal window XML for the WindowXML checks

## What this cannot cover from inside an addon, and why

- `Dialog.browse`, `Dialog.select`, `Dialog.contextmenu` and
  `Dialog.multiselect` block waiting for a person. They carry shapes 18, 20 and
  24. Their argument conversion is covered indirectly by shapes 19 and 20
  through `ControlList`, but the return conversion of
  `Alternative<String, vector<String>>` and `unique_ptr<vector<int>>` is not
  exercised. Covering those needs either a UI automation layer or a temporary
  test-only wrapper that calls the same C++ function without the modal loop.
- `vector<bool>` and `vector<double>` (shapes 3 and 4) reach the API only
  through control setters that need a window that has actually been drawn.
- `onAction` and `onClick` need an input path. They are attempted and reported
  as skipped rather than failed when no front end delivers the event, so the
  harness stays usable headless.
- Object release is asserted only if `AddonClass::getNumAddonClasses` is
  exposed to python. It is not today, so that check reports as skipped. It is
  the one check worth adding a binding for, because leaks are otherwise
  invisible from python.
