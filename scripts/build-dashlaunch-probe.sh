#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
toolchain_root="${OPENXECHAIN_ROOT:-/opt/openxechain}"
output_dir="${repo_root}/build/dashlaunch-probe"
mkdir -p "${output_dir}"
export LIBRARY_PATH="" C_INCLUDE_PATH="" CPLUS_INCLUDE_PATH=""
"${toolchain_root}/bin/clang" -std=c99 -Oz -Wall -Wextra -Werror \
    -fno-zero-initialized-in-bss -DAURORAAZ_XBOX360=1 -DAURORAAZ_BOOT_PROBE=1 \
    -I"${repo_root}/native/include" \
    "${repo_root}/native/src/dashlaunch_probe.c" \
    "${repo_root}/native/src/netdbg_m2a_exports.c" \
    -Wl,/dll -Wl,/entry:DllMain \
    -Wl,/def:"${repo_root}/native/netdbg_exports.def" \
    -Wl,/base:0x91D00000 -Wl,/filealign:128 -Wl,/align:4096 -Wl,/opt:ref \
    -Xlinker "/section:.xexexp,ER" \
    -o "${output_dir}/AuroraAZ-boot-probe.dll"
python3 "${repo_root}/scripts/xex_exports.py" prepare-pe \
    --pe "${output_dir}/AuroraAZ-boot-probe.dll" --ordinals 2,3,4,5
"${toolchain_root}/bin/synthxex" -t sysdll \
    -i "${output_dir}/AuroraAZ-boot-probe.dll" \
    -o "${output_dir}/AuroraAZ-boot-probe.xex"
python3 "${repo_root}/scripts/xex_exports.py" finalize-xex \
    --pe "${output_dir}/AuroraAZ-boot-probe.dll" \
    --xex "${output_dir}/AuroraAZ-boot-probe.xex" \
    --ordinals 2,3,4,5 --module-flags 0xA
(cd "${output_dir}" && sha256sum AuroraAZ-boot-probe.xex > AuroraAZ-boot-probe.xex.sha256)
python3 "${repo_root}/scripts/describe-dashlaunch-probe.py" \
    "${output_dir}/AuroraAZ-boot-probe.dll" \
    "${output_dir}/AuroraAZ-boot-probe.json"

"${toolchain_root}/bin/clang" -std=c99 -Oz -Wall -Wextra -Werror \
    -fno-zero-initialized-in-bss -I"${repo_root}/native/include" \
    "${repo_root}/native/src/dashlaunch_probe_runner.c" \
    -Wl,/entry:main -Wl,/base:0x82000000 -Wl,/filealign:128 -Wl,/align:4096 \
    -o "${output_dir}/Run-Boot-Probe.dll"
"${toolchain_root}/bin/synthxex" -t title \
    -i "${output_dir}/Run-Boot-Probe.dll" -o "${output_dir}/Run-Boot-Probe.xex"
(cd "${output_dir}" && sha256sum Run-Boot-Probe.xex > Run-Boot-Probe.xex.sha256)
