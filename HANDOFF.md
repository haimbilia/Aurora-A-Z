# Aurora A-Z — Handoff

## v1.0 release — 2026-09-05

User confirmed `f0ff432` works, including the five-second mode notification.
v1.0 ships that exact hardware-tested binary, SHA-256
`FD928EBB0490966BB232C1E199D650D31B8483B11A7C8EE8C827E1ED1A26F1D7`.
Installation and supported controls are in the concise README. NetDbg is the
current loader, not DashLaunch. No installer is shipped in v1.0; older installer
sources/artifacts are experimental and must not be used for this release.
Module radio UI and icon remain deferred. Historical notes below are evidence,
not current user instructions. The uncommitted SynthXEX title-TLS experiment
was not used to build the released binary and remains outside the release.

## Return to NetDbg — 2026-09-05 (supersedes deployment notes below)

User requested abandoning DashLaunch for now and returning to NetDbg.
Restore artifact: `build/ci-artifacts/266e680/runtime/AuroraAZ.xex`, the
last pre-DashLaunch build with R3+L3 mode switching. SHA-256:
`44D481FBDA11FACCF880B33D8CF2ED0A2330CDDE38894B9DCF29C2B1B418D6E9`.
Deployed: `/Hdd1/Aurora/Plugins/NetDbgDll.xex`. Both staged and active FTP
roundtrip hashes matched. Fresh reboot and hardware acceptance are still required.
All five HDD launch.ini plugin slots were checked empty; no edit needed.
Keep the failed DashLaunch payload and installer disabled. Current source/CI
still builds DashLaunch experiments: do not deploy those as NetDbg artifacts.
User confirmed the restored build works. Module radio UI and icon bugs remain
deferred. New polishing change: bottom-right Browse Mode / Filter Mode notice,
five seconds from mode change, independent of R3 release. Build script now
uses the NetDbg title-DLL path again. Do not use the historical DashLaunch
installer; this update is a direct plugin replacement only.

Notice candidate `f0ff432`: CI `33956736531` passed; 26 local host tests
passed, including notice-only rendering and no rendering when dismissed.
Artifact SHA-256:
`FD928EBB0490966BB232C1E199D650D31B8483B11A7C8EE8C827E1ED1A26F1D7`.
Staged FTP roundtrip verified. Previous active plugin backed up to
`Plugins/NetDbgDll.xex.before-mode-notice`. User reboot/visual acceptance pending.

## Historical DashLaunch investigation — 2026-09-05

STOP DEPLOYMENT: build `923e6ef` caused startup to hang after full reboot.
FTP port 21 is unreachable. Recovery takes priority: bypass DashLaunch plugins
on boot, then disable `Plugins\AuroraAZ.xex` and its plugin1 entry. The Utility
installer also contains this failing payload and must be disabled before use.
Host tests and CI passed but did not establish hardware boot safety.

Recovery completed via USB override. HDD launch.ini was downloaded, only its
AuroraAZ plugin1 value cleared, staged, and read back with a matching hash.
Old configuration: `/Hdd1/launch.ini.pre-az-recovery`. Failed binary renamed
to `/Hdd1/Aurora/Plugins/AuroraAZ.failed-923e6ef`; installer Main.lua renamed
to Main.disabled. All five HDD plugin slots are empty; Default still points
to Hdd:\Aurora\Aurora.xex. User requests continued DashLaunch development.

On-demand diagnostic build `f1ecd79` passed CI run `33955043811` and all
26 host tests. Its probe and runner are staged and hash-verified in
`/Hdd1/AuroraAZProbe/` (not a boot plugin or replacement dashboard).
Nova POST /title/launch accepted the runner with HTTP 202 using curl multipart
forms; PowerShell -Form was rejected with HTTP 400. Aurora was reachable again
after ten seconds, but `probe-result.bin` was absent in both the runner folder
and Aurora root. Do NOT count this as proof of probe DllMain execution. It is
not yet known whether the title runner entered main, failed its first file
write, or was rejected during launch. Logs are saved under
`build/ci-artifacts/f1ecd79/`. No further boot plugin has been enabled.

Next diagnostic is the separate `AuroraAZ-boot-probe.xex`, built by
`scripts/build-dashlaunch-probe.sh`. It only updates a 24-byte `.azboot`
record in DllMain. No threads, I/O, waits, Aurora calls or hooks. Packaged as
sysdll at preferred base 0x91D00000; JSON gives status RVA for a later external
memory read (add actual module base if relocated). It is NOT the selector.
Do not substitute the full failed runtime or re-enable its installer.

User confirmed a full console reboot with system-DLL build `84cad88`; R3
remains inactive and no fresh M2a startup marker appeared. Earlier telemetry
was stale. Container type alone is not an established cause of this failure.
The current fix removes DllMain's dependency on Aurora's private thread wrapper:
ExCreateThread starts a plugin-owned startup routine, waits up to two minutes
for exact Aurora admission, then uses the validated title wrapper for runtime
initialization. Bootstrap markers use `Hdd:\Aurora\Data\Logs` so they do not
depend on the boot-time `game:` mount. The new DashLaunch host test covers
startup before admission, repeated attach, delayed admission and handoff.
Hardware acceptance and system-module behavior across title relaunch remain
unproven. Do not call this release hardware-verified until fresh logs and R3
behavior confirm it. Production uses `Plugins\AuroraAZ.xex` via launch.ini;
the historical NetDbg deployment descriptions below are superseded.

Written 2026-09-02. Target: Aurora 0.7b.2 Rev1655 on Xbox 360, library of 2241 titles.

Read this before touching anything. Most of it was expensive to learn; the
native bootstrap status below supersedes the earlier failed-attempt notes.

---

## 1. Status right now

The polished one-file build at commit `ee93a9d` is the current production
rollback baseline. Hardware confirmed its centered dimmed `ALL # A ... Z`
row, enlarged highlight without the duplicate small glyph, release animation,
repeated filtering, and normal game launch. Do not replace it with an
unverified candidate.

Browse mode and the module row now work on production hardware. Browse scans
Aurora's already-sorted active QuickView vector, then hands a direct first-match
jump back to the main UI thread; it does not rebuild the list. The first
settings implementation used a system message box and froze Aurora after a
selection, so it is rejected. The current candidate hooks the key-7 dispatcher
at `0x822C8B88` and loads an embedded, classless settings XUR into Aurora's
normal `ModuleHost`, matching the FTP/Nova page integration. Browse and Filter
use Aurora-style radio rows. The saved mode chooses the matching embedded XUR,
initial focus, checked marker, and status; A queues persistence to the worker
so the UI thread performs no file I/O.
The same candidate continuously reapplies the embedded `icon.png` artwork to
the Aurora A-Z module row because Aurora may asynchronously restore the stock
Network Debugger icon after the row is first populated. Exact ABI details are
in `reference/BROWSE_MODE_REV1655.md`.

The legacy A-Z experiment has been removed from the console. A read-only pull
of the live `settings.db` on 2026-09-02 confirmed the stock 7 QuickViews, zero
`AURORA_AZ` rows, zero one-character `NameFilter` rows, and no `AuroraAZ*`
settings. The fixed uninstaller completed the cleanup described in §7.

M1 is complete. The passing one-file canary was tested only in
`Hdd1:\AuroraAZLab\` from commit `39b551c`, GitHub Actions run `33604028771`.
Its CI and FTP round-trip SHA-256 was
`87894F41A89F4F3CAAFA8A1864AB8F8A91A2ED011882EEEF36E4D3FAEF58596C`.
The test did not modify the production Aurora tree or `launch.ini`. Before the
next lab deployment, enumerate the lab plugin directory rather than relying on
this document to assert whether a later session left the active target present.

M3a interaction passed on hardware on 2026-09-03 with commit `c86ad0c`, run
`33747222158`, and SHA-256
`94F32460DBC5A76153F63BB9B23158E7CFE690277D792DBB7822666B93B09CF8`.
The full centered row rendered without diagonal clipping. R3 selected `#`,
D-pad and left-stick Left/Right moved the highlight, and RB remained stock.
A was deliberately consumed but inert because the filter gate stayed false.
M3b pre-hook bind plus the read-only all-27 registry and active aggregate
copy/validate/destroy probe passed immediately afterward. `AZF3` v1 reported
`bind_result=1` (`idle`), `probe_result=1` (`idle`), `probe_count=1`,
`runtime_verified=1`, and `disabled=0`. The one-shot live apply from commit
`43aba10`, run `33750116775`, then passed on hardware: the first A press loaded
the selected letter filter, `worker_step_result=0` (`scheduled`),
`scheduled_count=1`, `rejected_count=0`, and the apply completed without a
crash. A second attempt was correctly inert because that safety build revoked
its filter gate after the first enqueue. The next experimental build replaces
that one-shot gate with conservative repeat arming: at least eight seconds
after a successful schedule and one continuous second with Aurora's queue
empty and worker-busy byte clear. It remains lab-only until two sequential
letter changes pass on hardware.

The first repeatable candidate, commit `ffceb8f`, exposed a startup race rather
than an apply failure. Hardware marker `AZF3` v3 reported `bind_result=9`
(`bad-image`), `probe_result=4` (`not-bound`), and zero requests, so A correctly
remained fail-closed. The initial whole-image revision gate had already passed
(the hooks and overlay started), but the filter binder redundantly hashed all
live Aurora text again after its worker-thread handshake. Aurora changed an
unrelated text byte during that gap. The follow-up reuses the opaque permit
from the successful initial exact-revision gate, verifies that it belongs to
the same loaded image, and still rechecks every filter helper signature before
installing this plugin's first hook.

The controller contract changed on 2026-09-04 after an unarmed build allowed A
to reach Aurora and launch the highlighted game. The row is now hidden during
normal coverflow use. Press and hold R3 to show it at `ALL`, navigate with D-pad
or left-stick Left/Right while continuing to hold R3, and release R3 to apply
and hide it. A press/release of R3 without any actual highlight movement is a
cancel/no-op and must not apply `ALL`. Aurora A-Z no longer assigns, consumes, or
clears A at all. Hardware confirmed the first hold/release build filtered
correctly, but its fixed 8-second plus 1-second-idle re-arm window was visibly
slow. The next build uses observed queue/busy activity followed by 200 ms of
stable idle; the old 8-second timeout remains only as a missed-activity safety
fallback.

Hardware then confirmed commit `57dd888` fixed both interaction details: an
unmoved R3 press/release is a no-op, and the selector re-arms after Aurora's
actual filter rebuild rather than the old visible delay. A separate A/B test
found a release-blocking lifecycle bug: launching a known-good title from the
lab black-screened with `NetDbgDll.xex` active, while the identical lab copy
launched it successfully after that file was disabled. The failing log reached
`ContentLauncher: INITIALIZE`, printed the selected ContentID, closed
`AuroraSql`, and stopped. Do not deploy `57dd888` to production. The follow-up
candidate at `8a6dcf0` routed NetDbg ordinal 3 to a non-blocking runtime
shutdown request, but hardware still black-screened. After reboot its v5
marker recorded `shutdown_requests=0` and runtime state `RUNNING`, proving
Aurora does not call ordinal 3 before the failing handoff. The next candidate
therefore gates and intercepts the exact Rev1655 `ContentLauncher` entry at
`0x82294DD0`. The first implementation requested shutdown and waited for the
worker before resuming at `0x82294DD4`; hardware froze at `Gathering
information 0%`. Waiting on the launcher thread is therefore forbidden. The
revised bridge only signals the worker and immediately resumes Aurora, giving
cleanup the information-gathering interval to cancel filtering, restore the
ContentLauncher, Font::End, RenderMenu, and input hooks, and exit
asynchronously. The revised nonblocking build subsequently passed the
production game-launch gate recorded at the top of this section.

After reboot, the frozen synchronous candidate's persisted v5 marker decoded
with `shutdown_requests=1` and runtime state `CLOSED`. That proves the boundary
fired and the worker finished cleanup; the freeze was caused by holding the
launcher thread across that cleanup, not by a missed hook or a worker that
remained running.

### Current console state

```
QuickViews                                        stock 7 rows
User/Scripts/*                                    no Aurora A-Z scripts
Skins/*                                           no Aurora A-Z test skins
Production Plugins/NetDbgDll.xex                  d1cfced rollback baseline active
AuroraAZLab Plugins/NetDbgDll.xex                 verify before each experiment
M1 primary marker                                 ordinal 4 / phase 5 / running
M1 worker marker                                  ordinal 4 / phase 7 / running
```

Original skins were never modified. Untouched local references are in the
ignored `original/Skins/` directory.

### Console access

FTP to the console's LAN address, port 21. Credentials are Aurora's FTP settings
(Settings → Network); they are deliberately not recorded in this repo. Aurora's
`FtpDll` is quirky:

- `LIST <path>` ignores the argument and lists the root. **CWD first, then bare `LIST`.**
- Same for `RETR`. CWD to the directory, then `RETR <filename>`.
- Connections drop constantly. Wrap connect in a retry loop (~8 attempts, 4-5s apart).
- `settings.db` and `content.db` are in `Data/Databases/`, not `Data/`.

---

## 2. The actual performance problem

This was the user's real complaint and it is **not** what the project was built to solve.

`debug.log` times every list rebuild. Four samples:

| stage | boot | boot | RB switch | RB switch |
|---|---|---|---|---|
| `Sorting Game List` → `Filter Game List` | 7.08s | 6.05s | 5.25s | 5.29s |
| `Filter Game List` → `Swap Active List` | 0.31s | 0.32s | 0.33s | 0.36s |

**The sort is ~94% of the cost. Filtering is ~0.33s.** Any design that optimises
filtering is chasing 6%. Boot additionally spends ~47s before `Sorting Game List`
even starts.

Every QuickView change pays the full ~5.6s, and the sort is identical every time
because all A-Z views use `SortMethod = 'Title Name'`.

`debug.log` is a free profiler. Grep for `GameListManager` and diff the timestamps
before believing any performance claim.

**Untested and worth testing:** does switching between two QuickViews with the
*same* `SortMethod` still log `Sorting Game List`? If Aurora skips the re-sort,
the cost model changes completely.

---

## 3. Aurora's Lua API — complete, from a live dump

Lua **5.3**. Produced by `AuroraAZApiDump`; rerun it if you doubt any of this.

```
Script (18)   CreateDirectory  FileExists  GetBasePath  GetProgress  GetStatus
              IsCancelEnabled  IsCanceled  RefreshListOnExit  SetCancelEnable
              SetProgress  SetStatus  ShowFilebrowser  ShowKeyboard
              ShowMessageBox  ShowNotification  ShowPasscode  ShowPasscodeEx
              ShowPopupList

Aurora (24)   hashes, temperatures, IP/MAC, DVD tray, memory, language, skin
              name, dash version, Reboot, Restart, Shutdown

also          Sql  Profile  IniFile  ZipFile  Thread  GizmoUI  BuildType  Days
```

What is **absent**, and each absence killed a design:

- **no `io`, no `os`** — a script cannot read or write a binary file. This is why
  skin patching cannot be an Aurora script.
- **no scene/UI API** — no element creation, no input handling. The
  `XuiObject.Scene` / `XuiMessage.KeyDown` enums in `UnityLiNKInfo/AuroraUI.lua`
  are dead weight; nothing in this build implements them.
- **no `Content` package** — cannot enumerate or launch titles from Lua.

Confirmed positives:

- **`Sql` reaches both databases.** `QuickViews` → 34 rows, `ContentItems` → 2241.
- **`Script.RefreshListOnExit`**, not `SetRefreshListOnExit`. The wrong name was
  the `LUAERROR` on the first install.
- `Script.ShowPopupList(title, emptyText, items)` → `{Canceled, Selected{Key,Value}}`
  is the only list UI. `MenuSystem.lua` builds nested menus on it.
- `print()` reaches `debug.log` as `LUA > <text>`. Use it for anything you need
  to observe remotely.

Scripts are modal and one-shot. They can show UI while open; they cannot run
behind the coverflow or see its input.

---

## 4. Filters — Aurora already does this

**Stock filter scripts register into `GameListFilterCategories.User`.** Not a
custom table. Every stock example (`HideKinect`, `HideBackups`, `HideMultiDisc`,
`OnlyLocalCoop`) does:

```lua
GameListFilterCategories.User["Hide Kinect"] = function(Content) ... end
```

The title field is **`Content.Name`** (every stock sorter uses `Item.Name`).

**Filter names in `QuickViews.FilterMethod` are fully qualified.** The boot log
prints the exact identifiers. A bare name is rejected:

```
CQuickViewSettingObject  QuickView 'G' has invalid filter method (syntax error). Ignoring entry
```

Stock views use `'Xbox 360'` only because those filters are top-level. Ours needed
`User.A-Z G`.

**Aurora ships a complete built-in A-Z filter set:**

```
NameFilter.A - F.{A..F}     NameFilter.G - L.{G..L}    NameFilter.M - R.{M..R}
NameFilter.S - X.{S..X}     NameFilter.Y - Z.{Y,Z}     NameFilter.Other
```

So `source/content/Filters/AuroraAZ.lua` is **redundant** and the installer now
points at the native filters. Delete the custom filter script unless you need a
predicate Aurora lacks, e.g. stripping leading articles so "The ..." doesn't all
land under T.

`QuickViews` schema: `Id, DisplayName, SortMethod, FilterMethod, Flags,
CreatorXUID, OrderIndex, IconHash`. `Flags = 2` for filter-expression views.

---

## 5. Skins — what is and isn't possible

**Every scene loads from the active skin package. There is no override path.**

```
SkinManager  Adding Cache: Path: memory://40200030,4F0B6D#Aurora_QuickView.xur
SkinManager  Adding Cache: Path: memory://40200030,4F0B6D#Aurora_Main.xur
```

Same memory blob for all of them. Anything you want to change in Aurora's UI
means shipping a modified skin. Full stop.

### Aurora_Main is a dead end for custom UI

Aurora fills exactly ten text elements:

```
GameTitleInfo  GameListCounterInfo  StatusInfo  textDiscTitle  textNoDisc
textGamerTag   textCredScoreAndAchievementsEarned  RSSFeedMessage1/2  RSSFeedStatus
```

`DataAssociation` appears on exactly two elements, both `XuiImagePresenter`
(`texCoverflow`=1, `BackgroundPresenter`=2). The coverflow is **one render
target**, not a list — there is no `XuiList` in `Aurora_Main` and no per-item
element to select.

Consequence: a custom element added to a skin can be **drawn but never updated
and never interacted with**. That is why the alphabet row from r2-r4 could not
work, and why `XuiList.SetCurSel()` does not apply to the coverflow.

**Do not repurpose `QuickViewRB`.** Stock is `PressKey=22532` (RB), `Enabled=false`;
Aurora handles RB natively. r3/r4 overwrote it with `22545` (D-pad Down) and
**broke the RB button**. Verified on hardware.

### Aurora_QuickView is the one usable hook

`Aurora_QuickView.xur` → `ScnQuickView` / `ClassOverride: ScnQuickViewUI`:

```
Animator   XuiTabScene  TabCount=5  Wrap=true  NoAutoHide=true
  QVText1..5      Aurora fills these with QuickView DisplayName
  QVImage1..5     Aurora fills these from Images\Icons\QuickView\
  Tab1..5
LeftShoulderIcon / RightShoulderIcon      AButton/BButton/XButton/YButton
```

Aurora drives the text, the selection highlight, LB/RB input, and hand-authored
transition frames (`1To2`, `2To3`, `5To1`, ...). **This is the only place a
skin can show state Aurora actually maintains.**

`Animator` animates offscreen → rest → offscreen, so the timeline visits the
offscreen Y twice. **The resting position is the minimum Y**, not the last
keyframe. Skins disagree on it:

| skin | rest Y |
|---|---|
| Default / Dark / Dark Theme Ultimate | 602 |
| Series | -115.07 |

Any patch must read the rest Y and shift **relative**, never absolute.

Only `QVText1` is visible at rest; 2-5 sit at opacity 0 offscreen as animation
buffers. So out of the box this gives **one letter at a time**, not a full row.
Showing several side by side means rewriting the `Animator` timelines. Whether
`TabCount` can exceed 5 (with `QVText6`, `QVText7`, ...) is **untested** and is
the gate on ever reaching the full `# A B ... Z`.

### Other scenes

`Game_Browser.xur` (Quick Browse, X button) contains a real `XuiList`
(`GameBrowserList`) plus `SearchButton`. Never evaluated. For "find one game out
of 2241" it may beat everything we built, at zero cost.

---

## 6. Tooling

```
scripts/xzp.ps1                            XZP extract/build  (production)
research/patch-skin.ps1                    skin patcher       (research only)
research/quickview-alphabet-row.ps1        the XUI edit       (research only)
```

The two `research/` scripts are rejected as a product route; see
`research/README.md`. They remain because they are the only hardware-verified
proof that `ScnQuickViewUI` is drivable.

```powershell
pwsh -File research\patch-skin.ps1 `
     -SkinPackage "original\skins\Series.xzp" `
     -OutputPackage "out.xzp" [-Offset 70]
```

Needs `tools\XUIHelper\XUIHelper.CLI\bin\Release\net8.0\XUIHelper.CLI.exe`
(gitignored). Decompile with `-f xuiv12`, recompile with `-f xurv5 -g AuroraV5`.

Verified on Default, Dark, Dark Theme Ultimate, Series: each output changes
**exactly two entries** (`Aurora_QuickView.xur`, `skin.meta`), everything else
byte-identical, `Aurora_Main.xur` untouched.

**Gotchas:**

- `Series.xzp` ships a **corrupt `skin.meta`** — valid JSON followed by leftover
  bytes from a longer previous version. Aurora stops at the closing brace and
  doesn't care; `ConvertFrom-Json` throws. The renamer does a targeted text edit,
  which also stops `ConvertTo-Json` reformatting every other skin's file.
- `xzp.ps1` round-trips content correctly but **not byte-identically**:
  `Sort-Object Name` is culture-aware and orders `Aurora_Settings_PathConfig.xur`
  before `Aurora_Settings.xur`. Ordinal sort would make rebuilding stock
  hash-verifiable. Worth doing; it is the only real algorithmic risk in the repo
  and there are still zero tests.

`.cfljson` files in `Media/Layouts/` are coverflow geometry only —
`rows/columns/pagesize` plus 9 modes of `covers/camera/light/mirror/grid`. No
text, no UI, cannot help the alphabet row. Every stock layout is `rows:1`; a
multi-row grid is untried and is a cheap, zero-risk way to get more games on
screen.

---

## 7. Mistakes made here, so they aren't repeated

1. `GameListFilterCategories.AuroraAZ` instead of `.User` — filters never registered.
2. Bare `'A-Z G'` in `FilterMethod` instead of `User.A-Z G` — "invalid filter method".
3. Repurposing `QuickViewRB` for D-pad Down — **broke the RB button**.
4. `SetRefreshListOnExit` instead of `RefreshListOnExit` — `LUAERROR` on install.
5. Narrowing the install/uninstall `DELETE` to `IconHash = 'AURORA_AZ'` after an
   earlier build had written rows with `IconHash = ''`. The next install matched
   nothing and **appended a second full set of 27**. Install and uninstall must
   use the *same* predicate and it must cover every generation ever shipped.
6. Building four skin revisions on an architecture that could never work,
   because `Aurora_Main` was never inspected for what Aurora actually drives.
   Check what the host fills before designing UI around it.

---

## 8. The goal, honestly assessed

The target is a transient `# A B C ... Z` row over the coverflow, visible only
while R3 is held, with the current letter highlighted and responding to the
D-pad or left stick.

- **Dynamic row on the coverflow:** impossible in a skin. Nothing can update it.
- **Highlight tracking state:** impossible in a skin, for the same reason.
- **In Lua:** impossible. No scene API, no input.
- **Closest achievable:** restyle `ScnQuickView` (§5). One highlighted letter at a
  chosen Y over the live coverflow, LB/RB and the highlight driven by Aurora.
  Built and uploaded; **never tested on hardware**.
- **The real thing:** needs native code in Aurora's process. Rev1655 creates
  exactly seven hard-coded wrappers and does not discover arbitrary
  `Plugins/*.xex` files. The optional key-7 Network Debugger wrapper is the
  hardware-proven bootstrap: it requests the literal
  `Plugins\NetDbgDll.xex` path, resolves ordinals 2-5, and the recovered logger
  calls only 2-4 with ignored returns. The release archive and live console
  contain no stock `NetDbgDll.xex`, so the candidate occupies an unused
  optional path rather than replacing an installed feature. It would keep the
  product skin agnostic and one-file with no `launch.ini` change. M1 passes
  wrapper loading, ordinal resolution, automatic ordinal-4 dispatch, and
  AuroraAZ worker entry. M2a direct input observation also passes on hardware;
  input consumption, hardware-proven overlay drawing, and filter mutation are
  not yet functional. See
  `reference/NETDBG_BOOTSTRAP.md`.

### First native loader canary: failed safely

The first OpenXeChain XEX was produced by successful CI, validated offline,
uploaded only to `Hdd1:\AuroraAZLab\Plugins\NetDbgDll.xex`, and downloaded
again with the same SHA-256:

```text
B20E2F54608FE071BACBFE2FF8221158A72D7577D51D5B82E297CE35E59699BA
```

The lab Aurora survived and remained usable, but NOVA found no thread in the
canary's reserved `0x91D00000-0x91DFFFFF` window. The pulled lab log begins:

```text
Failed to load game:\Plugins\NetDbgDll.xex
Failed to load NetDbgDll
```

This locates the failure before `DllMain`, module-handle recovery, or ordinal
resolution. Production Aurora was never given this image, the default
`launch.ini` target remained `Hdd:\Aurora\Aurora.xex`, and the failed lab file
was disabled by the recoverable rename recorded in §1.

Raw XEX comparison found three fields changed for the retry: module flags move
from `0xA` (`sysdll`) to `0x9` (`titledll`), optional header `0x10201` now
records image base `0x91D00000`, and the synthesized empty `0x20104` TLS header
is omitted. Do not turn those facts into a proven cause: stock Nova loads with
flags `0xA`, stock Aurora carries an empty TLS tuple, and Aurora wrapper mode
`9` is not the same field as XEX module flags `0x9`. The missing Image Base
Address header is the strongest difference because every inspected working
Rev1655 XEX has it. The corrected bundle has now passed that loader test, but
the combined change does not isolate which field fixed the rejection.

### Corrected loader retry: wrapper gate passed (historical)

The corrected 24,576-byte XEX round-tripped through FTP with SHA-256:

```text
C51E3A322B07D1DE094C644E33D005D87305FFB24B587548953F1E88678C63E5
```

AuroraAZLab remained usable at 1280x720 and its log contained exactly these
successful wrapper events:

```text
IDllBase::Load: Completing DLLModule loading:  dll.aurora.netdbg
PluginManager: Module Loaded:  dll.aurora.netdbg
```

This earlier result proved that the corrected XEX entered Aurora's module
container and reached the post-resolution loaded notification for the wrapper
that requests ordinals 2-5. Its original `DllMain`/NOVA observation did not
prove AuroraAZ worker entry, so a later canary used durable marker records.

After that earlier evidence capture, the file was renamed
`NetDbgDll.xex.disabled-c51e3a322b07`, the lab restarted without an active
target, and production Aurora was restored. Production files and `launch.ini`
were untouched.

### Final M1 canary: code-execution gate passed

The passing artifact was built from commit `39b551c` by GitHub Actions run
`33604028771`. Its CI and lab round-trip SHA-256 was:

```text
87894F41A89F4F3CAAFA8A1864AB8F8A91A2ED011882EEEF36E4D3FAEF58596C
```

Raw `ExCreateThread`/startup combinations were not sufficient under Aurora
Rev1655.
The passing implementation validates and calls Aurora's complete Rev1655
thread wrapper at `0x82361AA8`, after validating the first 32 bytes of
`XapiThreadStartup` at `0x82804650`. The wrapper supplies that startup routine,
uses create flags `2`, selects processor `3`, sets priority `15`, and resumes
the handle once.

The primary 36-byte big-endian `AZM1` v4 record reported `call_count=1`,
`source_ordinal=4`, `phase=5` (`COMPLETE`), `state=2` (`RUNNING`), and zero
create/resume statuses. The separate worker record kept the same identity and
statuses and reported `phase=7` (`WORKER_ENTERED`). The source field proves
Aurora called ordinal 4 automatically; the separate phase-7 record proves the
AuroraAZ worker entered. M1 is therefore complete.

### M2a direct input gate: passed

Commit `06affc4`, GitHub Actions run `33736960588`, produced the 397,312-byte
hardware-passing artifact with SHA-256:

```text
431FAD613E1C177B5B5A486B5B21B98AB17BB2AC2592C1A6F630DC07E68EB86E
```

The v3 `AuroraAZ-M2a.bin` marker returned runtime result 0 and target
`0x82801D90`. On the RGH/freeBOOT console, the direct compare/exchange patch
worked even though `MmQueryAddressProtect` continued reporting `0x20` before
and after the advisory protection change. Final input telemetry selected slot
A, generation 57, with 139 relevant observations, zero invalid events, zero
drops, and clean safety masks. All seven controls were present. The deliberate
left-stick Right hold recorded 4 presses, 74 repeats, and 5 releases. Every
event remained unconsumed and no filter request was queued. Aurora stayed
responsive.

The current work is an explicit `OVERLAY_CANARY`: direct RenderMenu and
Font::End hooks feed the existing renderer while the input bridge remains in
OBSERVE and filter verification remains false. This canary is title-lifetime
and cold-restart-only because direct hooks have no hot-unload admission relay.
Do not describe the selector as functional until visible rendering, later
input ownership, and filtering each pass their hardware gates.

The user rejected the popup-list fallback and does not want a 27-QuickView
carousel. If the mockup is non-negotiable, the honest next step is reverse
engineering, starting with how `GameTitleInfo` and `GameListCounterInfo` get
driven — whatever fills those is what a custom row would have to hook.

`Nova.xex` (installed, documented in `tools/aurora-dev-docs/docs/nova-0.7b.2r1622/`)
exposes a REST API including `post_title_launch` and `get_title`. Unexplored, and
the only found route to launching a title programmatically.

---

## 9. Document map

| File | Role |
|---|---|
| `README.md` | Entry point and current status |
| `REQUIREMENTS.md` | Product contract. Normative controller and filtering behaviour |
| `ARCHITECTURE.md` | Architecture decision that follows from the requirements |
| `IMPLEMENTATION_PLAN.md` | Gated roadmap, M0-M7 |
| `HANDOFF.md` | This file. Verified facts about Aurora, measured or dumped |
| `research/README.md` | The rejected skin route and what to carry forward from it |
| `reference/RESEARCH.md` | External research notes |
| `reference/NATIVE_LOADER.md` | Rev1655 module-loader proof and bootstrap options |
| `reference/NETDBG_BOOTSTRAP.md` | Exact one-file NetDbg compatibility contract and test gate |
| `reference/NATIVE_HOOKS.md` | Exact Rev1655 input, render, and filter hook map |
| `reference/OVERLAY_IMPLEMENTATION.md` | Final-composite overlay ABI, atlas upload, and state contract |
| `reference/FILTER_IMPLEMENTATION.md` | In-memory A-Z ownership, snapshot, and async scheduler ABI |
| `reference/NATIVE_TOOLCHAIN.md` | Reproducible native build audit |

Rule of thumb: if it is a *decision*, it belongs in `ARCHITECTURE.md` or
`IMPLEMENTATION_PLAN.md`. If it is an *observed fact about Aurora*, it belongs
here, with the evidence that produced it.

Cleanup completed 2026-09-02: the r4 skin builders
(`build-functional-test.ps1`, `build-visual-test.ps1`, `add-alphabet-row.ps1`),
the redundant Lua filters (`source/content/Filters/AuroraAZ.*`), `release/`,
`source/skin/README.md` and the generated `build/` tree were deleted. They built
the design described in §7 item 6 and were a standing hazard.
