# Aurora A-Z

Aurora A-Z is an on-coverflow alphabetical selector for the Aurora dashboard on
Xbox 360. The target UI is a persistent `# A B ... Z` row above the game title,
with controller navigation and selection directly from the main coverflow.

## Current status

Development has moved into the stock Aurora 0.7b.2 `Aurora_Main` scene.

- Stock `Default.xzp` extraction works.
- `Aurora_Main.xur` converts losslessly to editable XUI and back.
- The project has an open PowerShell XZP build/extract tool.
- A separate visual-test skin displays the alphabet row on the coverflow.
- Controller navigation, active-letter highlighting, and applying the selected
  letter are the next milestone.

The earlier filter-only v0.1.0 prerelease is deprecated because Aurora already
contains a stock name filter and it does not implement the project mockup.

## Test the current visual skin

Build the skin:

```powershell
.\scripts\build-visual-test.ps1
```

Upload the generated file:

```text
build\Aurora-A-Z-visual-test.xzp
```

to the Xbox at:

```text
Hdd1:\Aurora\Skins\Aurora-A-Z-visual-test.xzp
```

In Aurora, open **B → View Settings → Skin**, select
**Aurora A-Z Visual Test**, and restart Aurora if prompted. Do not overwrite or
rename `Default.xzp`.

This build only verifies the row's position, size, font, and readability. The
letters are not selectable yet.

To roll back, select the Default skin in View Settings. After switching away,
the visual-test XZP can be deleted safely.

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
original/Default.xzp     Local stock skin; ignored by Git
original/extracted/      Local extracted stock skin; ignored by Git
tools/                   Local converters and extension definitions
build/                   Generated test skins
```

## Safety

All development builds use a distinct skin name. The stock skin, Aurora
executable, and content database are not modified. Keep FTP access available
during early hardware tests so a test skin can be removed if Aurora fails to
load it.
