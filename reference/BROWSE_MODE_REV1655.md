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

The settings UI has Browse and Filter variants compiled from
`native/assets/AuroraAZ_Settings.xui` and
`native/assets/AuroraAZ_Settings_Filter.xui`. Both are embedded in the XEX; the
worker writes the variant matching the persisted mode to the common runtime
cache before the hook is installed and after each mode change. They are
classless 800x570 scenes hosted inside Aurora's existing `ModuleHost`, matching
the integration used by the FTP and Nova pages without requiring their private
C++ scene classes. Browse and Filter use the same `XuiCheckbox` plus
`XuiRadioButton` visual combination as Aurora's Profile page. The matching
variant supplies the correct initial focus and checked marker when the page is
opened.

A new scene-generation counter initializes the tracked selection from the
persisted operation mode whenever Aurora instantiates the page. Directional
input updates that tracked choice in parallel with Aurora's native radio-row
navigation; A is consumed only while that captured page generation is active
and queues the tracked mode to the worker. The worker persists the choice,
refreshes the cached variant, and the live page updates its checked marker and
status. This avoids depending on generated XUR control IDs, which Aurora does
not expose reliably through the public descendant APIs. File I/O never occurs
on the UI thread. `XuiElementHasFocus` on the captured scene is checked first;
because Aurora may host this classless resource beside the focused child, the
verified controller's `ModuleHost` handle is the fallback lifecycle gate. When
focus returns to Aurora's module list, neither handle owns focus and the page
immediately stops owning A.

At dispatcher entry, `r31` is Aurora's temporary loader stack frame—not a
persistent controller. `XuiSceneCreate` writes the instantiated `HXUIOBJ` to
the frame's `+0x70` output slot. A second exact-signature detour at
`0x822C8C38` captures that handle immediately after creation and before the
loader returns; retaining the stack address is invalid. The resource cache
handle is only a template and must never be used for interaction. The entry
detour also captures Aurora's verified module-controller pointer from `r26`.
Rev1655's generated member bindings put `ModuleIcon` at controller `+0x60`,
`ModuleList` at `+0x68`, and `ModuleHost` at `+0x78`; the corresponding binding
routine is `0x822C9698`. The plugin replaces the header image through the
direct `ModuleIcon` handle and walks the direct list handle to update Aurora
A-Z's row presenter. Generated scene IDs remain only a best-effort path for
status text and row descendants.

Browse is the default. A saved choice is written as a tiny versioned file at
`game:\Data\AuroraAZ.ini`; missing, torn, oversized, or unknown content falls
back to Browse. Both generated XUR variants, the icon cache, and the mode file
are extracted at runtime from `AuroraAZ.xex`; they do not change the one-XEX
installation contract.
