# Aurora A-Z

Aurora A-Z adds alphabetical title filtering to the Aurora dashboard on Xbox
360. Version 0.1 provides a functional `#`/A–Z selector inside Aurora's existing
**View Settings → Filters & Sort** screen.

> [!IMPORTANT]
> Version 0.1 does not yet draw the persistent alphabet row from the mockup over
> the coverflow. That requires a compiled Aurora skin and Xbox XUI tooling. This
> release establishes and tests the filtering behavior first.

## Compatibility

- Target: Aurora 0.7b.2 r1655
- Console: RGH/JTAG Xbox 360 running Aurora
- Installation: FTP, USB, or Aurora File Manager
- No database, executable, or stock skin files are replaced

Other Aurora versions with the Lua content-filter API may work, but 0.7b.2 is
the release package used for development.

## Install

1. Download `Aurora-A-Z-v0.1.0.zip` from the GitHub Releases page.
2. Extract the ZIP on your computer or USB drive.
3. Open the extracted `Aurora-A-Z-v0.1.0` folder.
4. Copy its `User` folder into the root of your Aurora installation and allow
   the folders to merge.

The resulting files on the Xbox must be:

```text
Aurora\User\Scripts\Content\Filters\AuroraAZ.ini
Aurora\User\Scripts\Content\Filters\AuroraAZ.lua
```

5. Restart Aurora. A full Aurora restart is required after adding or updating
   filter scripts.

### Installing with Aurora File Manager

1. Extract the release ZIP to a USB drive on the PC.
2. Connect the USB drive to the Xbox 360.
3. In Aurora, press **Back** and open **File Manager**.
4. In one pane, open the extracted release folder on the USB drive.
5. In the other pane, open the folder containing `Aurora.xex`.
6. Copy the release's `User` folder to the Aurora folder and merge it.
7. Restart Aurora.

### Installing with FTP

Connect to Aurora's FTP server and upload both release files to:

```text
/User/Scripts/Content/Filters/
```

The path above is relative to the folder containing `Aurora.xex`. Restart
Aurora after the transfer finishes.

## Use

1. From Aurora's coverflow, press **B** to open **View Settings**.
2. Open **Filters & Sort**.
3. Select **A-Z**.
4. Select a range and then a letter. Use **#** for titles whose names begin
   with a number, symbol, empty value, or non-ASCII character.
5. Return to the coverflow. Aurora displays only matching titles.

Use **Clear All** in **Filters & Sort** to restore the complete game list.

## Uninstall or roll back

Delete only these two files from the Xbox:

```text
Aurora\User\Scripts\Content\Filters\AuroraAZ.ini
Aurora\User\Scripts\Content\Filters\AuroraAZ.lua
```

Restart Aurora and use **Clear All** if the old filter selection remains active.
The release does not modify `content.db`, `Aurora.xex`, or `Default.xzp`.

## Known limitations

- Selection happens in View Settings, not in an on-coverflow alphabet row.
- Choosing a letter filters the list; it does not keep the full list and jump
  the coverflow to the first matching title.
- Matching uses the first byte of `Content.Name`. ASCII A–Z is supported;
  accented and non-Latin initials currently appear under `#`.
- Hardware testing is still required. Keep FTP or USB recovery access available
  when testing early releases.

## Development

VS Code is the primary editor. Build a distributable ZIP from PowerShell:

```powershell
.\scripts\build-release.ps1
```

The generated archive is written to `build/Aurora-A-Z-v<VERSION>.zip` and is
excluded from Git. The script prints its SHA-256 checksum.

Project layout:

```text
source/filter/       Aurora-loadable filter and metadata
source/lua/          Shared matching helpers for future UI work
source/skin/         Future Aurora XUI skin sources
scripts/             Release tooling
reference/           Research notes and local tooling guidance
original/            Local stock Default.xzp; never committed
build/               Generated releases; never committed
```

The future visual selector requires `XuiTool.exe`, `AuroraElements.xml`, and an
XZP packer. It will be developed against Aurora 0.7b.2's stock `Default.xzp`.

## Acknowledgements

The filter integration follows the public
[XboxUnity AuroraScripts](https://github.com/XboxUnity/AuroraScripts) format.
Aurora and its scripting API are maintained by XboxUnity/Phoenix.
