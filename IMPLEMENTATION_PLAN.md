# Aurora A-Z implementation plan

## Outcome

Build a skin-agnostic runtime extension for Aurora 0.7b.2 Rev1655 that renders
the mockup's centered `# A B ... Z` row over the live coverflow, enters selector
mode with R3, accepts D-pad and left-stick Left/Right, and applies the selected
letter with A. It must leave every skin, the on-disk `Aurora.xex`, and Aurora's
normal RB QuickView menu unchanged. The production release and installed
payload must be exactly one self-contained binary. It is released as
`AuroraAZ.xex` and, if M1 succeeds, installed as `Plugins\NetDbgDll.xex`, the
literal path used by Aurora's optional Network Debugger wrapper. Static
analysis proves the path and ordinal contract; hardware acceptance of the
one-file bootstrap is still pending.

`REQUIREMENTS.md` is the product contract. `ARCHITECTURE.md` is the architecture
decision. This file defines the order in which the risky parts must be proven.

## Rules for the implementation

- Do not build another patched skin as a production candidate.
- Do not install 27 persistent QuickViews for the production selector; that
  changes the RB menu and pays Aurora's full sort cost.
- Reuse Aurora's built-in `NameFilter` predicates instead of shipping redundant
  Lua filters.
- Embed all code, hook signatures, defaults, shaders, fonts, and glyph data in
  `AuroraAZ.xex`; do not require companion assets, scripts, configuration,
  QuickViews, database rows, or `launch.ini` changes.
- Do not permanently patch `Aurora.xex` on disk.
- Gate every native hook on the exact Rev1655 executable hash and expected
  instructions at the hook site.
- Develop against a separately launchable laboratory copy of Aurora. Do not add
  an experimental module to the console's default boot path.
- Every hardware experiment needs a removal or bypass procedure before it is
  deployed.

## Milestones and gates

| Milestone | Deliverable | Exit gate |
|---|---|---|
| M0 — Safe baseline | Clean database, console/repo snapshots, performance baseline, isolated Aurora lab copy | Stock seven QuickViews restored; production Aurora still boots; backups and hashes verified |
| M1 — Native lab | Reproducible Rev1655 analysis project and minimal export-capable `AuroraAZ.xex` | Installed as the one `Plugins\NetDbgDll.xex` file, it loads without unresolved ordinals and is disabled by removing that file |
| M2 — Input bridge | Log-only, then selectively consuming controller hook | R3, D-pad, left stick, and A are detected on the coverflow; RB and other scenes remain unchanged |
| M3 — Overlay spike | Runtime-owned row and highlight, with no filtering yet | Same overlay works on Default, CleanNXE, and two materially different third-party skins |
| M4 — Filter bridge | In-memory application of `NameFilter.Other` or `NameFilter.*.<letter>` | Correct results on 2,241 titles without adding QuickViews; latency gate passes |
| M5 — Vertical slice | Complete R3 → navigate → A state machine | All controller acceptance tests pass on one skin and the coverflow never moves while selecting |
| M6 — Fidelity and compatibility | Mockup-quality visuals and lifecycle handling | Visual/skin/resolution matrix passes with no skin-file changes |
| M7 — Release candidate | Single `AuroraAZ.xex`, compatibility gate, recovery guide, checksum | Cold-boot, stress, one-file removal, and unsupported-version tests pass |

Failure at a gate stops dependent work. A partial success must not be labelled a
functional release.

Current gate status: M0 is safe and reproducible. M1 is still open. The first
inert hardware canary was byte-verified after FTP upload and Aurora survived
the isolated lab launch, but `XexLoadImage` rejected the image before its entry
point ran. The corrected retry is described under M1 below; M2 and later work
must not be linked or deployed until that retry passes.

## M0 — Stabilize and measure

### Console cleanup

1. Back up the current `Data/Databases/settings.db` again.
2. On the console, run **Aurora A-Z Installer → Uninstall**, then restart.
3. Pull the resulting database and verify:
   - exactly the original seven QuickViews remain;
   - no `AURORA_AZ` rows or one-character `NameFilter` rows remain;
   - `AuroraAZInstalledVersion` and backup settings are gone;
   - the user's previous `DefaultQuickView` is preserved.
4. Leave the unused test skins and scripts inactive until their hashes and
   paths are recorded; remove them only as a separate, reversible cleanup.

### Safe laboratory copy

Create `Hdd1:\AuroraAZLab\` as a copy of the working Aurora installation and
launch it manually from a rescue-capable dashboard/file manager. Keep the
normal DashLaunch boot path pointing at the known-good Aurora copy. A crash in
the lab must recover with a power cycle and must not create a boot loop.

### Performance baseline

Use `debug.log` timestamps around:

```text
Sorting Game List
Filter Game List
Swap Active List Event
```

Measure at least five runs for each case:

- stock boot;
- switching between two QuickViews with the same sort method;
- switching between QuickViews with different sort methods;
- built-in name-filter application;
- Quick Browse/search as a comparison only.

Record median and worst-case values. The key question is whether Aurora can
apply a name filter without repeating the roughly five-second title sort.

### Repository hygiene

- Preserve the current installer/API-dump work in its own reviewed commit.
- Keep `HANDOFF.md` out of a public commit until live FTP credentials are
  removed or replaced with placeholders.
- Mark skin-patching scripts and custom A-Z filter scripts as legacy research;
  do not delete them until the clean database backup is verified.

## M1 — Prove native loading before writing hooks

### Offline analysis setup

1. Record SHA-256 hashes for the console's `Aurora.xex`, extracted
   `Aurora.exe`, `FtpDll.xex`, and `Nova.xex`.
   The current local `Aurora.xex` reference hash is
   `583BCD442D8017D6FCB2645B93CDA987F4C0A43A688B652D7364CCAEDAEEFA9F`;
   confirm the console copy independently before using it as an allowlist.
2. Use the existing `tools/jeff.exe` to capture XEX metadata and produce an
   analyzable PowerPC image. The local Rev1655 image is based at `0x82000000`
   and already has an extracted PE available under `original/`.
3. Create a Ghidra PowerPC big-endian project and import the extracted PE.
4. Map string references for `PluginManager`, `Module Loaded`, `FtpDll`,
   `Nova`, `ScnApplication`, `Sorting Game List`, `Filter Game List`, and
   `Swap Active List Event`.
5. Compare imports/exports and initialization flow in `FtpDll.xex` and
   `Nova.xex` to recover the actual Aurora module contract. A `Plugins`
   directory alone is not proof that arbitrary modules are loadable.

### Toolchain gate

Establish a reproducible PowerPC/XEX build using either a legally available
Xbox 360 XDK toolchain compatible with Aurora's libraries or a demonstrated
open toolchain. Document exact versions and commands; do not commit proprietary
SDK files.

### Minimal module

Build `AuroraAZ.xex`, with no companion manifest, asset, configuration, script,
database record, or boot-loader edit. Copy the same bytes to the lab as
`Plugins\NetDbgDll.xex` only after confirming that path is absent. The canary
does only five things:

1. validates Aurora's loader identity, image layout, and exact code probes;
2. writes a canary lifecycle/result message to the kernel debug channel;
3. makes no hooks or database writes;
4. cleanly unloads or becomes inert when incompatible.
5. exports valid ordinal-only NetDbg entries 2-5; all remain inert.

Test it only in `AuroraAZLab`. The M1 gate passes only if Aurora loads this
standalone file through the exact key-7 wrapper and NOVA independently proves
the canary thread is alive. A
DashLaunch plugin, `launch.ini` edit, patched executable, helper loader, or
companion manifest may be investigated to understand the platform but is not a
compliant fallback. If the gate fails, stop and present the evidence before
revisiting the one-file requirement.

### M1 hardware attempt and corrected retry

The first hardware attempt failed safely in `Hdd1:\AuroraAZLab\`. Its
round-tripped SHA-256 matched the uploaded build, the lab dashboard remained
usable, and the first two relevant `debug.log` lines were exactly:

```text
Failed to load game:\Plugins\NetDbgDll.xex
Failed to load NetDbgDll
```

NOVA reported no thread in the reserved `0x91D00000-0x91DFFFFF` window, so
the failure occurred before the canary entry point, module-handle recovery, or
export resolution. The lab file was renamed to a SHA-derived disabled filename
and the production `Hdd1:\Aurora\` tree, its plugin directory, and `launch.ini`
were untouched.

The failed XEX used system-DLL module flags `0xA`, lacked optional header
`0x10201` (Image Base Address), and carried a synthesized empty TLS header. The
current retry changes all three together:

- package as a title DLL with module flags `0x9`;
- emit `0x10201 = 0x91D00000`, matching both the PE and XEX security image
  base;
- omit optional header `0x20104` because AuroraAZ has no TLS data.

These are corrected compatibility candidates, not a proven diagnosis. Aurora's
private wrapper mode `9` is not known to mean XEX module flags `0x9`; stock
Nova loads with flags `0xA`. The working `FtpDll.xex` and Nova DLLs omit the
TLS header, while stock Aurora itself carries the same empty TLS tuple as the
failed image. The absent `0x10201` is the strongest isolated difference because
all three inspected working Rev1655 XEX images carry it, but only a hardware
retry can establish whether the corrected bundle loads.

Repeat the isolated upload, round-trip hash, lab launch, debug-log,
NOVA-thread, and rollback checks. M1 passes only after the corrected image
loads on hardware.

## M2 — Input bridge

Prefer hooking Aurora's `ScnApplication`/coverflow input path over globally
hooking `XInputGetState`; a global hook is more likely to steal input from
dialogs and launched games.

Develop in two steps:

1. **Observe only:** log R3, D-pad Left/Right, left-stick Left/Right, A, and RB,
   including the current scene/state. Do not consume anything.
2. **Selector ownership:** after an R3 edge on the coverflow, consume only the
   required navigation/A events until selection completes. All other states
   pass through unchanged.

The bridge needs edge detection, left-stick dead-zone/hysteresis, and controlled
repeat for held directions. It must ignore R3 outside the main coverflow and
must not alter RB behavior.

Initial test values are a 0.70 stick-engage threshold, 0.35 release threshold,
300 ms initial repeat delay, and 110 ms repeat interval. Lock each selector
session to the controller that pressed R3 and deduplicate simultaneous stick
and D-pad direction events. These values remain configurable until hardware
testing is complete.

Gate M2 with a log-backed controller test before any rendering code is added.

## M3 — Choose and prove the overlay

Run two bounded rendering spikes and keep only one:

### Candidate A: runtime XUI injection

Create elements under a process-owned/top-level scene using XUI runtime calls,
without loading resources from the selected skin. This is preferable if the
elements survive skin changes and can be removed cleanly.

### Candidate B: renderer-owned D3D9 overlay

Draw after the coverflow/skin and restore every touched D3D state. Use an
embedded Aurora A-Z-owned glyph atlas or other embedded redistributable font
resource rather than a skin font. This is preferable if XUI scene/resource
lookup is inseparable from the active skin.

### Overlay gate

The spike must show a rectangle and the complete alphabet in a 1280×720 virtual
coordinate system on Default, CleanNXE, and at least two structurally different
third-party skins. It must:

- remain centered and inside title-safe bounds;
- remain above the active skin and live coverflow;
- hide whenever the main coverflow is not the active scene;
- survive changing skins without reinstalling;
- add no visible frame pacing regression or leaked resources.

Choose based on hardware evidence, not implementation preference.

## M4 — Filter bridge and latency gate

The production bridge should construct or select the built-in name-filter
predicate in memory and refresh the active coverflow without persisting A-Z
QuickViews. Reverse engineering should begin from cross-references to the three
GameListManager log strings and from the native QuickView apply path.

Prove these separately:

1. apply `NameFilter.Other` and every `NameFilter.*.<letter>` correctly;
2. preserve the currently selected sort and sort direction;
3. avoid changing the user's QuickView database or RB menu;
4. update the visible list and counter safely for zero or many matches;
5. return to the main coverflow without restarting Aurora.

Proposed latency budget on the 2,241-title console:

- highlight movement: visible within 50 ms;
- A press to filter completion: median under 1 second;
- no new `Sorting Game List` event for letter-only changes.

If Aurora's public/internal path always forces the roughly five-second re-sort,
stop at M4 and get an explicit product decision: accept that delay with a busy
state, or continue into a riskier cache/list-swap implementation.

## M5 — Integrate the required state machine

Required transitions:

```text
Coverflow --R3--> Selector(#)
Selector --Left/Right--> Selector(previous/next letter)
Selector --A--> Apply filter --> Coverflow
```

While `Selector` is active, letter navigation must never move the coverflow.
Filter application must be idempotent and resilient to an empty result.

Resolve these product decisions before freezing the state machine:

- whether `#` and `Z` wrap or clamp;
- whether B, R3, or both cancel without applying;
- how the user clears a letter filter without adding an `ALL` item to the
  mockup row;
- whether a letter replaces the active QuickView filter or combines with it;
- whether the row is always visible or only emphasized in selector mode.

## M6 — Match the mockup

Use the mockup as a measured visual reference, not a general inspiration:

- one unbroken `# A B ... Z` row;
- horizontally centered in the 1280×720 virtual viewport;
- fitted within approximately 91% of the title-safe width, with a reference
  text baseline near logical Y=558 (about 77.5% down the viewport);
- white/light-gray, lightweight 32–36 px sans-serif glyphs with even optical
  spacing;
- a clearly distinguishable selected letter without rearranging the row;
- non-selected glyphs only slightly dimmed while active, with the selected
  glyph at full white and a restrained glow or underline;
- a soft black shadow offset by roughly 2–3 logical pixels, added after the
  interaction is stable;
- no opaque popup, list panel, or replacement QuickView scene.

Capture hardware screenshots and compare alignment on every test skin. Minor
theme overlap is handled by the extension's own contrast treatment, never by
editing the skin.

## M7 — Hardening, packaging, and release

### Compatibility matrix

- skins: Default, CleanNXE, Dark, Series, plus one additional third-party skin;
- video modes used by the console, with 720p as the reference;
- cold boot, warm restart, skin change, profile sign-in/out, opening/closing
  QuickView, launching a title, and returning to Aurora;
- D-pad taps/holds, left-stick taps/holds, R3 bounce, rapid A, and empty-result
  filters;
- wired/wireless controllers in ports 1–4, controller disconnect/reconnect,
  and a second controller attempting input during an owned selector session;
- at least 100 selector open/navigate/apply/cancel cycles and a 30-minute idle
  run while the overlay is present.

### Release safety

- exact executable-hash allowlist and hook-site signature checks;
- fail-closed notification/log on unsupported Aurora builds;
- no writes under `Skins` and no on-disk `Aurora.xex` changes;
- one documented `Plugins\NetDbgDll.xex` removal/rename path that disables the module;
- backup, recovery, uninstall, and crash-log instructions;
- a release archive containing only `AuroraAZ.xex`, plus a published SHA-256
  checksum and documentation outside the installed payload;
- before/after hashes proving skins are unchanged.

## Immediate next work session

1. Build the corrected inert canary in CI with the pinned, cached OpenXeChain
   toolchain and patched SynthXEX.
2. Require the strict validator and `jeff.exe` inspection to show module flags
   `0x9`, load address `0x91D00000`, optional header `0x10201`, no empty TLS
   header, and ordinal-only exports 2-5.
3. Upload only that verified image to the isolated lab, download it again, and
   require an exact SHA-256 match before launch.
4. Launch the lab through NOVA, inspect `debug.log`, and require a live canary
   thread in `0x91D00000-0x91DFFFFF`.
5. Return to production, disable the lab file by recoverable rename, relaunch
   the lab once, and prove that the canary thread disappears.

No input hook, overlay, or production deployment begins until the corrected
no-op module passes M1 in the isolated lab copy.
