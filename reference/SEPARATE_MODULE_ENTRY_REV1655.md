# Separate Configure Modules entry — Rev1655

Status: target bindings and the key-8 scene route are implemented and
host-tested; **not compiled into an Xbox release or deployed to hardware**.

## Goal

Add a distinct `Aurora A-Z` entry to **Configure Modules**, with its own
settings scene, while preserving the stock `Network Debugger` entry. This
replaces the old UI approach that renamed the fixed key-7 Network Debugger
row.

The loader remains out of scope here. The current XEX may still be bootstrapped
by Aurora's unused `NetDbgDll.xex` contract; this work changes only the module
registry and settings UI identity after that XEX is resident.

## Recovered registry contract

All addresses apply only to Aurora 0.7b.2 Rev1655 with the image hash recorded
in `NATIVE_LOADER.md`.

- `PluginManager` is the singleton at `0x82BC3860` (vtable `0x821425BC`).
- Its constructor (`0x8238CBF8`) inserts seven module wrappers in an ordered
  map at `PluginManager + 4`.
- The constructor sequence is:

  1. allocate a wrapper;
  2. construct it;
  3. form `{ key, wrapper }` on the stack;
  4. call `0x8238E6F0` with the map and pair to obtain an insertion hint;
  5. call `0x82227638` to insert the pair.

- Key 7 is the NetDbg wrapper and is a `0xA8` byte object with vtable
  `0x821425C0`. Its constructor is `0x8238E848`; its ordinal resolver is
  `0x82389650`.
- The existing key-7 wrapper holds the loaded module handle at `+0x60` and is
  ready at `+0xA4`.

## Planned mutation

After the existing key-7 wrapper is verified and pinned resident:

1. Refuse if key 8 already exists.
2. Allocate and invoke Aurora's own NetDbg wrapper constructor for a new
   `0xA8`-byte object.
3. Replace only the clone's display label with `Aurora A-Z`.
4. Copy the loaded key-7 module handle to the clone, set its policy to
   resident, and invoke the proven NetDbg ordinal resolver. No second XEX is
   loaded.
5. Insert the clone into Aurora's map under key **8** using Aurora's own map
   insertion routines.
6. Verify that Aurora's own key lookup returns the new wrapper.
7. Extend the settings-scene detour so key 8, not key 7, loads
   `AuroraAZ_Settings.xur`.

The resulting list should contain separate rows:

```text
Network Debugger    key 7  stock identity
Aurora A-Z          key 8  custom settings scene
```

## Safety gates before hardware enablement

- Keep the complete Rev1655 hash/instruction gate.
- Verify manager vtable, source key-7 vtable, nonzero module handle, and ready
  flag before allocating anything.
- Make registration idempotent: an existing key 8 is success-without-mutation.
- Never call Aurora's common loader for the clone; it must reuse the already
  loaded key-7 handle.
- Make both key-7 and key-8 wrappers resident so shutdown cannot unload the
  shared XEX twice.
- First hardware test is telemetry-only: register, lookup key 8, then reboot.
  Do not open Configure Modules until that startup/reboot gate passes.

## Current code

`native/src/module_registry_injection.c` implements the guarded registration
sequence through an address-based binding table. Its host tests prove normal,
unready-source, and repeated-registration paths. The Xbox binding calls only
the recovered Rev1655 functions above, and `rev1655_runtime.c` invokes it
before installing the module settings hooks. The assembly settings detour now
routes key 8 rather than key 7.

The remaining release gate is a clean OpenXeChain cross-compile followed by a
telemetry-only hardware canary. This development machine currently lacks the
expected `/opt/openxechain/bin/clang`, so no binary is considered releasable
from this checkout yet.
