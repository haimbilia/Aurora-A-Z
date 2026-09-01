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

## 0.1.0 - 2026-09-01 (deprecated)

- Initial filter-only experiment. This did not implement the project mockup.
