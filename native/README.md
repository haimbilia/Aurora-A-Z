# Native implementation

This directory contains the platform-independent selector core and the native
Xbox 360 runtime. The release payload is one file, `AuroraAZ.xex`, installed as
`Plugins\AuroraAZ.xex` and loaded by a DashLaunch plugin slot. Its DashLaunch
entry creates a retrying bootstrap worker, validates the exact Rev1655 image
and thread wrapper before installing any hooks, and does not use Aurora's
Network Debugger slot. The NetDbg source and evidence below are retained as
historical loader research.

## Host tests

The state machine, built-in filter mapping, compatibility probes, and logical
layout are ordinary C99 and can be tested without an Xbox toolchain:

```bash
cmake -S native -B build/native-host -G Ninja
cmake --build build/native-host
ctest --test-dir build/native-host --output-on-failure
```

Both endpoint behaviors are implemented and tested. The Xbox integration must
choose `AZ_EDGE_CLAMP` or `AZ_EDGE_WRAP` only after that product decision is
recorded in `REQUIREMENTS.md`.

## M1 Xbox canary

`src/canary.c` is the historical, deliberately inert M1 payload. Its worker
validates Aurora's fixed-base PE identity and `.text` layout before checking
the exact Rev1655 code probes. It installs no hooks, draws nothing, and mutates
no Aurora state.

The older NOVA helper remains available for read-only thread diagnostics:

```powershell
pwsh -File scripts/verify-nova-canary.ps1
```

It is not the final M1 acceptance oracle. Aurora's working thread wrapper starts
at `XapiThreadStartup`, so M1 used durable primary and worker marker records to
prove the ordinal call and the first instruction executed by AuroraAZ-owned
worker code.

The first hardware attempt failed safely in the isolated lab. Aurora logged:

```text
Failed to load game:\Plugins\NetDbgDll.xex
Failed to load NetDbgDll
```

The corrected retry packages as a title DLL (`0x9` rather than `0xA`), includes
Image Base Address optional header `0x10201 = 0x91D00000`, and omits
SynthXEX's empty TLS header. Its round-trip SHA-256 was
`C51E3A322B07D1DE094C644E33D005D87305FFB24B587548953F1E88678C63E5`,
and Aurora then logged:

```text
IDllBase::Load: Completing DLLModule loading:  dll.aurora.netdbg
PluginManager: Module Loaded:  dll.aurora.netdbg
```

This earlier retry proved the corrected container and ordinal-resolution path,
but its entry-point observation was inconclusive. The production installation
remained untouched and the lab file was recoverably renamed
`.disabled-c51e3a322b07`.

The final M1 canary came from commit `39b551c`, GitHub Actions run
`33604028771`, with SHA-256
`87894F41A89F4F3CAAFA8A1864AB8F8A91A2ED011882EEEF36E4D3FAEF58596C`.
The staged file downloaded from the lab had the same hash. It validated
Aurora's complete 100-byte Rev1655 thread wrapper at `0x82361AA8` and the first
32 bytes of `XapiThreadStartup` at `0x82804650` before calling the wrapper. The
wrapper supplies `XapiThreadStartup`, creates the thread with flags `2`, selects
processor `3`, sets priority `15`, and resumes the handle once.

The 36-byte big-endian `AZM1` primary record decoded as version `4`, record size
`36`, call count `1`, source ordinal `4`, phase `5` (`COMPLETE`), state `2`
(`RUNNING`), create status `0`, and resume status `0`. A separate worker record
had the same identity and status fields with phase `7` (`WORKER_ENTERED`). These
records prove automatic ordinal-4 dispatch and worker entry, so M1 is complete.

`src/netdbg_exports.c` preserves the M1 canary contract. The current M2a build
uses `src/netdbg_m2a_exports.c`: ordinal 4 is Aurora's deterministic logger
callback and performs only a once-gated worker bootstrap before returning;
ordinals 2, 3, and 5 remain compatibility no-ops. The worker in
`src/netdbg_bootstrap.c` records its entry and starts only
`AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE`. It does not enable selector ownership,
overlay rendering, or filtering.

The cross-build validates that the PE export table is exactly ordinals 2, 3, 4,
and 5. Because stock SynthXEX does not generate the Xbox export fields itself,
the pipeline embeds the big-endian export-address table before packaging, sets
the security-info pointer afterward, and then validates the module flags,
explicit image-base header, absence of the synthetic TLS header, export table,
code-page descriptor, page hash chain, and header hash before producing the
SHA-256 file. A PE-only export success cannot reach deployment.

The cross-build entry point is `scripts/build-openxechain.sh`. It currently
packages the M2a observe-only bootstrap and input-hook runtime; render and
filter-consumer sources are deliberately excluded. It requires a complete
OpenXeChain prefix; SynthXEX alone is not a compiler. See
`reference/NATIVE_TOOLCHAIN.md`, `reference/NATIVE_LOADER.md`, and
`reference/NETDBG_BOOTSTRAP.md` before trying to deploy the result.
