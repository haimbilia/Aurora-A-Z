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

## User-visible behavior

- The coverflow displays one centered row containing `# A B ... Z` above the
  game title area.
- Pressing R3 (right-stick click) while the coverflow is active enters letter
  selection without opening Aurora's QuickView menu.
- Selection starts at `#` each time selector mode is entered.
- D-pad Left and D-pad Right move the highlight one letter at a time.
- Left-stick Left and left-stick Right perform the same movement as the D-pad.
- While selector mode is active, those four inputs move only the letter
  highlight. They must not move or scroll the coverflow.
- Exactly one letter is visibly highlighted at a time.
- Pressing A applies the highlighted initial-character filter to the coverflow.
- After A is pressed, selector mode closes and normal coverflow control resumes.
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

1. **Coverflow active:** Aurora retains its normal controls. R3 enters selector
   mode; RB retains the normal QuickView action.
2. **Selector active:** D-pad or left-stick Left/Right changes the highlighted
   character, and A applies it. Coverflow navigation is suspended until the
   selection is applied.

Cancel behavior and whether navigation stops or wraps at `#` and `Z` are not
specified yet. They must not be assumed by an implementation until documented.

## Acceptance tests

1. From the coverflow, press R3. The highlight appears on `#`, and no QuickView
   menu opens.
2. Press D-pad Right once. `A` is highlighted and the coverflow does not move.
3. Press D-pad Left once. `#` is highlighted and the coverflow does not move.
4. Press left-stick Right once. `A` is highlighted and the coverflow does not
   move.
5. Press left-stick Left once. `#` is highlighted and the coverflow does not
   move.
6. Highlight a known letter and press A. Only matching titles remain visible,
   the selector relinquishes input, and normal coverflow navigation resumes.
7. Press RB from the coverflow. Aurora's normal QuickView menu opens unchanged.
8. Repeat tests 1 through 7 with Aurora's Default skin and at least one
   third-party skin. The controls and filtering behavior remain identical.
9. Compare the selected skins before and after installation and removal. No
   `.xzp` file or file under `Skins` has changed.

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
