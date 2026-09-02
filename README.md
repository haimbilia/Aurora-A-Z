# Aurora A-Z

Aurora A-Z is a skin-agnostic, on-coverflow alphabetical selector for the
Aurora dashboard on Xbox 360. The target UI is a persistent `# A B ... Z` row
above the game title, with controller navigation and selection directly from
the main coverflow. It must work without modifying or replacing the selected
Aurora skin.

The production artifact is strictly one self-contained file,
`AuroraAZ.xex`. All runtime resources are embedded. Installation must not
require Lua scripts, QuickViews, database records, loose assets, skin files,
changes to `Aurora.xex`, or changes to `launch.ini`.

## Required interaction

The normative controller and filtering behavior is defined in
[`REQUIREMENTS.md`](REQUIREMENTS.md). In short: R3 enters the on-coverflow
selector at `#`; D-pad Left/Right and left-stick Left/Right move the highlight;
A filters the coverflow by the highlighted initial and returns control to the
coverflow. RB must retain Aurora's normal QuickView menu.

The architectural constraints that follow from these requirements are recorded
in [`ARCHITECTURE.md`](ARCHITECTURE.md). The gated engineering roadmap is in
[`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md).

## Current status

There is no compliant functional release yet. The native C99 selector core is
now implemented and host-tested: it models the R3/Left/Right/A state machine,
maps `#` and `A` through `Z` to Aurora's built-in name filters, carries the
mockup's measured 1280x720 layout, and rejects binaries that do not match the
exact Rev1655 code probes. The first Xbox build is an inert compatibility
canary; input hooks, overlay drawing, and filter mutation remain gated hardware
work.

Offline analysis also resolved the loader question: Rev1655 constructs exactly
seven hard-coded module wrappers and does not enumerate arbitrary files under
`Plugins`. Consequently, copying `AuroraAZ.xex` into that directory cannot pass
the strict one-file discovery requirement. A DashLaunch system-plugin slot is
the cleanest technically viable bootstrap found so far, but it requires a
one-time `launch.ini` entry and is therefore a product decision rather than a
silent fallback. See `reference/NATIVE_LOADER.md`.

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
its own selector overlay above the active skin, and bridge A-button selection
to Aurora's coverflow filtering. It must not write to `Skins`. Aurora's native
module loader and the local XEX toolchain must be proven before the first
canary is installed; failure of either gate is reported rather than worked
around with extra installed files.

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
native/                  Host-tested selector core and version-gated canary
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
