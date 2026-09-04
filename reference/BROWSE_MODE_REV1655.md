# Rev1655 Browse mode and settings contract

Date: 2026-09-04

This contract applies only to Aurora 0.7b.2 Rev1655. The complete Aurora image
gate and each hook-site signature must pass before any address below is used.

## Browse data path

Browse mode reads Aurora's already-sorted active list. It never asks Aurora to
sort, filter, or swap lists.

- `0x8223FF30`: `GameListManager` singleton.
- manager `+0x74`: map containing the active vectors.
- `0x8223FFB0`: map lookup; key `2` returns the current active vector.
- vector elements are 8-byte shared-pointer pairs. The content object address
  is the first word.
- content object `+0x35C`: Aurora string containing the displayed title name.

The worker snapshots the vector bounds, scans the current QuickView result,
then reads the bounds again. A changed vector cancels the request. `ALL` maps
to index zero; `#` matches empty names and names not beginning with ASCII A-Z;
letters are case-insensitive. A missing match leaves the selection untouched.

The worker publishes only `{GameContentManager, target index, item count}`.
It never calls coverflow UI code.

## UI-thread movement

The input detour consumes a published jump on Aurora's next main input poll,
including a poll that reports no keystroke. It revalidates the live
`GameContentManager` and Aurora's stock movement gates, then uses Aurora's
own `0x8234C698` cover-window population path. It publishes the previous and
new selection indices and invokes the same coverflow-owner selection callback
used by Aurora's movement code. This updates the visible coverflow immediately
instead of animating across every intervening title. Pending and in-flight
jumps participate in the title-launch shutdown drain.

## Configure Modules integration

Aurora's fixed module list already contains the optional NETDBG/key-7 row. The
plugin changes only the verified live wrapper label from `Network Debugger` to
`Aurora A-Z`; no skin or XUR is modified.

The module-scene dispatcher at `0x822C8B88` is hooked. For NETDBG key `7`, the
detour supplies the XUI resource locator
`file://game:/Data/AuroraAZ_Settings.xur` and rejoins Aurora's normal
module scene loader at `0x822C8BF0`. Every other key receives the displaced
`cmpwi cr6,r30,1` and resumes at `0x822C8B8C`, preserving the stock dispatcher.

The XUR is compiled from `native/assets/AuroraAZ_Settings.xui`, embedded in the
XEX, and rewritten to the runtime cache before the hook is installed. It is a
classless 800x570 scene hosted inside Aurora's existing `ModuleHost`, matching
the integration used by the FTP and Nova pages without requiring their private
C++ scene classes. Browse and Filter are ordinary native XUI buttons. The input
detour consumes A only when either button owns focus and queues the selected
mode to the worker; file I/O never occurs on the UI thread.

At dispatcher entry, `r31` is Aurora's temporary loader stack frame—not a
persistent controller. `XuiSceneCreate` writes the instantiated `HXUIOBJ` to
the frame's `+0x70` output slot. A second exact-signature detour at
`0x822C8C38` captures that handle immediately after creation and before the
loader returns; retaining the stack address is invalid. All control lookup,
focus testing, and status updates use the captured live handle. The resource
cache handle is only a template and must never be used for interaction. A
bounded `XuiElementGetParent` walk from the embedded scene reaches the live
settings container, where `ModuleIcon`, `ModuleList`, and the Aurora A-Z row's
`IconPresenter` receive the embedded icon. Lookup falls back to a bounded walk
using the standard
`XuiElementGetChildById`, `XuiElementGetFirstChild`, and `XuiElementGetNext`
exports.

Browse is the default. A saved choice is written as a tiny versioned file at
`game:\Data\AuroraAZ.ini`; missing, torn, oversized, or unknown content falls
back to Browse. The generated XUR, icon cache, and mode file do not change the
one-XEX installation contract.
