# Aurora Rev1655 scene gate

## Result

AuroraAZ can gate its selector on the stock main scene without changing a skin.
For the exact Aurora 0.7b.2 Rev1655 image, the useful predicate is:

> the cached `Aurora_Main.xur` scene has a live handle and
> `XuiElementHasFocus(mainScene)` returns true.

This distinguishes the coverflow (including a native AuroraAZ overlay drawn on
top of it) from Aurora scenes reached through forward navigation. QuickView,
Game Options/Details, and the utility popup all use that navigation path in the
Rev1655 executable. The implementation is read-only and fail-closed.

The implementation is isolated in:

- `native/include/auroraaz/scene_gate_rev1655.h`
- `native/src/scene_gate_rev1655.c`
- `native/tests/scene_gate_rev1655_tests.c`

It is not permission to enable input consumption. Hardware observations in the
matrix below remain required before `CONSUME` is eligible.

## Exact evidence image

| Artifact | SHA-256 |
|---|---|
| `original/Aurora.exe` | `5BB5BAF8DF4CCB197241B34935EB400F36C8C20648CC074E2C30FA80ADD37E3C` |
| `original/Aurora.xex` | `583BCD442D8017D6FCB2645B93CDA987F4C0A43A688B652D7364CCAEDAEEFA9F` |

Run `python scripts/verify-scene-gate-rev1655.py` from the repository root. It
checks both hashes and all seven runtime signature spans against the PE's actual
virtual-address mapping. Configuration also requires the existing exact-image
verifier to have succeeded; signature checks are an additional guard, not a
replacement for the image hash.

## Recovered structures and APIs

### Application and scene cache

| Address/offset | Static evidence |
|---|---|
| `0x82212160` | Application-manager singleton accessor. Its return sequence at `0x82212194` materializes `0x82BBFFF8`. |
| `0x82BBFFF8` | Singleton object. The Rev1655 constructor installs vtable `0x8211F980`. The probe rejects any other vtable. |
| manager `+0x4C` | Scene-cache object. Aurora's scene-navigation wrappers obtain the singleton, add `0x4C`, then call cache lookup `0x82225520`. |
| cache `+0x28` | Singly linked cache head, therefore fixed address `0x82BC006C`. |
| node `+0x00` | Pointer to a big-endian UTF-16 scene locator/path. |
| node `+0x04` | cached `HXUIOBJ` scene handle. |
| node `+0x08` | acquired/in-use flag. Cache lookup requires zero and writes one when returning a scene. |
| node `+0x0C` | next node. |
| `0x82225520` | Cache lookup. The block `0x82225540..0x8222559C` proves every node offset above. |
| `0x829600B8` | Case-insensitive wide-string comparison used by cache lookup; `lhz` and two-byte pointer increments prove the path encoding. |

The logical main-scene name is the big-endian UTF-16 literal
`Aurora_Main.xur` at `0x821208B8`. The cache stores a skin-dependent locator,
for example the observed `memory://...#Aurora_Main.xur`. The gate consequently
matches only the logical basename after `/`, `\`, `:`, or `#` and does not encode a
skin package, mount, media ID, or memory locator.

The walk is bounded to 256 nodes and each path to 512 UTF-16 code units. A
cycle, limit hit, unreadable pointer, unterminated path, duplicate main-scene
basename, or changing snapshot denies capture.

### XUI predicates

| Address | Recovered behavior |
|---|---|
| `0x8280F448` | `XuiHandleIsValid(HXUIOBJ) -> BOOL`. It checks the handle-table index, live bit, generation, and object pointer without writing target state. |
| `0x82821978` | `XuiElementHasFocus(HXUIOBJ) -> BOOL`. Aurora itself calls this address with native scene/control handles at `this+4`. |
| `0x8280FA78` | Handle-to-object conversion used by the focus function. |
| `0x8281F078` | `XuiElementGetParent`, used by the ancestor walk. |
| `0x8281F7B0` | Ancestor test used by `XuiElementHasFocus`. It repeatedly gets the focused object's parent and compares it with the candidate object. |
| `0x82824FA0` | `XuiSceneNavigateFirst`. |
| `0x828250E0` | `XuiSceneNavigateForward`. |
| `0x828252D0` | `XuiSceneNavigateBack`. |

`XuiElementHasFocus(mainScene)` is true when the focused object is the main
scene itself or one of its descendants. It checks the global focus object and
the four user focus objects. It is false when focus belongs to a separately
navigated sibling scene.

### Main versus modal scene transitions

| Scene | Literal / load site | Navigation proof |
|---|---|---|
| Main | `Aurora_Main.xur` at `0x821208B8`; load path begins `0x82222560` | wrapper `0x823D1300`, cached branch calls `XuiSceneNavigateFirst` at `0x823D13EC` |
| QuickView | `Aurora_QuickView.xur` at `0x8212175C`; referenced at `0x8222D328` and `0x8222DDA4` | wrapper `0x823D0DE0`, cached branch calls `XuiSceneNavigateForward` at `0x823D0F68` |
| Details / Game Options | `Game_Options.xur` at `0x821280E8`; referenced at `0x8226CDEC` | same wrapper `0x823D0DE0` and forward-navigation branch |
| Utility popup | `Utilities_Popup.xur` at `0x82129318`; referenced at `0x82276300` | wrapper `0x823D1068`, cached branch calls `XuiSceneNavigateForward` at `0x823D11F4` |

The implementation of `XuiSceneNavigateForward` at `0x828250E0` obtains the
new scene's parent. If it has none, it obtains the current scene's parent and
inserts the new scene under that parent (`0x828251C8..0x82825224`). The new scene
is therefore a sibling in the navigation container, not a descendant of
`Aurora_Main`. That is the structural reason the focus predicate is suitable;
it is not an input-button or timing heuristic.

## Runtime contract

Configuration succeeds only if all of the following hold:

1. the caller asserts that the exact Rev1655 image was verified;
2. all memory and XUI predicate callbacks are present; and
3. the seven code/data signatures match in live memory.

Every probe then performs these read-only checks:

1. validate the application-manager vtable;
2. snapshot and traverse the cache with cycle and length bounds;
3. decode paths as target big-endian UTF-16 and locate exactly one logical
   `Aurora_Main.xur` node;
4. double-read each node and re-read the head/manager to detect concurrent
   changes;
5. require `acquired == 1`, a nonzero handle, and
   `XuiHandleIsValid(handle) == 1`;
6. require `XuiElementHasFocus(handle) == 1`; and
7. re-read the main node and cache head after the XUI predicates.

Only reason `main-focused` returns `allows_capture = 1`. Every other result,
including not configured and transient cache changes, returns zero.

The Xbox binding uses `MmIsAddressValid` before memory reads. No cache flag,
focus object, scene handle, XUI tree, or Aurora setting is modified.

## Integration handoff

After the exact-image verifier succeeds and before publishing any hooks:

```c
az_rev1655_scene_gate_reset();
result = az_rev1655_scene_gate_configure_default(1u);
```

Keep input fail-closed if configuration is not `AZ_SCENE_GATE_CONFIGURE_OK`.
At each hooked input decision, probe immediately before `az_input_process` and
publish only its Boolean result through:

```c
az_rev1655_input_detour_set_scene_allows_capture(
    az_rev1655_scene_gate_probe(&decision));
```

Probe again on the selector's render path before constructing
`AzOverlayDrawRequest`. The existing renderer derives `proven_modal_clear` from
`input_status.scene_allows_capture`; a stale result from before Aurora handled
RB would otherwise allow one or more selector frames over QuickView. Probing
only when handling R3/A/Left/Right is insufficient because an Aurora modal can
appear asynchronously.

The source still needs to be added to the native runtime target, and the test
source to a host test target. Do not enable `CONSUME` merely because the module
links.

## Telemetry

`az_rev1655_scene_gate_snapshot_status` exposes:

- configuration attempts/successes, exact-image and signature state;
- total probes, allows, and denies;
- the last reason, cache head, main node, main handle, and scanned-node count;
- counters for unavailable manager, unreadable memory, cache change/cycle/limit,
  bad paths, missing/duplicate main scene, inactive main node, invalid handle,
  and main scene without focus.

Log bounded scalar telemetry only. Do not log arbitrary path contents or dump
target memory.

## Required hardware observations

Run first in `OBSERVE`; `CONSUME` must remain ineligible. Record the decision and
bounded status snapshot at each row.

| Console state | Expected gate result | Required observation |
|---|---|---|
| Main coverflow idle | `main-focused`, allow | one main suffix, acquired `1`, valid handle, focus `1` |
| R3 selector open | `main-focused`, allow | same main node/handle; overlay visible |
| QuickView opened with stock RB | `main-not-focused`, deny | overlay absent on the first rendered QuickView frame |
| Back from QuickView | `main-focused`, allow | same or newly validated main handle |
| Details / Game Options | `main-not-focused`, deny | overlay absent throughout transition and scene |
| Utility/filter/settings popup | deny | identify whether focus loss or another fail-closed reason occurs |
| Aurora message box | deny | confirm its focus ancestry; this path is not fully proven statically here |
| Xbox Guide/system UI | deny through the existing `XamIsUIActive` gate | scene gate may remain focused; both gates are required |
| Scene opening/closing transition | deny on any changing snapshot | no cache fault, hang, or visible stale overlay |
| Repeated open/back loop | counters remain coherent | no handle reuse accepted without `XuiHandleIsValid` |

The static evidence proves the addresses, layouts, and QuickView/Details/popup
forward-navigation paths for the exact binary. The table above is deliberately
left as hardware evidence: live focus ownership, acquired-flag lifetime, and
message-box behavior must be observed on the console before capture is enabled.
