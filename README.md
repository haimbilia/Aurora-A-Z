# Aurora A-Z

A skin-independent alphabet selector for **Aurora 0.7b.2 Rev1655** on Xbox 360.
Jump through large game libraries or filter by letter—without changing skins.

[Download v1.0](https://github.com/haimbilia/Aurora-A-Z/releases/tag/v1.0)

## Install

1. Download `AuroraAZ.xex` from the release.
2. Rename it to `NetDbgDll.xex` and copy it into your Aurora `Plugins` folder:
   `Hdd1:\Aurora\Plugins\NetDbgDll.xex`.
3. Reboot the console.

Back up any existing `NetDbgDll.xex` first. Aurora A-Z uses the Network Debugger
slot, so the two cannot run together. **No launch.ini edits or DashLaunch plugin
entry are needed.** Do not use the experimental DashLaunch installers with v1.0.

## Controls

- **Hold R3:** show the alphabet over a dimmed coverflow.
- **D-pad / left stick left or right:** move the highlight; hold to repeat.
  Navigation wraps and remembers the last letter.
- **Release R3:** apply the letter. Without moving, release cancels.
- **While holding R3, click L3:** switch Browse/Filter mode. The saved mode appears
  bottom-right for five seconds; switching again restarts the timer.

**Browse** jumps to the first matching title in the current QuickView without
rebuilding the list. **Filter** shows matching titles; large libraries can take
longer. `ALL` appears only in Filter mode and clears only the alphabetical filter,
preserving your QuickView. A and RB keep their normal Aurora functions.

## Compatibility & removal

Requires a homebrew-capable Xbox 360 and the exact Aurora revision above.
Module settings radio UI and the module icon remain unfinished; use R3+L3.
To uninstall, remove this plugin's `NetDbgDll.xex`, restore any original debugger
file you backed up, and reboot. Skins and `Aurora.xex` are untouched.

[Changelog](CHANGELOG.md) · [Technical docs](HANDOFF.md)
