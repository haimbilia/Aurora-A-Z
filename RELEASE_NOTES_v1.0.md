# Aurora A-Z v1.0

Skin-independent alphabetical browsing and filtering for Aurora 0.7b.2 Rev1655.

## Install

Download `AuroraAZ-v1.0-Installer.zip`. Extract and copy `AuroraAZInstaller` to
`Hdd1:\Aurora\User\Scripts\Utility\`, keeping Main.lua, AuroraAZ.xex and icon.png
together. Open Aurora's Scripts menu, press X to refresh, launch **Install
Aurora A-Z** under Utility and choose **Install**. Reboot the console manually.

The script stages the plugin, renames an existing NetDbgDll.xex to an unused
backup name, and installs the payload into Aurora's Plugins folder. No launch.ini
or skin changes, no automatic restart. The Network Debugger and Aurora A-Z cannot
share this slot. This package replaces the experimental DashLaunch installer.

Manual alternative: back up the existing plugin, copy the standalone
`AuroraAZ.xex` as `Aurora\Plugins\NetDbgDll.xex`, then reboot.

## Use

Hold R3, move with D-pad/left stick, then release to apply. Hold left/right for
repeat; navigation wraps and remembers the last letter. An unmoved R3 tap cancels.
While holding R3, click L3 to switch and save Browse/Filter mode. Its name appears
bottom-right for five seconds, even after R3 release.

Browse jumps within the current QuickView without rebuilding the list. Filter
shows matching games; ALL clears only the alphabetical filter. Large libraries
can take longer to filter. Module settings radio UI and icon remain deferred.

Hardware-confirmed binary: `f0ff4325fa867c78ed16f97cedc56c3992bb3c79`.
CI run: `33956736531`; 26 host tests passed.
SHA-256: `FD928EBB0490966BB232C1E199D650D31B8483B11A7C8EE8C827E1ED1A26F1D7`.
