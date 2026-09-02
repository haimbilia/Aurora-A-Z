# Native implementation

This directory contains the platform-independent selector core and the native
Xbox 360 canary. The release payload remains one file, `AuroraAZ.xex`. Aurora
Rev1655 loads those same bytes from its reserved optional path, so the installed
filename is `Plugins\NetDbgDll.xex`. No companion file or DashLaunch slot is
used.

## Host tests

The state machine, built-in filter mapping, compatibility probes, and logical
layout are ordinary C99 and can be tested without an Xbox toolchain:

```bash
cmake -S native -B build/native-host -G Ninja
cmake --build build/native-host
ctest --test-dir build/native-host --output-on-failure
```

Both endpoint behaviors are implemented and tested. The Xbox integration must
choose `AZ_EDGE_CLAMP` or `AZ_EDGE_WRAP` only after that still-open product
decision is recorded in `REQUIREMENTS.md`.

## Xbox canary

`src/canary.c` is deliberately read-only. A system thread validates Aurora's
fixed-base PE identity and `.text` layout before checking three exact Rev1655
code probes. It then sleeps using only an atomic stop flag and a bounded kernel
delay; it never polls the module loader. This lets Aurora join it safely during
detach while leaving a live proof address for NOVA. The canary logs its result
with `DbgPrint`, installs no hooks, draws nothing, and writes no console state.

After a canary reboot, verify that evidence from the development PC with:

```powershell
pwsh -File scripts/verify-nova-canary.ps1
```

The check is read-only and fails unless Aurora is the running title and NOVA
reports a live thread starting inside the canary's reserved module window.

`src/netdbg_exports.c` supplies the four ordinal-only exports required by the
Rev1655 Network Debugger wrapper. They immediately return and ordinal 4 never
logs, avoiding recursion through Aurora's log sink. The cross-build validates
that the PE export table is exactly ordinals 2, 3, 4, and 5. Because stock
SynthXEX does not generate the Xbox export fields itself, the pipeline embeds
the big-endian export-address table before packaging, sets the security-info
pointer afterward, and then validates the table, code-page descriptor, page
hash chain, and header hash before producing the SHA-256 file. A PE-only export
success cannot reach deployment.

The cross-build entry point is `scripts/build-openxechain.sh`. It requires a
complete OpenXeChain prefix; SynthXEX alone is not a compiler. See
`reference/NATIVE_TOOLCHAIN.md` and `reference/NATIVE_LOADER.md` before trying
to deploy the result.
