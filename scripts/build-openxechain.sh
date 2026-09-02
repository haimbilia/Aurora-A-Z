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
    -fno-zero-initialized-in-bss \
    -Wall -Wextra -Werror \
    -DAURORAAZ_XBOX360=1 \
    -I"${repo_root}/native/include" \
    "${repo_root}/native/src/canary.c" \
    "${repo_root}/native/src/netdbg_exports.c" \
    "${repo_root}/native/src/compatibility.c" \
    "${repo_root}/native/src/image.c" \
    -Wl,/dll \
    -Wl,/entry:DllMain \
    -Wl,/def:"${repo_root}/native/netdbg_exports.def" \
    -Wl,/base:0x91D00000 \
    -Wl,/filealign:128 \
    -Wl,/align:4096 \
    -Wl,/opt:ref \
    -Xlinker "/section:.xexexp,ER" \
    -o "${output_dir}/AuroraAZ.dll"

python3 "${repo_root}/scripts/xex_exports.py" prepare-pe \
    --pe "${output_dir}/AuroraAZ.dll" \
    --ordinals 2,3,4,5

"${packager}" \
    -t titledll \
    -i "${output_dir}/AuroraAZ.dll" \
    -o "${output_dir}/AuroraAZ.xex"

python3 "${repo_root}/scripts/xex_exports.py" finalize-xex \
    --pe "${output_dir}/AuroraAZ.dll" \
    --xex "${output_dir}/AuroraAZ.xex" \
    --ordinals 2,3,4,5

python3 "${repo_root}/scripts/xex_exports.py" validate \
    --pe "${output_dir}/AuroraAZ.dll" \
    --xex "${output_dir}/AuroraAZ.xex" \
    --ordinals 2,3,4,5

(cd "${output_dir}" && sha256sum "AuroraAZ.xex" > "AuroraAZ.xex.sha256")
echo "Built ${output_dir}/AuroraAZ.xex"
