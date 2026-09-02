# Native Xbox 360 toolchain audit

Date: 2026-09-02

## Result

A real `AuroraAZ.xex` can be produced as one file, but a complete compiler is
not available on this workstation yet. The XEX packager stage is installed and
verified. The remaining blocker is the PowerPC/Xbox-360 PE compiler and linker.

Do not treat a file renamed to `.xex`, a libXenon ELF, or a repacked existing
binary as a plugin build.

## Installed Microsoft/XDK route

The workstation was checked for the normal Xbox 360 XDK integration:

- `%XEDK%` is not set.
- `C:\Program Files (x86)\Microsoft Xbox 360 SDK` is absent.
- `C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\Platforms\Xbox 360`
  is absent.
- No `xexbuild.exe`, `imagexex.exe`, `xextool.exe`, `xenon-gcc`, or PowerPC
  cross-compiler is on `PATH`.
- Visual Studio Community 2026 is installed, but its C++ toolset directory is
  absent and `vswhere` did not report the native C++ workload.
- The 32-bit .NET Framework MSBuild 4 executable is present at
  `C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe`, but it is not
  useful without the XDK's Xbox 360 platform files and libraries.

Therefore an XDK project cannot currently be compiled here. If a legitimately
licensed XDK is supplied later, this remains the conservative production
route. Primary project references confirm that a DashLaunch-style module is a
dynamic/system XEX with a DLL entry point and XEX image metadata:

- <https://github.com/ClementDreptin/ModdingResources/blob/main/GettingStarted/getting-started.md>
- <https://github.com/xeghosted/xbox360-xex-vs2026-tutorial/blob/main/README.md>

## Clean-room route: OpenXeChain

[OpenXeChain](https://github.com/OpenXeChain) is a current clean-room LLVM
toolchain targeting programs that run under the Xbox 360 operating system. It
is distinct from libXenon, which targets bare-metal XeLL payloads and is not a
drop-in dashboard plugin toolchain.

The audited buildscript commit is
`eed1fa65bf9577fd31625764b320a90182ea9ade`. It pins:

| Component | Commit |
| --- | --- |
| LLVM/Clang/LLD | `890b83f6c8259a8899e182a5f7d9cf39c64131cc` |
| Newlib | `f929633c27099d404e9cb5b2739a9c7f9b6afccc` |
| SynthXEX | `48d1453a55468aa2f8a211db0b20edd594ef5be3` (`v0.0.6`) |
| xecorelib | `c65d67e071357acade681f04c46ae9719797f239` |

Evidence relevant to AuroraAZ:

- SynthXEX `v0.0.6` accepts `-t sysdll`, the required container class for a
  system/DashLaunch-style plugin.
- xecorelib supplies ordinal import libraries for `xboxkrnl.exe` and
  `xam.xex`; its definitions include `DbgPrint`, module-loading APIs,
  `XamInputGetState`, and `XamInputGetKeystrokeEx`.
- The project describes itself as early-development software. Hardware
  acceptance is mandatory; successful compilation alone is not a release
  gate.

Primary sources:

- <https://github.com/OpenXeChain/buildscript>
- <https://github.com/OpenXeChain/SynthXEX>
- <https://github.com/OpenXeChain/xecorelib>

## Local packager proof

WSL2 Ubuntu 24.04 is installed. The following packages were installed:

```text
build-essential 12.10ubuntu1
bzip2           1.0.8-5.1ubuntu0.1
clang           1:18.0-59~exp2 (compiler reports 18.1.3)
cmake           3.28.3-1build7
gawk            1:5.2.1-2ubuntu0.1
git             1:2.43.0-1ubuntu7.3
gzip            1.12-1ubuntu3.2
lld             1:18.0-59~exp2
make            4.3-4.1build2
ninja-build     1.11.1-2
python3-yaml    6.0.1-2build2
unzip           6.0-28ubuntu4.1
zip             3.0-13ubuntu0.2
```

Exact setup command:

```powershell
wsl.exe -d Ubuntu-24.04 -- bash -lc `
  "export DEBIAN_FRONTEND=noninteractive; apt-get update -qq && apt-get install -y -qq clang cmake ninja-build make build-essential git python3-yaml bzip2 gzip unzip zip gawk lld"
```

SynthXEX was built at its buildscript-pinned version and installed at
`/opt/openxechain-packer-v0.0.6/bin/synthxex`:

```bash
git clone --branch v0.0.6 --depth 1 \
  https://github.com/OpenXeChain/SynthXEX.git \
  /opt/auroraaz-synthxex-v0.0.6

cmake -S /opt/auroraaz-synthxex-v0.0.6 \
  -B /opt/auroraaz-synthxex-v0.0.6/build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/openxechain-packer-v0.0.6

cmake --build /opt/auroraaz-synthxex-v0.0.6/build --parallel 2
cmake --install /opt/auroraaz-synthxex-v0.0.6/build
```

`synthxex --version` reports `v0.0.6`; `synthxex --help` lists `sysdll`.
The source plus build tree occupies about 1.2 MiB and the installed packager
about 60 KiB.

As a compatibility probe, SynthXEX validated the extracted stock
`original/Aurora.exe` as a PE and read its basic headers, but exited `255` while
reading its XDK-generated import table:

```text
SynthXEX> PE valid!
SynthXEX> Got header data from PE!
SynthXEX> Retrieving import data from PE...
SynthXEX> ERROR: Invalid RVA or offset found. Aborting.
```

The latest SynthXEX main commit
`4bda05e21e3f6384ac447f8db3103a0a15b96887` was also built and produced the
same result. This does not show that OpenXeChain-generated PE files fail; it
shows that repacking Aurora's XDK PE is not a substitute for building a plugin.

Ubuntu's stock Clang is also not a substitute. This command emits an
`elf32-powerpc` object even with a Windows-looking triple:

```bash
clang --target=powerpc-pc-windows-msvc \
  -ffreestanding -nostdlib -c test.c -o test.obj
```

Stock `lld-link-18 /machine:ppc` then fails with `unknown /machine argument:
ppc`. OpenXeChain's patched LLVM/LLD is required.

## Disk constraint and prebuilt check

The full OpenXeChain LLVM repository reports a GitHub repository size of
1,680,790 KiB before a build tree. The host currently has less than 9 GiB free
on `C:` and less than 1 GiB free on `D:`. A local LLVM source build was not
started because its transient source, object, and install footprint could fill
the host volume.

The primary OpenXeChain repositories currently publish no GitHub Releases and
no GitHub Actions artifacts for the buildscript or LLVM repository. No
verified prebuilt compiler was found. Third-party binaries were deliberately
not substituted.

Installing the WSL prerequisites changed the WSL root filesystem from roughly
1.5 GiB used to 3.23 GB used. `/usr/lib/llvm-18` occupies about 657 MiB and the
APT archive cache about 241 MiB at the time of measurement.

## What unblocks a real canary

Use one of these two evidence-backed paths:

1. Install a legitimately obtained Xbox 360 XDK and its legacy Xbox 360
   MSBuild platform integration, then build a dynamic/system XEX.
2. Give the OpenXeChain build at least 20-25 GiB of safe free workspace (or a
   disposable CI runner), build the pinned toolchain, and hardware-test its
   `sysdll` output before writing any hooks.

The OpenXeChain source build starts with:

```bash
git clone https://github.com/OpenXeChain/buildscript.git openxechain-build
cd openxechain-build

# The repository records SSH submodule URLs. Override them on machines without
# GitHub SSH credentials.
git config submodule.llvm.url https://github.com/OpenXeChain/llvm.git
git config submodule.newlib.url https://github.com/OpenXeChain/newlib.git
git config submodule.synthxex.url https://github.com/OpenXeChain/SynthXEX.git
git config submodule.xecorelib.url https://github.com/OpenXeChain/xecorelib.git
git submodule update --init --recursive

PREFIX=/opt/openxechain PARALLEL=2 ./build-toolchain.sh
```

Before calling the result usable, build a no-hook canary that only writes an
`AuroraAZ` load/unload line through `DbgPrint`, wrap it with
`synthxex -t sysdll`, inspect it with `tools/jeff.exe xex info`, and load it
only through the isolated hardware test path. Until that succeeds, the answer
to “can a real canary be built here?” is **not yet**.
