# Skin-agnostic architecture decision

## Decision

Aurora A-Z will be implemented as a runtime extension for Aurora 0.7b.2
Rev1655, not as an Aurora skin. Users keep their existing stock or third-party
skin unchanged. The extension is distributed and installed as one
self-contained binary. Its release name is `AuroraAZ.xex`; its required
Rev1655 candidate installed name is `Plugins\NetDbgDll.xex`, the literal path
owned by the optional key-7 wrapper. That wrapper contract is established by
static analysis; the one-file bootstrap is not accepted until M1 passes on
hardware.

Implementation must proceed through the safety and feasibility gates in
[`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md). In particular, rendering
and filter work do not begin until a reversible native loader is proven.

## Required components embedded in `AuroraAZ.xex`

1. **Compatibility gate** verifies the running Aurora revision before enabling
   native integration.
2. **Input bridge** observes R3, D-pad Left/Right, left-stick Left/Right, and A,
   and consumes navigation input only while selector mode is active.
3. **Selector state** tracks the inactive/active state and the current `#`–`Z`
   index independently of the coverflow.
4. **Runtime overlay** draws or injects the alphabet row and highlight above
   the active skin using resources embedded in Aurora A-Z.
5. **Filter bridge** applies the selected initial-character predicate to the
   visible coverflow and then returns input ownership to Aurora.
6. **Single-file lifecycle** initializes through Aurora's optional Network
   Debugger wrapper after its hardware gate passes, and becomes fully disabled
   when the installed `Plugins\NetDbgDll.xex` is removed or renamed and Aurora
   is restarted.

## Hard constraints

- Do not patch, replace, or redistribute `Default.xzp` or any third-party skin.
- Do not write files under Aurora's `Skins` directory.
- Do not rely on element IDs or resources defined by the selected skin.
- Do not replace or restyle Aurora's RB QuickView menu.
- Do not permanently modify the on-disk `Aurora.xex`.
- Do not require any installed artifact other than the single Aurora A-Z XEX;
  this
  includes scripts, configuration files, loose assets, database records,
  QuickViews, or boot-loader configuration changes.
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
- Can the export-capable XEX canary satisfy the key-7 Network Debugger wrapper
  on hardware and roll back cleanly before any hook is enabled? The first
  attempt was rejected by `XexLoadImage`; a corrected title-DLL image is
  awaiting the M1 retry.
