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
`GameContentManager` and Aurora's stock movement gates before calling:

- `0x8234BC68` to move toward a lower index;
- `0x8234C148` to move toward a higher index.

Both calls use the stock coverflow helper at `GameContentManager + 0x550`, the
stock layout pointer at `+0x21B8`, the distance from the current index at
`+0x584`, and Aurora's existing movement speed. Pending and in-flight jumps
participate in the title-launch shutdown drain.

## Configure Modules integration

Aurora's fixed module list already contains the optional NETDBG/key-7 row. The
plugin changes only the verified live wrapper label from `Network Debugger` to
`Aurora A-Z`; no skin or XUR is modified.

The dispatcher completion tail at `0x822C8CE8` is hooked. At that point `r30`
still equals `7` only for the NETDBG row. The detour queues a settings request,
emulates the displaced `li r5,1`, and resumes at `0x822C8CEC`, so Aurora keeps
its original task-completion behavior. Every other module passes through.

The worker opens an asynchronous system message box with Browse, Filter, and
Cancel. Browse is the default. A saved choice is written as a tiny versioned
file at `game:\Data\AuroraAZ.ini`; missing, torn, oversized, or unknown content
falls back to Browse. This generated state does not change the one-XEX install
contract.
