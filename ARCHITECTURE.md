# Skin-agnostic architecture decision

## Decision

Aurora A-Z will be implemented as a runtime extension for Aurora 0.7b.2
Rev1655, not as an Aurora skin. Users keep their existing stock or third-party
skin unchanged.

## Required components

1. **Compatibility gate** verifies the running Aurora revision before enabling
   native integration.
2. **Input bridge** observes R3, D-pad Left/Right, left-stick Left/Right, and A,
   and consumes navigation input only while selector mode is active.
3. **Selector state** tracks the inactive/active state and the current `#`–`Z`
   index independently of the coverflow.
4. **Runtime overlay** draws or injects the alphabet row and highlight above
   the active skin using resources owned by Aurora A-Z.
5. **Filter bridge** applies the selected initial-character predicate to the
   visible coverflow and then returns input ownership to Aurora.
6. **Reversible installer** deploys only Aurora A-Z-owned files and database
   records and can remove them without touching skins.

## Hard constraints

- Do not patch, replace, or redistribute `Default.xzp` or any third-party skin.
- Do not write files under Aurora's `Skins` directory.
- Do not rely on element IDs or resources defined by the selected skin.
- Do not replace or restyle Aurora's RB QuickView menu.
- Do not permanently modify the on-disk `Aurora.xex`.
- Reject unsupported Aurora revisions instead of applying uncertain hooks.

## Legacy work

The existing skin patches and r3 package are visual and backend research only.
They are explicitly outside the production architecture and must not be used as
the basis of a release claim.

## Open engineering questions

- Which Rev1655 function receives coverflow controller input?
- Which internal function changes the active filter and refreshes the list?
- Is top-level XUI scene injection stable across skin changes, or is a small
  renderer-owned overlay safer?
- Which module-loading mechanism provides the safest reversible deployment?
