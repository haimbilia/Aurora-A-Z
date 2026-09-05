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

- The production executable payload consists of exactly one file,
  `AuroraAZ.xex`, installed as `Plugins\NetDbgDll.xex`.
- Executable code, compatibility signatures, default settings, shaders, and
  every font or glyph resource used by the selector must be embedded in that
  file.
- Installation copies the plugin to Aurora's Network Debugger slot, backing up
  any existing file first. It requires no launch.ini changes.
- Uninstalling removes this plugin's `Plugins\NetDbgDll.xex`, restores any
  backed-up debugger file, then reboots the console.
- The production plugin must not require a companion script, preinstalled
  configuration file, asset directory, database row, QuickView, patched skin,
  or patched `Aurora.xex`. The executable payload remains one XEX. After first use, the plugin
  may generate one small settings file
  under Aurora's `Data` directory to persist the selected operating mode.
- Runtime filtering must remain in memory. It must not persist A-Z QuickViews
  or other Aurora A-Z-owned records in the user's database.
- v1.0 uses the Network Debugger slot. Never enable the experimental DashLaunch
  route alongside it. DashLaunch installers are not part of v1.0.
- The Aurora Script-menu installer is the primary installation method. Its
  package includes Main.lua, the single XEX payload and icon.png. It stages the
  payload before renaming an existing plugin to an unused backup filename,
  attempts rollback if activation fails, and asks for a manual console reboot.

## Mode notification

- R3+L3 switches and persists Browse/Filter mode.
- Display "Browse Mode" or "Filter Mode" at the bottom right for five seconds
  after switching. The notice survives R3 release; a new switch restarts the timer.
- Hide it over modal/system UI and during title handoff with the other overlays.

## User-visible behavior

- The alphabet is hidden during normal coverflow use.
- Holding R3 (right-stick click) while the coverflow is active darkens the
  complete viewport and displays one horizontally and vertically centered row
  containing `ALL # A B ... Z` in Filter mode or `# A B ... Z` in Browse mode.
- The first selector session starts at `ALL` in Filter mode and `#` in Browse
  mode. Later sessions reopen on the last selected character.
- D-pad Left and D-pad Right move the highlight one letter at a time.
- Left-stick Left and left-stick Right perform the same movement as the D-pad.
- Holding either horizontal input repeats quickly without requiring separate
  presses.
- While selector mode is active, those four inputs move only the letter
  highlight. They must not move or scroll the coverflow.
- Exactly one item is visibly highlighted at a time. The selected item is
  rendered at 100% opacity and larger than the inactive row items.
- Highlight changes ease between the former and new item instead of snapping.
- Releasing R3 applies the highlighted initial-character filter only if at
  least one Left/Right navigation input changed the selection during that hold.
  It then hides the alphabet and returns normal coverflow control.
- After a changed selection is released, the row disappears immediately. The
  selected item briefly grows while fading to transparent, then the viewport
  dimming disappears. The animation must not delay or gate filter scheduling.
- Pressing and releasing R3 without changing the highlight is a no-op: it
  hides the alphabet and must not apply `ALL` or rebuild the coverflow.
- The on-coverflow selector must never consume, clear, or assign any action to
  A. The embedded Configure Modules page may consume A only while its Browse or
  Filter control owns focus.
- RB continues to open Aurora's unmodified QuickView menu.
- Launching a game or XEX must behave identically with Aurora A-Z installed or
  absent; a black screen, delayed handoff, or required shutdown is a release
  blocker.
- While the row is visible, clicking L3 toggles the persistent Browse/Filter
  mode. This is an R3+L3 chord because the row is visible only while R3 is
  held; it must not affect normal coverflow input.

## Operating mode setting

- The supported mode control is the R3+L3 chord while the selector is visible;
  it must not depend on Aurora's Configure Modules UI.
- `Browse` is the default for a missing, invalid, or unsupported settings file.
- Saving a choice persists it across Aurora restarts. A torn or invalid write
  must fail safely back to `Browse` without preventing Aurora from starting.
- Opening, navigating, saving, or cancelling the settings overlay must not
  change the current QuickView, alphabet selection, or coverflow selection.
- The settings UI must not repurpose R3 or RB. Those controls retain their
  coverflow behavior outside the settings screen.

## Browse-mode semantics

- In `Browse` mode, releasing a changed selection moves the coverflow to the
  first title in the current active list whose displayed name belongs to the
  chosen initial-character group. It does not rebuild or replace the list.
- Browse mode searches only the current QuickView result, naturally preserving
  its non-alphabetical constraints.
- `#`, `A` through `Z`, case handling, and empty-match classification are the
  same as Filter mode.
- If the chosen group has no match, the current selection remains unchanged.
- Browse mode must not invoke Aurora's sort/filter/swap worker and should feel
  immediate on a library of at least 2,000 titles.

## Filtering semantics

These rules apply when the saved operating mode is `Filter`:

- `ALL` removes only an active `NameFilter` predicate. It must preserve the
  current QuickView and every non-name predicate, so selecting `ALL` while the
  XBLA QuickView is active displays all XBLA titles rather than all titles.
- Only Aurora's own QuickView `ALL` selection may switch to the global all-game
  view; Aurora A-Z must never synthesize that QuickView change.

- `A` through `Z` match title names case-insensitively by their first displayed
  character.
- `#` matches empty names and titles whose first character is not `A` through
  `Z`, including titles beginning with a digit, whitespace, or punctuation.
- Applying a letter updates the visible coverflow contents.
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

Navigation wraps in both modes: Left from `ALL` reaches `Z` and Right from `Z`
reaches `ALL` in Filter mode; Left from `#` reaches `Z` and Right from `Z`
reaches `#` in Browse mode.

## Acceptance tests

1. From the coverflow, verify that the alphabet is hidden, then press and hold
   R3. The centered row appears over a dimmed viewport with `ALL` highlighted
   in Filter mode or `#` in Browse mode at full opacity and a larger size; no
   QuickView menu opens.
2. In Filter mode while holding R3, press D-pad Right once. `#` is highlighted and the
   coverflow does not move.
3. While holding R3, press D-pad Left once. `ALL` is highlighted and the
   coverflow does not move.
4. While holding R3, press left-stick Right once. `#` is highlighted and the
   coverflow does not move.
5. While holding R3, press left-stick Left once. `ALL` is highlighted and the
   coverflow does not move.
6. While holding R3, highlight a known letter and release R3. The alphabet
   hides and normal coverflow navigation resumes. In Browse mode the selection
   jumps to the first match without changing the title count; in Filter mode
   only matching titles remain visible.
7. Press RB from the coverflow. Aurora's normal QuickView menu opens unchanged.
8. Repeat tests 1 through 7 with Aurora's Default skin and at least one
   third-party skin. The controls and filtering behavior remain identical.
9. Compare the selected skins before and after installation and removal. No
   `.xzp` file or file under `Skins` has changed.
10. Install the release XEX as `Plugins\NetDbgDll.xex`, backing up an existing
    debugger file first. Remove it and restore that backup to uninstall. Verify
    launch.ini, skins, database records, and `Aurora.xex` remain unchanged.
11. Press A during normal coverflow use and verify Aurora receives it unchanged;
    Aurora A-Z must neither apply a letter nor consume the key.
12. Press and release R3 without any Left/Right input. The row appears and
    hides, but the active filter, title count, and coverflow remain unchanged.
13. After a filter completes, verify R3 becomes available as soon as Aurora's
    queue has returned to a stable idle state; it must not impose a fixed
    multi-second delay when completion is already observable.
14. Launch a known-good game and a known-good XEX application, return to
    Aurora, and repeat. Every title handoff must complete normally.
15. In Filter mode, activate the XBLA QuickView, apply a letter, then select `ALL` by moving
    away from it and back before releasing R3. All XBLA titles return, while
    non-XBLA titles remain excluded and Aurora's QuickView stays on XBLA.
16. Release a changed selection and verify the row vanishes immediately while
    only the selected item grows and fades away; the selected Browse/Filter
    action begins without waiting for the animation.
17. While holding R3 to show the selector, click L3. Verify that `ALL` appears
    in Filter mode and disappears in Browse mode; restart Aurora after each
    choice and verify that the choice is restored.
18. In Browse mode, select a letter and verify that the full title count is
    unchanged, the coverflow moves to the first matching title, and no
    `Sorting Game List` / `Filter Game List` cycle is logged.
19. In Browse mode, verify the row contains `# A ... Z` with no `ALL`, then
    wrap Left from `#` to `Z` and Right from `Z` to `#`.
20. In Browse mode, choose a group with no match and verify that the current
    cover remains selected.
21. Select a letter, release R3, then hold R3 again and verify the same letter
    is highlighted. Hold Left or Right and verify repeat navigation advances
    quickly with a smooth highlight transition.

## Explicitly non-compliant implementations

- A static alphabet row with no focus or highlight.
- Opening or restyling Aurora's normal QuickView menu instead of selecting on
  the coverflow.
- Reusing Aurora's stock name-filter screen.
- Providing cursor-jump behavior while the saved mode is `Filter`, or
  rebuilding the list while the saved mode is `Browse`.
- Supporting only D-pad navigation or only left-stick navigation.
- Requiring a specially patched skin, modifying `Default.xzp`, or distributing
  a replacement `.xzp`.
- Relying on skin-owned elements to receive input or draw the selector.
