# Rev1655 input-detour implementation contract

Date: 2026-09-02

## Scope

`native/src/input_detour.c` and `input_detour_shim.S` implement the Xbox-only
bridge between Aurora Rev1655's keystroke wrapper and the already host-tested
selector policy. They are deliberately not part of the inert canary build and
install no hook by themselves.

The bridge is exact-build code. It may only be reached after the running image
identity and the signatures in `NATIVE_HOOKS.md` pass. A different Aurora
revision must not reuse these addresses.

## Exact ABI

The hook target is Aurora's wrapper at `0x82801D90`, not the imported XAM
function. Its ABI is:

```c
uint32_t AuroraInputWrapper(
    uint32_t user_index,        /* r3 */
    uint32_t flags,             /* r4 */
    XINPUT_KEYSTROKE *key);     /* r5 */
```

Aurora changes this into the imported `XamInputGetKeystrokeEx(DWORD*, DWORD,
XINPUT_KEYSTROKE*)` call internally. The imported/wrapper result is zero only
when a key was returned. The eight-byte key layout is asserted by the portable
input module.

The live target patch must be a non-linking relative branch. The entry shim
copies the untouched LR to `r6`, the fourth C argument, then tail-branches to
`az_rev1655_input_detour_c`. Only LR `0x822113F8`, the instruction following
the per-frame call at `0x822113F4`, is allowed to drive selector policy. The
secondary drain helper returns to `0x82211ED4` and is always passed through.
Any other caller is counted and passed through.

The dedicated fallback trampoline executes the displaced `mflr r12`, then
branches absolutely to `0x82801D94`. This avoids a race in which a monolithic
hook installer makes the target live before publishing its generated
trampoline. It is valid only because the exact Rev1655 signature proves that
the displaced instruction and continuation address match.

## Stages and fail-closed gates

The bridge starts `OFF`. `OBSERVE` may be requested only after image and input
hook verification. It calls the original first, records successful raw keys in
a fixed 32-entry single-producer/single-consumer queue, and never clears
Aurora's key buffer.

`CONSUME` selector ownership is refused unless all of these have been
published:

1. exact Aurora image identity passed;
2. exact input and RenderMenu hook signatures passed and both hooks are live;
3. A, R3, D-pad Left/Right, and left-stick Left/Right were each confirmed from
   hardware observations;
4. at event time, the dynamic Aurora scene/modal probe allows capture;
5. a successful RenderMenu report belongs to the immediately preceding input
   frame.

The filter-consumer gate is independent. Until its worker completes the
read-only runtime probe, A is consumed while the selector is active but leaves
the selector open and queues no work. This permits a bounded interaction
canary without impersonating filter readiness or leaking A into title launch.

Failure of any dynamic gate preserves new input. A release/repeat belonging to
a key already consumed remains consumed until release, preventing half of a
press/release pair from leaking into Aurora. RB is never consumed; the portable
policy also leaves selector mode as RB continues into Aurora's stock filter
menu.

The RenderMenu hook only publishes `{input_frame, manager, result}` after it
calls Aurora's original method. The next main input call consumes one fresh
publication. Missing, stale, concurrent, failed, or null-manager reports
invalidate coverflow scope.

## No work inside the input hook

The input hook performs no allocation, file/network access, debug printing,
filtering, sorting, or render calls. It only calls Aurora's original wrapper,
updates bounded state, optionally clears the eight-byte key, and enqueues a
small observation/filter request.

A worker drains observations for logging. `take_filter_request()` marks one
request in flight; the worker must always call `finish_filter_request()`, even
after a rejected/failed apply. While a filter is pending or in flight, a new R3
entry is preserved rather than opening a second selector session.

The observation queue assumes the proven single per-frame caller is its sole
producer and one module worker is its sole consumer. A full queue drops new
telemetry and increments `observation_drops`; it never changes consumption.

## Installation and removal order

Installation remains disabled in the inert canary. A later explicit hardware
stage must use this order:

1. validate image and all exact signatures;
2. call `az_rev1655_input_detour_reset()`;
3. install the RenderMenu marker and input entry hooks, with the input hook's
   expected first instruction `0x7D8802A6`;
4. publish verification and request `OBSERVE` only;
5. drain/log observations from the module worker and verify every controller
   code plus caller LR;
6. prove scene/modal gating and filter-worker recovery before requesting
   `CONSUME`.

Removal must request `OFF`, disable new capture gates, let already-consumed
keys release until status `consumed_controls == 0`, remove the input branch,
wait until status `in_flight == 0`, then remove the render branch and free
code/data. Before unload, `pending_filter` must equal
`AZ_INPUT_DETOUR_NO_FILTER_REQUEST` and `filter_in_flight` must be false. Never
unload while a detour or filter request is in flight.

## Remaining hardware risks

- The expected left-stick virtual keys must still be confirmed on a physical
  controller and real kernel.
- The application input-before-render ordering inferred from Rev1655 control
  flow must be verified through observation counters.
- A reliable Aurora scene/modal probe is not implemented yet. Until it is,
  `scene_allows_capture` must remain false and consume mode handles nothing.
- The filter consumer is not implemented by this bridge. Its verification gate
  must remain false until the asynchronous ownership/threading canary in
  `FILTER_IMPLEMENTATION.md` passes.
- Hook removal still needs a module-level quiescence protocol; `in_flight` is
  only the input-side signal.
