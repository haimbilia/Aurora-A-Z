# Aurora A-Z v1.0

Skin-independent alphabetical browsing and filtering for Aurora 0.7b.2 Rev1655.

## Install

Download `AuroraAZ.xex`, rename it to `NetDbgDll.xex`, and copy it to your
Aurora `Plugins` folder. Back up any existing file first, then reboot.
No launch.ini edits. The Network Debugger and Aurora A-Z cannot share this slot.
Experimental DashLaunch installers are not included or supported in v1.0.

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
