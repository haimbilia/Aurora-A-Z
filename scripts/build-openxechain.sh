#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
toolchain_root="${OPENXECHAIN_ROOT:-/opt/openxechain}"
output_dir="${AURORAAZ_OUTPUT_DIR:-${repo_root}/build/native-xbox360}"

compiler="${toolchain_root}/bin/clang"
packager="${toolchain_root}/bin/synthxex"

if [[ ! -x "${compiler}" ]]; then
    echo "OpenXeChain compiler not found: ${compiler}" >&2
    exit 1
fi

if [[ ! -x "${packager}" ]]; then
    echo "SynthXEX not found: ${packager}" >&2
    exit 1
fi

mkdir -p "${output_dir}"

export LIBRARY_PATH=""
export C_INCLUDE_PATH=""
export CPLUS_INCLUDE_PATH=""

"${compiler}" \
    -std=c99 \
    -Oz \
    -Wall -Wextra -Werror \
    -DAURORAAZ_XBOX360=1 \
    -I"${repo_root}/native/include" \
    "${repo_root}/native/src/canary.c" \
    "${repo_root}/native/src/compatibility.c" \
    "${repo_root}/native/src/image.c" \
    -Wl,/dll \
    -Wl,/entry:DllMain \
    -Wl,/base:0x91D00000 \
    -Wl,/filealign:128 \
    -Wl,/align:4096 \
    -Wl,/opt:ref \
    -o "${output_dir}/AuroraAZ.dll"

"${packager}" \
    -t sysdll \
    -i "${output_dir}/AuroraAZ.dll" \
    -o "${output_dir}/AuroraAZ.xex"

(cd "${output_dir}" && sha256sum "AuroraAZ.xex" > "AuroraAZ.xex.sha256")
echo "Built ${output_dir}/AuroraAZ.xex"
