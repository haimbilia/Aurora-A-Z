# Aurora A-Z — Handoff

Written 2026-09-02. Target: Aurora 0.7b.2 Rev1655 on Xbox 360, library of 2241 titles.

Read this before touching anything. Most of it was expensive to learn and several
conclusions contradict what the README and CHANGELOG still say.

---

## 1. Status right now

The legacy A-Z experiment has been removed from the console. A read-only pull
of the live `settings.db` on 2026-09-02 confirmed the stock 7 QuickViews, zero
`AURORA_AZ` rows, zero one-character `NameFilter` rows, and no `AuroraAZ*`
settings. The fixed uninstaller completed the cleanup described in §7.

The console currently runs the production Aurora copy. Nothing we built is
active there. The inert canary was tested only in `Hdd1:\AuroraAZLab\` and was
then recoverably disabled. The latest, successfully loaded file is now
`Plugins\NetDbgDll.xex.disabled-c51e3a322b07`; the active lab target path is
absent again and the lab was restarted in that state.

### Current console state

```
QuickViews                                        stock 7 rows
User/Scripts/*                                    no Aurora A-Z scripts
Skins/*                                           no Aurora A-Z test skins
Production Plugins/NetDbgDll.xex                  absent
AuroraAZLab Plugins/NetDbgDll.xex                 absent (failed canary disabled)
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

The target is a persistent `# A B C ... Z` row over the coverflow with the current
letter highlighted, responding to the D-pad.

- **Persistent row on the coverflow:** impossible in a skin. Nothing can update it.
- **Highlight tracking state:** impossible in a skin, for the same reason.
- **In Lua:** impossible. No scene API, no input.
- **Closest achievable:** restyle `ScnQuickView` (§5). One highlighted letter at a
  chosen Y over the live coverflow, LB/RB and the highlight driven by Aurora.
  Built and uploaded; **never tested on hardware**.
- **The real thing:** needs native code in Aurora's process. Rev1655 creates
  exactly seven hard-coded wrappers and does not discover arbitrary
  `Plugins/*.xex` files. The optional key-7 Network Debugger wrapper is the
  current bootstrap candidate: it requests the literal
  `Plugins\NetDbgDll.xex` path, resolves ordinals 2-5, and the recovered logger
  calls only 2-4 with ignored returns. The release archive and live console
  contain no stock `NetDbgDll.xex`, so the candidate occupies an unused
  optional path rather than replacing an installed feature. It would keep the
  product skin agnostic and one-file with no `launch.ini` change. The corrected
  hardware canary now passes the wrapper load and ordinal-resolution gate, but
  M1 remains open because its own code-execution signal was not observed. See
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

### Corrected loader retry: wrapper gate passed

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

This proves that the corrected XEX enters Aurora's module container and reaches
the post-resolution loaded notification for the wrapper that requests ordinals
2-5. It does **not** yet prove that AuroraAZ's initialization path executed:
the log contains no `AuroraAZ` line and NOVA reported no live thread in
`0x91D00000-0x91DFFFFF`.

After evidence capture, the file was renamed
`NetDbgDll.xex.disabled-c51e3a322b07`, the lab restarted without an active
target, and production Aurora was restored. Production files and `launch.ini`
were untouched. The next gate is a minimal, independently observable entry or
export-call signal; do not link hooks yet.

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
