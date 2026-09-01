# Aurora A-Z

Aurora A-Z is a skin-agnostic, on-coverflow alphabetical selector for the
Aurora dashboard on Xbox 360. The target UI is a persistent `# A B ... Z` row
above the game title, with controller navigation and selection directly from
the main coverflow. It must work without modifying or replacing the selected
Aurora skin.

## Required interaction

The normative controller and filtering behavior is defined in
[`REQUIREMENTS.md`](REQUIREMENTS.md). In short: R3 enters the on-coverflow
selector at `#`; D-pad Left/Right and left-stick Left/Right move the highlight;
A filters the coverflow by the highlighted initial and returns control to the
coverflow. RB must retain Aurora's normal QuickView menu.

The architectural constraints that follow from these requirements are recorded
in [`ARCHITECTURE.md`](ARCHITECTURE.md). The gated engineering roadmap is in
[`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md).

## Current status

There is no compliant functional release yet. The current source proves two
pieces independently: a legacy skin prototype can draw the centered alphabet
row, and the Lua backend can expose initial-character filters as QuickViews. It
does not yet provide a skin-independent overlay, focus, in-row navigation,
highlighting, or direct A-to-filter behavior.

Functional test r3 is retained only as a hardware research build. It requires a
custom skin and its attempted D-pad Down remap does not work: the coverflow
consumes Down, while RB opens Aurora's separate QuickView menu. That build is
not the requested selector and is not part of the target architecture.

The earlier filter-only v0.1.0 prerelease is also deprecated because Aurora
already contains a stock name filter and it does not implement the project
interaction.

## Rebuild legacy functional test r3

Build the skin:

```powershell
.\scripts\build-functional-test.ps1
```

The generated research package is:

```text
build\Aurora-A-Z-functional-test-r3.zip
```

Extract it and merge the contents of its `Aurora-A-Z` folder into:

```text
Hdd1:\Aurora\
```

For backend or skin research only:

1. Open **Back/System → Scripts → Utility → Aurora A-Z Installer**.
2. Choose **Install / Update** and restart Aurora.
3. Open **B → View Settings → Skin** and select
   **Aurora A-Z Functional Test r3**.
4. Do not treat the Down-to-select behavior as implemented; it is a known
   failed experiment documented above.

Do not overwrite or rename `Default.xzp`.

To roll back, run the installer again and choose **Uninstall**, then select the
Default skin. The uninstall transaction removes only Aurora A-Z QuickViews and
restores the previous QuickView order and default.

## Target development workflow

The production implementation must be a version-gated runtime extension for
Aurora 0.7b.2 Rev1655. It will own controller-state handling, render or inject
its own selector overlay above the active skin, and bridge A-button selection
to Aurora's coverflow filtering. It must not write to `Skins`.

## Legacy skin research workflow

The local build uses:

- Aurora 0.7b.2's stock `Default.xzp`
- [XUIHelper](https://github.com/SGCSam/XUIHelper) for XUR/XUI conversion
- XboxUnity's `AuroraElements.xml` extension definitions
- `scripts/xzp.ps1` for open, reproducible XZP extraction and packaging

The required local-only inputs and tools are excluded from Git. The editable
selector is applied by `source/skin/patches/add-alphabet-row.ps1`, so the
project does not need to commit Aurora's stock skin assets.

Project layout:

```text
source/skin/patches/     Source-controlled changes to Aurora_Main
scripts/xzp.ps1          XZP build/extract utility
scripts/build-visual-test.ps1
scripts/build-functional-test.ps1
source/content/          Initial-character filter backend
source/utility/          Reversible QuickView installer
original/Default.xzp     Local stock skin; ignored by Git
original/extracted/      Local extracted stock skin; ignored by Git
tools/                   Local converters and extension definitions
build/                   Generated test skins
```

## Safety

Production builds must not modify skin packages or the on-disk `Aurora.xex`.
Any native integration must verify the exact Aurora revision before applying
in-memory hooks and must fail closed on unsupported builds. Database changes
must remain transactional and reversible. Keep FTP access available during
early hardware tests.
