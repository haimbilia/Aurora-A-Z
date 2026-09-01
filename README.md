# Aurora A-Z

Aurora A-Z is an on-coverflow alphabetical selector for the Aurora dashboard on
Xbox 360. The target UI is a persistent `# A B ... Z` row above the game title,
with controller navigation and selection directly from the main coverflow.

## Current status

Functional test r3 connects the coverflow row to Aurora's native QuickView
engine. It remaps D-pad Down on the main scene to the QuickView picker, installs
`#` through `Z` as ordered QuickViews, and uses Aurora's own list-refresh path
when A applies the selected letter.

The next hardware test must confirm that Aurora 0.7b.2 enters at `#`, accepts
Left/Right, applies with A, and presents its native selected-view indicator in a
way that can be converted into an in-row letter highlight.

The earlier filter-only v0.1.0 prerelease is deprecated because Aurora already
contains a stock name filter and it does not implement the project mockup.

## Test functional build r3

Build the skin:

```powershell
.\scripts\build-functional-test.ps1
```

Upload the generated file:

```text
build\Aurora-A-Z-functional-test-r3.zip
```

Extract it and merge the contents of its `Aurora-A-Z` folder into:

```text
Hdd1:\Aurora\
```

Then:

1. Open **Back/System → Scripts → Utility → Aurora A-Z Installer**.
2. Choose **Install / Update** and restart Aurora.
3. Open **B → View Settings → Skin** and select
   **Aurora A-Z Functional Test r3**.
4. From the coverflow, press Down, move with Left/Right, and press A.

Do not overwrite or rename `Default.xzp`.

To roll back, run the installer again and choose **Uninstall**, then select the
Default skin. The uninstall transaction removes only Aurora A-Z QuickViews and
restores the previous QuickView order and default.

## Development workflow

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

All development builds use a distinct skin name. The stock skin, Aurora
executable, and content database are not modified. The installer updates only
the QuickViews and two project-specific values in `settings.db`, inside a
transaction, and includes an uninstall path. Keep FTP access available during
early hardware tests.
