# Changelog

## Unreleased

- Redirect development to the requested on-coverflow selector.
- Add lossless stock XUR/XUI conversion using XUIHelper and Aurora extensions.
- Add an open PowerShell XZP builder/extractor with verified 418-file round trip.
- Add a visual-test skin with `# A ... Z` in `Aurora_Main`.
- Center the selector and enable a high-contrast drop shadow in visual test r2.
- Add functional test r3 backed by Aurora's native QuickView engine.
- Map D-pad Down to the existing QuickView picker in `ScnApplication`.
- Add `#` through `Z` initial-character predicates and ordered QuickViews.
- Add a transactional installer/uninstaller that preserves the existing
  QuickView order and default.
- Deprecate the filter-only v0.1.0 experiment because it duplicates Aurora's
  stock name filter.
- Define the selector's normative controller and filtering contract and mark
  functional test r3 as a non-compliant research build.
- Require the production selector to be skin agnostic: no `.xzp` or `Skins`
  changes, no dependency on skin-owned controls, and identical behavior across
  Default and third-party skins.
- Add a host-tested C99 selector core, built-in filter mapping, mockup layout,
  and exact Rev1655 compatibility probes.
- Document the Rev1655 native input, render, filter, and module-loader paths.
- Prove that Aurora does not discover arbitrary `Plugins/*.xex` files; retain
  the strict loader gate around the key-7 Network Debugger candidate.
- Add repeatable NOVA screenshot capture and hardware-baseline documentation.
- Record the first isolated hardware canary failure: `XexLoadImage` rejected
  the image before entry, production Aurora remained untouched, and the lab
  payload was recoverably disabled.
- Correct the retry XEX shape from module flags `0xA` to `0x9`, add Image Base
  Address optional header `0x10201`, and omit SynthXEX's empty TLS stub.
- Prove that the corrected XEX loads through Aurora's key-7 wrapper and reaches
  its module-loaded notification with ordinals 2-5 resolved. M1 remains pending
  because neither the canary log nor its resident thread was observed.
- Complete the M1 bootstrap and M2a observe-only controller gates on isolated
  hardware without touching the production Aurora installation.
- Add the first renderer-owned M2b row canary and correct Aurora's texture-lock
  output contract.
- Correct the recovered vertex-constant ABI: target the vertex bank rather than
  the pixel bank and publish the mandatory 64-bit dirty mask for c1/c2 so the
  font shader actually receives the overlay constants.
- Match the ATG font pipeline's disabled viewport transform by submitting raw
  screen-pixel vertex positions instead of clip-space-normalized coordinates.
- Match Aurora's native four-vertex quad order (`TL, TR, BR, BL`) to prevent
  the alphabet row from being diagonally clipped to one triangle.
- Enable the bounded selector interaction canary: publish the live scene gate,
  enter with R3, move with D-pad/left-stick Left/Right, and visibly highlight
  the selected glyph. Filter application remains independently fail-closed;
  A is consumed and keeps the selector open until that worker is verified.
- Record the successful M3a hardware interaction test and add the M3b
  fail-closed filter foundation: aligned host object layouts, exact provenance,
  pristine pre-hook binding, and a read-only registry/snapshot probe marker.
- Add the lab-only one-shot apply gate after M3b passed on hardware. A single
  A press may enqueue Aurora's native additional-filter job; the filter gate
  then revokes immediately to prevent repeated expensive sort jobs.
- Record the successful one-shot hardware apply: Aurora loaded the selected
  letter filter, the native scheduler reported one job and zero rejections,
  and the safety gate correctly blocked a second request.
- Replace the one-shot state with a lab-only repeatable apply cycle. Each
  successful enqueue disables A for at least eight seconds and requires one
  continuous second of an empty Aurora queue and clear worker-busy state before
  re-arming the next letter selection.

## 0.1.0 - 2026-09-01 (deprecated)

- Initial filter-only experiment. This did not implement the project mockup.
