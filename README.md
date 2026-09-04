# Aurora A-Z

Aurora A-Z is a skin-agnostic, on-coverflow alphabetical selector for the
Aurora dashboard on Xbox 360. The target UI is a transient `# A B ... Z` row
above the game title, shown only while R3 is held, with controller navigation
and selection directly from the main coverflow. It works without modifying or
replacing the selected Aurora skin.

The production artifact is strictly one self-contained file,
`AuroraAZ.xex`. All runtime resources are embedded. Installation must not
require Lua scripts, QuickViews, database records, loose assets, skin files,
changes to `Aurora.xex`, or changes to `launch.ini`.

## Required interaction

The normative controller and filtering behavior is defined in
[`REQUIREMENTS.md`](REQUIREMENTS.md). In short: hold R3 to reveal the
on-coverflow selector at `#`; while holding it, D-pad Left/Right and left-stick
Left/Right move the highlight; release R3 to filter and hide the row. Aurora
A-Z never consumes A, and RB retains Aurora's normal QuickView menu. A quick
R3 press/release without moving the highlight cancels without filtering.

The architectural constraints that follow from these requirements are recorded
in [`ARCHITECTURE.md`](ARCHITECTURE.md). The gated engineering roadmap is in
[`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md).

## Current status

There is no compliant filtering release yet. The native C99 selector core is
implemented and host-tested: it models the hold-R3/Left/Right/release state machine, maps
`#` and `A` through `Z` to Aurora's built-in name filters, carries the mockup's
measured 1280x720 layout, and rejects binaries that do not match the exact
Rev1655 code probes. M1, the one-file native bootstrap and worker-entry gate, is
complete on hardware. M2a is also complete on hardware: the direct observe-only
input hook saw every required control, including stick holds/repeats, while
recording zero invalid events, drops, consumed keys, or filter requests. M3's
renderer-owned overlay and selector interaction passed on hardware with commit
`c86ad0c`, GitHub Actions run `33747222158`, and artifact SHA-256
`94F32460DBC5A76153F63BB9B23158E7CFE690277D792DBB7822666B93B09CF8`.
The complete row rendered without diagonal clipping; R3, D-pad Left/Right,
left-stick Left/Right, and RB behaved as required. The filter bridge remains
hardware-gated; A is no longer part of the Aurora A-Z interaction.

The hold-R3 filtering interaction now works on hardware, including cancel on
an unmoved R3 tap and completion-based re-arming. Commit `57dd888` is not a
release candidate: an isolated A/B test proved that its still-running worker
blocked normal title handoff. Hardware then disproved ordinal 3 as a launch
notification: the shutdown-capable follow-up still black-screened and its
persisted marker recorded zero shutdown requests. The current lab candidate
instead intercepts the exact Rev1655 `ContentLauncher` entry, signals its
worker to restore all four hooks and exit, then immediately resumes
Aurora's original launcher. It must pass game/XEX launch testing before any
production install.

Offline analysis resolved the static loader contract: Rev1655 constructs
exactly seven hard-coded module wrappers and does not enumerate arbitrary
files under `Plugins`. Its optional Network Debugger wrapper requests the
literal `game:\Plugins\NetDbgDll.xex` path and resolves ordinals 2-5. The
release binary remains `AuroraAZ.xex`; the candidate one-file installation
copies those same bytes under that literal filename. It requires no
DashLaunch slot, `launch.ini` change, skin change, or companion file, and it
is valid only when no real `NetDbgDll.xex` is installed.

The first hardware loader canary was rejected safely in the isolated lab:

```text
Failed to load game:\Plugins\NetDbgDll.xex
Failed to load NetDbgDll
```

The corrected `C51E3A...` retry then established the module-container and
ordinal-resolution path, but its original entry-point observation was
inconclusive. That result is retained as historical evidence, not the current
gate status.

M1 subsequently passed with commit `39b551c`, GitHub Actions run
`33604028771`, and artifact SHA-256
`87894F41A89F4F3CAAFA8A1864AB8F8A91A2ED011882EEEF36E4D3FAEF58596C`.
The FTP round-trip hash matched. That canary validates and calls Aurora
Rev1655's complete thread wrapper at `0x82361AA8`, after validating the
Xapi-thread-startup probe at `0x82804650`. The primary `AZM1` record reported
`source_ordinal=4`, `phase=5` (`COMPLETE`), `state=2` (`RUNNING`), and zero
create/resume statuses; the separate worker record reported `phase=7`
(`WORKER_ENTERED`). Together they prove that Aurora invoked ordinal 4
automatically and that the wrapper entered AuroraAZ-owned worker code. See
[`reference/NETDBG_BOOTSTRAP.md`](reference/NETDBG_BOOTSTRAP.md) and
[`reference/NATIVE_LOADER.md`](reference/NATIVE_LOADER.md).

Functional test r3 is retained only as a hardware research build. It requires a
custom skin and its attempted D-pad Down remap does not work: the coverflow
consumes Down, while RB opens Aurora's separate QuickView menu. That build is
not the requested selector and is not part of the target architecture.

The earlier filter-only v0.1.0 prerelease is deprecated because Aurora
already contains a stock name filter and it does not implement the project
interaction.

The r3/r4 skin builders were deleted on 2026-09-02. They patched `Aurora_Main`
and repurposed the `QuickViewRB` control, which **broke the RB button on
hardware**, and rule 1 of `IMPLEMENTATION_PLAN.md` forbids shipping a patched
skin. Their findings survive in `HANDOFF.md`; the one piece worth keeping is in
`research/`.

## Where to start

Read in this order:

1. [`REQUIREMENTS.md`](REQUIREMENTS.md) — what the selector must do
2. [`ARCHITECTURE.md`](ARCHITECTURE.md) — why it must be a native runtime extension
3. [`HANDOFF.md`](HANDOFF.md) — **verified facts about Aurora**, measured or dumped from
   the console. Read before designing anything; most of it was expensive to learn
4. [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md) — the gated roadmap

## Target development workflow

The production implementation must be a version-gated runtime extension for
Aurora 0.7b.2 Rev1655. It will own controller-state handling, render or inject
its own selector overlay above the active skin, and bridge an owned R3 release
to Aurora's coverflow filtering. It must not write to `Skins`. The XEX
toolchain and Network Debugger bootstrap are operational, and M1 is complete.
The hardware-passing M2a artifact is commit `06affc4`, GitHub Actions run
`33736960588`, SHA-256
`431FAD613E1C177B5B5A486B5B21B98AB17BB2AC2592C1A6F630DC07E68EB86E`.
Its final telemetry recorded all seven observed controls, the recovered main caller,
and clean safety fields. The current overlay canary adds only the centered
transient row; filter mutation remains behind its independent hardware gate.

## Project layout

```text
REQUIREMENTS.md          Product contract
ARCHITECTURE.md          Architecture decision
IMPLEMENTATION_PLAN.md   Gated roadmap, M0-M7
HANDOFF.md               Verified facts about Aurora, with evidence

scripts/xzp.ps1          XZP extract/build utility
scripts/build-openxechain.sh
                         Native Xbox cross-build entry point
scripts/capture-nova.ps1 Repeatable hardware screenshots through NOVA
native/                  Host-tested core, proven M1/M2a, M3 overlay canary
source/utility/          On-console Lua: QuickView installer, API dump
research/                Rejected skin route, kept as evidence only

original/                Local Aurora binaries and skins; ignored by Git
tools/                   XUIHelper, jeff.exe, Aurora dev docs; ignored by Git
reference/               External research notes
```

Local-only inputs and tools are excluded from Git, so the project never commits
Aurora's binaries or stock skin assets.

Third-party tooling in use: [XUIHelper](https://github.com/SGCSam/XUIHelper) for
XUR/XUI conversion, XboxUnity's `AuroraElements.xml` extension definitions, and
`tools/jeff.exe` for XEX metadata and PowerPC image extraction.

## Safety

Production builds must not modify skin packages or the on-disk `Aurora.xex`.
Any native integration must verify the exact Aurora revision before applying
in-memory hooks and must fail closed on unsupported builds. Database changes
must remain transactional and reversible. Keep FTP access available during
early hardware tests.

Experimental XEX builds are tested only from a separately launchable
`Hdd1:\AuroraAZLab\` copy. The normal boot path must remain on the known-good
`Hdd1:\Aurora\Aurora.xex`; no development canary is copied into the production
plugin directory. See `reference/NETDBG_BOOTSTRAP.md` for the hash, upload,
NOVA verification, and recoverable rollback gates.
