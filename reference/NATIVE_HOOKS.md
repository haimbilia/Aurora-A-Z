# Rev1655 native hook map

Date: 2026-09-02

## Result

Offline analysis found actionable Rev1655 hook candidates for controller input
and skin-independent rendering. It also found Aurora's complete in-memory
filter/sort/swap pipeline. The input and render sites are suitable for a
fail-closed hardware canary. The filter pipeline is not yet safe to call from a
release build: its functions and call order are known, but the ownership,
threading, and transient work-object ABI still need to be confirmed on hardware.

Nothing in this hook analysis was uploaded to or changed on the console.
Loading is handled separately by the exact key-7 compatibility contract in
`reference/NETDBG_BOOTSTRAP.md`; none of these hooks may be enabled until its
ordinal-2 code-execution canary produces the expected resident-thread signal.

## Exact binary scope

Every address in this file applies only to this image:

| Artifact | SHA-256 |
| --- | --- |
| `original/Aurora.xex` | `583BCD442D8017D6FCB2645B93CDA987F4C0A43A688B652D7364CCAEDAEEFA9F` |
| extracted `original/Aurora.exe` | `5BB5BAF8DF4CCB197241B34935EB400F36C8C20648CC074E2C30FA80ADD37E3C` |

The XEX is Aurora 0.7b.2 Rev1655, based at `0x82000000`, with entry point
`0x828050E0`. The extracted PE's `.text` begins at `0x82210000` and is
PowerPC 32-bit big-endian code.

A plugin must check the supported image identity and every hook's expected
instructions before writing any branch. A mismatch disables Aurora A-Z without
patching memory. A future Aurora build must be rediscovered semantically; it
must not be accepted by merely making these byte signatures more permissive.

## Evidence labels

- **Proven** means supported by an import relocation, literal diagnostic/class
  string, direct control flow, or an observed field access in this exact image.
- **Strong inference** means the role follows from the surrounding control flow
  but Aurora did not expose a symbol for it.
- **Hypothesis** means it is a candidate implementation choice that requires a
  hardware probe before it can become a release dependency.

Function boundaries were recovered from the big-endian `.pdata` records, then
the functions were decoded with Capstone in PowerPC big-endian mode. Literal
string xrefs, import thunks, direct branches, and callers were cross-checked.

## Controller input

### Keystroke wrapper

**Proven:** `0x82801D90..0x82801DD0` is Aurora's only direct wrapper around the
`XamInputGetKeystrokeEx` import thunk at `0x82B6624C`.

```text
82801D90  mflr    r12
82801D94  stw     r12,-8(r1)
82801D98  stwu    r1,-0x60(r1)
82801D9C  stw     r3,0x74(r1)
82801DA0  cmplwi  cr6,r3,0xff
82801DA4  bne     0x82801DB4
82801DA8  li      r11,0
82801DAC  oris    r4,r4,0x4000
82801DB0  stw     r11,0x74(r1)
82801DB4  addi    r3,r1,0x74
82801DB8  bl      0x82B6624C  ; XamInputGetKeystrokeEx
```

The wrapper has two direct callers:

| Call site | Containing function | Observed role |
| --- | --- | --- |
| `0x822113F4` | `0x82211348..0x822114A8` | per-frame input/update dispatch |
| `0x82211ED0` | `0x82211EB8..0x82211EF8` | one-keystroke drain/forward helper |

**Proven:** the main caller requests any user and any key, places the keystroke
at stack `+0x58`, then walks a registered-object list. It invokes vtable slot
zero on each live object with the keystroke pointer before forwarding into the
application/XUI object:

```text
822113D4  addi    r5,r1,0x58
822113E0  li      r4,0xff
822113E4  li      r3,0xff
822113F4  bl      0x82801D90
...
82211418  lwz     r11,0(r3)
8221141C  addi    r6,r1,0x58
82211420  mr      r4,r29
82211424  fmr     f1,f31
82211428  lwz     r11,0(r11)
8221142C  mtctr   r11
82211430  bctrl
```

The main caller does not branch on the wrapper's return value before dispatch.
That makes the keystroke buffer itself the practical consumption point.

### Recommended input canary

**Hypothesis, high confidence:** detour `0x82801D90`, call the original, and
inspect the caller-provided `XINPUT_KEYSTROKE` only when the original reports a
key. Keep the original return code. When Aurora A-Z consumes a key, clear the
eight-byte keystroke structure before returning so Aurora's normal callback
chain sees no key. This should stop selector Left/Right and A from also moving
or launching the coverflow.

The first build should only log keys. It must confirm this SDK-compatible
layout on hardware before consumption is enabled:

```cpp
struct XINPUT_KEYSTROKE {
    uint16_t VirtualKey;  // +0x00
    uint16_t Unicode;     // +0x02
    uint16_t Flags;       // +0x04
    uint8_t  UserIndex;   // +0x06
    uint8_t  HidCode;     // +0x07
};
```

Conventional XDK virtual-key values are listed below as probe expectations,
not as facts recovered from this binary:

| Control | Expected virtual key | Confidence |
| --- | ---: | --- |
| A | `0x5800` | high |
| D-pad Left | `0x5812` | high |
| D-pad Right | `0x5813` | high |
| R3/right thumb click | `0x5817` | high |
| left-stick Left/Right | `0x5823` / `0x5822` | must be logged and verified |

The canary must also record `Flags` so press, release, and repeat behavior can
be distinguished. Do not implement a second repeat timer until the native
repeat events are observed.

### Coverflow scope

Input must not be intercepted globally. Cache the `GameContentManager*` and a
monotonic "last successful RenderMenu" marker from the render hook below. R3
may enter selector mode only when `RenderMenu` returned `0` in the immediately
preceding frame. Left/Right and A are consumed only while selector mode is
active. RB is never consumed.

**Limitation:** successful coverflow rendering may continue behind an Aurora
modal scene. Therefore "recent RenderMenu" is an initial scope gate, not yet a
proof that no popup is active. Hardware tests must cover QuickView, Details,
Settings, the Guide, and message boxes. Add an active-scene/UI-state check if
any of those still pass the render gate.

## Skin-independent drawing

### Coverflow-specific render hook

**Proven:** `0x82358A08..0x82358B80` is
`GameContentManager::RenderMenu`. Literal diagnostics contain that exact class
and method name. The function checks three graphics resources and an enable
byte, uses the global D3D device, draws the coverflow, resolves it, and restores
the prior targets.

Observed fields and globals:

| Location | Proven use |
| --- | --- |
| `GameContentManager + 0x60` | coverflow renderer object passed to `0x823493D8` |
| `+0x2224` | required render resource / resolve source |
| `+0x222C` | required render resource, bound during the pass |
| `+0x2234` | required render target/surface |
| `+0x225D` | render-enabled byte |
| `+0x2260` | coverflow-draw gate used with item count |
| `0x82BC6BD8` | global `IDirect3DDevice9*` |

The key path is:

```text
82358A48  lbz     r11,0x225D(r3)
82358A54  lis     r11,0x82BC
82358A58  lwz     r31,0x6BD8(r11)    ; IDirect3DDevice9*
...
82358B0C  mr      r4,r31
82358B10  fmr     f1,f30
82358B14  addi    r3,r30,0x60
82358B18  bl      0x823493D8         ; coverflow draw
...
82358B58  mr      r5,r29
82358B60  mr      r3,r31
82358B64  bl      0x82778170         ; restore previous target
82358B68  mr      r4,r28
82358B6C  mr      r3,r31
82358B70  bl      0x82777E98         ; restore previous surface
82358B74  li      r3,0
```

**Recommended first hook:** detour the function entry, invoke the original, and
draw the alphabet only after the original returns `0`. The hook receives the
`GameContentManager*` in `r3`; cache it instead of hardcoding a guessed
singleton. Drawing after the original means Aurora has restored its previous
targets and the row should sit above the coverflow texture without changing a
skin package.

This layering is a **strong hypothesis**, not yet proven on hardware. The first
render canary should draw a small translucent rectangle at the intended row
position and verify it on Default and CleanNXE before font work is added.
Save and restore every D3D state, shader, texture, vertex declaration, viewport,
scissor, blend state, sampler state, and target touched by the plugin.

For fidelity to the mockup, the production renderer should use embedded
geometry and an embedded glyph atlas for `# A B ... Z`; no skin font, XUR,
element ID, or external asset is allowed. Center the complete row at 1280x720,
derive coordinates from the current viewport for other modes, use a subtle
dark offset pass for the shadow, and draw the selected glyph with a brighter
fill plus a compact translucent highlight.

### Last pre-present fallback

**Strong inference:** `0x822114A8..0x82211778` is the application render/present
loop. It invokes registered render phases, then reaches:

```text
8221162C  lwz     r3,0x80(r31)
82211630  bl      0x827737F8
82211634  lwz     r11,0xBC(r31)
82211638  li      r5,0
8221163C  lwz     r3,0x80(r31)
...
8221164C  bl      0x82773800
```

`0x82773800` is the D3D present/swap implementation; it calls the `VdSwap`
import at `0x82773B88`. The point immediately before `0x8221162C` is after the
observed registered render phases and before present.

If later XUI/skin rendering covers the `RenderMenu`-return overlay, an interior
hook before `0x8221162C` is the fallback. It is less desirable because it is a
global, invasive call-site patch and must use the coverflow visibility marker
to avoid drawing over other scenes. Hooking `VdSwap` itself is not recommended:
it has a clean frame boundary but insufficient Aurora scene and render-state
context.

## In-memory filtering

### GameListManager and filter expression

**Proven:** `0x8223FF30..0x8223FF88` is a singleton accessor. It constructs the
static object at `0x82BC999C` through `0x8235B818` and returns it. Literal log
xrefs in the surrounding methods identify the object as `GameListManager`.

**Proven:** the following functions form Aurora's list pipeline:

| Address | Observed behavior | Confidence |
| --- | --- | --- |
| `0x8235BDD8..0x8235C404` | constructs the composite filter expression and assigns it to manager `+0x58` | proven |
| `0x8235CC38..0x8235CD40` | logs `Sorting Game List` and sorts the active source list | proven |
| `0x8235C938..0x8235CBE0` | logs `Filter Game List`, resolves manager `+0x58`, evaluates content, populates output vectors, returns filtered count | proven |
| `0x82355508..0x823559F8` | swaps the resulting active list into `GameContentManager`/coverflow | proven |

`0x8235BDD8` receives the manager in `r3`, two scalar flags in `r4/r5`, two
string-like objects in `r6/r7`, and a vector of 0x1C-byte string-like objects in
`r8`. It builds a composite expression rather than storing only one filter
name. This is important: assigning only `NameFilter.*` directly to manager
`+0x58` could discard the current platform/source/filter constraints.

`0x8235C938` takes the manager in `r3` and an optional output-index pointer in
`r4`; it returns the size of the filtered vector. It calls the filter registry
singleton at `0x82271000`, resolves the expression through `0x823B6600`, and
iterates the current content pointers. This path is wholly in memory.

### QuickView translation and canonical asynchronous path

**Proven:** `0x823567B0..0x82356B2C` translates Aurora's current
`CQuickViewSettingObject` into a transient work structure. The object fields
used by this routine include:

| QuickView offset | Observed meaning |
| --- | --- |
| `+0x00` | integer QuickView ID |
| `+0x04` | display-name string |
| `+0x3C` | sort-method string |
| `+0x58` | raw database filter grammar |
| `+0x74` | flags bitfield |
| `+0xA0` | compiled Lua filter expression |

It validates the filter method through the registry (`0x82271000` then
`0x82324C60`) before returning. Its two direct callers are `0x82357718` in
`0x82357660` and `0x82359A14` in `0x82359998`.

**Proven call order:** `0x82356588..0x823566B0` performs the canonical worker
sequence:

```text
823565FC  bl      0x8223FF30       ; GameListManager
82356614  bl      0x8235BDD8       ; build/set composite filter
82356634  bl      0x8223FF30
82356640  bl      0x8235CC38       ; sort
82356648  bl      0x8223FF30
82356650  bl      0x8235C938       ; filter
82356664  stw     r11,0x11C(r30)   ; save output index/count-like result
...
82356684  li      r11,0x102
82356690  bl      0x8235A388       ; obtain event queue entry
823566A0  bl      0x82802F80       ; signal/submit event
```

The event payload is later consumed by the active-list swap path. The event ID
is `0x102`. The ownership-safe A-Z path, including the active `0xD0` snapshot,
`Work+0x68` additive vector, and scheduler flag `0x08`, is now documented in
[`FILTER_IMPLEMENTATION.md`](FILTER_IMPLEMENTATION.md).

**Strong inference:** `0x823566D8..0x823567A4` is the public apply/dispatch
layer above that worker. With `r8 == 1`, it calls `0x82343628` on
`GameContentManager + 8`, which appears to schedule asynchronous work. With
`r8 != 1`, it updates filter state synchronously but does not run the complete
sort/filter/swap sequence. The exact enum/boolean meanings of `r6`, `r7`, and
`r8`, and the ownership of its `r4/r5` objects, are not yet proven.

Aurora already registers the required filters:

```text
NameFilter.Other
NameFilter.A - F.A  ... NameFilter.A - F.F
NameFilter.G - L.G  ... NameFilter.G - L.L
NameFilter.M - R.M  ... NameFilter.M - R.R
NameFilter.S - X.S  ... NameFilter.S - X.X
NameFilter.Y - Z.Y  NameFilter.Y - Z.Z
```

`#` maps to `NameFilter.Other`. These identifiers are exact and do not require
custom Lua filters or QuickView database rows.

### Safe implementation sequence

The following order keeps the console recoverable and separates ABI discovery
from state mutation:

1. Add a read-only filter canary that calls the registry validation path for
   one embedded name such as `NameFilter.A - F.A`. Log only success/failure.
2. Capture a real transient work object produced by `0x823567B0` and log its
   first `0x74` bytes plus the short-string/heap cases for the filter and sort
   members. Do not retain pointers after the host destroys the object.
3. Confirm which thread invokes `0x82356588` and which thread owns
   `GameContentManager`. Never run the 5-7 second sort from the render or input
   hook.
4. Build a transient work object using Aurora's own constructors/assignment
   helpers, preserve its current sort and non-name filter components, replace
   only the name component, and submit it through `0x823566D8`'s asynchronous
   branch.
5. Let Aurora's event `0x102` perform the normal active-list swap. Do not call
   `0x82355508` with a hand-built payload until its allocator and destructor
   contract are proven.
6. After A is accepted, leave selector mode immediately but show a small busy
   state until the canonical swap event completes. Input must remain usable if
   a zero-result filter is valid.

The tempting shortcut—writing a string into `GameListManager + 0x58`, calling
`0x8235C938` on the render thread, and manually replacing coverflow vectors—is
not release-safe. It risks losing the current composite constraints, racing the
worker, violating small-string ownership, and corrupting list/event ownership.

## Expected instruction signatures

These are exact bytes from the supported extracted PE. All listed 16-byte
windows are unique in that file except the two marked with longer required
windows. Matching remains secondary to the SHA-256 allowlist.

| Purpose | VA | Required bytes |
| --- | ---: | --- |
| input wrapper | `0x82801D90` | `7D 88 02 A6 91 81 FF F8 94 21 FF A0 90 61 00 74 2B 03 00 FF` (20 bytes; unique) |
| per-frame input call context | `0x822113D8` | `FC 00 06 9C C1 BF 00 DC 38 80 00 FF 38 60 00 FF` |
| application render loop | `0x822114A8` | `7D 88 02 A6 48 75 68 21 DB E1 FF D8 94 21 FF 40` |
| last-render-phase iterator point | `0x82211618` | `48 17 21 91 81 5F 00 98 81 61 00 50 7F 0B 50 40` |
| `GameContentManager::RenderMenu` | `0x82358A08` | `7D 88 02 A6 48 60 F2 BD DB C1 FF C8 DB E1 FF D0` |
| `RenderMenu` restore tail | `0x82358B58` | `7F A5 EB 78 38 80 00 00 7F E3 FB 78 48 41 F6 0D` |
| GameListManager accessor | `0x8223FF30` | `7D 88 02 A6 91 81 FF F8 FB E1 FF F0 3B E1 FF A0 94 21 FF A0 3D 40 82 BD 81 6A 99 98 55 69 07 FF` (32 bytes; unique) |
| asynchronous filter worker | `0x82356588` | `7D 88 02 A6 48 61 17 29 3B E1 FF 30 94 21 FF 30` |
| apply/dispatch layer | `0x823566D8` | `7D 88 02 A6 48 61 15 E9 94 21 FF 80 7C 7D 1B 78` |
| QuickView-to-work translator | `0x823567B0` | `7D 88 02 A6 48 61 14 FD 3B E1 FE C0 94 21 FE C0` |
| filter expression builder | `0x8235BDD8` | `7D 88 02 A6 48 60 BE D5 3B E1 FB 30 94 21 FB 30` |
| in-memory filter pass | `0x8235C938` | `7D 88 02 A6 48 60 B3 6D 3B E1 FF 20 94 21 FF 20` |
| sort pass | `0x8235CC38` | `7D 88 02 A6 48 60 B0 7D 3B E1 FF 40 94 21 FF 40` |
| active-list swap | `0x82355508` | `7D 88 02 A6 48 61 27 A1 3B E1 FF 30 94 21 FF 30` |

The second instruction in many function prologues is a branch to Aurora's
shared frame/exception helper. Its displacement is expected to change in a
different build. Do not wildcard it to claim compatibility; use it as part of
the exact Rev1655 guard.

## Candidate ranking

| Need | Preferred site | Fallback | Status |
| --- | --- | --- | --- |
| observe/consume R3, Left/Right, A | `0x82801D90` wrapper detour | post-call interception around `0x822113F4` | ready for logging canary |
| coverflow-scoped overlay | mark visibility at `0x82358A08`, draw inside the final ATG Font bracket at `0x8247E390` | global pre-present point before `0x8221162C` | exact renderer ABI mapped; hardware pixel canary required |
| locate D3D device | `*(IDirect3DDevice9**)0x82BC6BD8` while `RenderMenu` succeeds | device passed through render loop | proven for this build |
| resolve built-in name | registry `0x82271000` / lookup `0x82324C60` | none | ready for read-only canary |
| apply without DB writes | clone active aggregate, mutate `Work+0x68`, schedule `0x82343628` with `0x08` | no release-safe fallback | ownership ABI proven; runtime queue/interleave canary required |

## Release gates created by this analysis

- No hook is installed unless both binary identity and every site signature
  pass.
- Any trampoline must relocate whole PowerPC instructions, preserve LR/CTR/CR,
  volatile GPRs/FPRs required by the ABI, flush/invalidate caches, and restore
  original instructions on unload.
- The render canary must survive repeated skin changes, scene transitions,
  suspend/resume, and 1280x720 screenshot capture without state leakage.
- The input canary must prove exact virtual keys and that consumed input does
  not reach coverflow or QuickView handling.
- The filter canary must prove work-object construction/destruction and worker
  affinity before it may mutate or swap a list.
- No implementation may create QuickViews, touch `settings.db`, modify an XZP,
  or write to `Skins`.
