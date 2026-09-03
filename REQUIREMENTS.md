# Aurora A-Z interaction requirements

This document is the normative behavior contract for Aurora A-Z. A build is not
functional unless it satisfies every acceptance criterion below on Aurora
0.7b.2 Rev1655 hardware.

## Skin independence

- Aurora A-Z must work with the stock Default skin and third-party Aurora
  skins without producing a separate patched version of either skin.
- Installation, updating, and removal must not create, replace, rename, or
  modify any `.xzp` file or anything under Aurora's `Skins` directory.
- The selector must not depend on controls, element IDs, timelines, fonts, or
  other resources supplied by the currently selected skin.
- The selector row and highlight must be provided at runtime by the extension's
  own overlay or injected scene.
- Changing the active Aurora skin must not disable the selector or require the
  extension to be reinstalled.
- Version-specific native integration may target Aurora 0.7b.2 Rev1655, but
  skin-specific integration is not permitted.

## Single-file distribution

- The production release consists of exactly one file, `AuroraAZ.xex`. The
  installed payload is still exactly one file, but the same bytes are named
  `Plugins\NetDbgDll.xex` because that is the literal path requested by
  Rev1655's selected optional wrapper. The wrapper now loads and resolves a
  compatible canary on hardware; this becomes a supported production path only
  after M1 also proves plugin code execution.
- Executable code, compatibility signatures, default settings, shaders, and
  every font or glyph resource used by the selector must be embedded in that
  file.
- Installation must require only copying `AuroraAZ.xex` as
  `Plugins\NetDbgDll.xex` after verifying that target does not already exist,
  then restarting Aurora.
- Disabling or uninstalling must require only removing or renaming the installed
  `Plugins\NetDbgDll.xex` and restarting Aurora.
- The production plugin must not require a companion script, configuration
  file, asset directory, database row, QuickView, patched skin, patched
  `Aurora.xex`, or `launch.ini` change.
- Runtime filtering must remain in memory. It must not persist A-Z QuickViews
  or other Aurora A-Z-owned records in the user's database.
- Aurora A-Z must supply the complete, verified Network Debugger ordinal ABI;
  it must not rely on Aurora accepting unresolved export pointers.
- Aurora A-Z and a real `NetDbgDll.xex` cannot coexist. Installation must stop
  rather than overwrite an existing file at that path.

## User-visible behavior

- The alphabet is hidden during normal coverflow use.
- Holding R3 (right-stick click) while the coverflow is active displays one
  centered row containing `# A B ... Z` above the game title area.
- Selection starts at `#` each time R3 is pressed and held.
- D-pad Left and D-pad Right move the highlight one letter at a time.
- Left-stick Left and left-stick Right perform the same movement as the D-pad.
- While selector mode is active, those four inputs move only the letter
  highlight. They must not move or scroll the coverflow.
- Exactly one letter is visibly highlighted at a time.
- Releasing R3 applies the highlighted initial-character filter only if at
  least one Left/Right navigation input changed the selection during that hold.
  It then hides the alphabet and returns normal coverflow control.
- Pressing and releasing R3 without changing the highlight is a no-op: it
  hides the alphabet and must not apply `#` or rebuild the coverflow.
- Aurora A-Z must never consume, clear, or assign any action to A.
- RB continues to open Aurora's unmodified QuickView menu.

## Filtering semantics

- `A` through `Z` match title names case-insensitively by their first displayed
  character.
- `#` matches empty names and titles whose first character is not `A` through
  `Z`, including titles beginning with a digit, whitespace, or punctuation.
- Applying a letter updates the visible coverflow contents; it does not merely
  jump the cursor within an unfiltered list.
- An empty match is valid and displays an empty coverflow without an error.

## Input-state contract

There are two distinct states:

1. **Coverflow active:** the alphabet is hidden and Aurora retains its normal
   controls. Holding R3 enters selector mode; RB retains the normal QuickView
   action.
2. **R3 held / selector active:** the alphabet is visible. D-pad or left-stick
   Left/Right changes the highlighted character. Releasing R3 applies it only
   after such a change, then hides the alphabet. A tap with no navigation
   cancels. Coverflow navigation is suspended while R3 is held.

Cancel behavior and whether navigation stops or wraps at `#` and `Z` are not
specified yet. They must not be assumed by an implementation until documented.

## Acceptance tests

1. From the coverflow, verify that the alphabet is hidden, then press and hold
   R3. The row appears with `#` highlighted and no QuickView menu opens.
2. While holding R3, press D-pad Right once. `A` is highlighted and the
   coverflow does not move.
3. While holding R3, press D-pad Left once. `#` is highlighted and the
   coverflow does not move.
4. While holding R3, press left-stick Right once. `A` is highlighted and the
   coverflow does not move.
5. While holding R3, press left-stick Left once. `#` is highlighted and the
   coverflow does not move.
6. While holding R3, highlight a known letter and release R3. The alphabet
   hides, only matching titles remain visible, and normal coverflow navigation
   resumes.
7. Press RB from the coverflow. Aurora's normal QuickView menu opens unchanged.
8. Repeat tests 1 through 7 with Aurora's Default skin and at least one
   third-party skin. The controls and filtering behavior remain identical.
9. Compare the selected skins before and after installation and removal. No
   `.xzp` file or file under `Skins` has changed.
10. Install the one `AuroraAZ.xex` release binary under the documented
    `Plugins\NetDbgDll.xex` loader name, then remove it. Verify that no
    companion files, database records, or boot-configuration edits are created
    or required.
11. Press A during normal coverflow use and verify Aurora receives it unchanged;
    Aurora A-Z must neither apply a letter nor consume the key.
12. Press and release R3 without any Left/Right input. The row appears and
    hides, but the active filter, title count, and coverflow remain unchanged.
13. After a filter completes, verify R3 becomes available as soon as Aurora's
    queue has returned to a stable idle state; it must not impose a fixed
    multi-second delay when completion is already observable.

## Explicitly non-compliant implementations

- A static alphabet row with no focus or highlight.
- Opening or restyling Aurora's normal QuickView menu instead of selecting on
  the coverflow.
- Reusing Aurora's stock name-filter screen.
- Moving the coverflow cursor to a title without filtering the visible list.
- Supporting only D-pad navigation or only left-stick navigation.
- Requiring a specially patched skin, modifying `Default.xzp`, or distributing
  a replacement `.xzp`.
- Relying on skin-owned elements to receive input or draw the selector.
