# Native implementation

This directory contains the platform-independent selector core and the native
Xbox 360 canary. The installed production payload remains one file:
`AuroraAZ.xex`.

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

`src/canary.c` is deliberately read-only. A bounded system monitor waits up to
60 seconds for Aurora, then validates its loader entry, PE identity and `.text`
layout before checking three exact Rev1655 code probes. It logs the result with
`DbgPrint`, installs no hooks, draws nothing, and writes no console state.

The cross-build entry point is `scripts/build-openxechain.sh`. It requires a
complete OpenXeChain prefix; SynthXEX alone is not a compiler. See
`reference/NATIVE_TOOLCHAIN.md` and `reference/NATIVE_LOADER.md` before trying
to deploy the result.
