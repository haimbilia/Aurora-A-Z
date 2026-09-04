#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
toolchain_root="${OPENXECHAIN_ROOT:-/opt/openxechain}"
output_dir="${AURORAAZ_OUTPUT_DIR:-${repo_root}/build/native-xbox360}"
embedded_icon_source="${output_dir}/auroraaz_embedded_icon.c"
embedded_settings_source="${output_dir}/auroraaz_embedded_settings.c"
embedded_settings_filter_source="${output_dir}/auroraaz_embedded_settings_filter.c"

compiler="${toolchain_root}/bin/clang"
packager="${toolchain_root}/bin/synthxex"

m2a_sources=(
    "${repo_root}/native/src/netdbg_bootstrap.c"
    "${repo_root}/native/src/netdbg_m2a_exports.c"
    "${repo_root}/native/src/netdbg_lifetime_rev1655.c"
    "${repo_root}/native/src/image.c"
    "${repo_root}/native/src/filters.c"
    "${repo_root}/native/src/filter_consumer_xbox360.c"
    "${repo_root}/native/src/browse_consumer_rev1655.c"
    "${repo_root}/native/src/sha256.c"
    "${repo_root}/native/src/rev1655_hook_gate.c"
    "${repo_root}/native/src/ppc.c"
    "${repo_root}/native/src/hook_plan.c"
    "${repo_root}/native/src/hook_runtime.c"
    "${repo_root}/native/src/selector.c"
    "${repo_root}/native/src/input.c"
    "${repo_root}/native/src/input_detour.c"
    "${repo_root}/native/src/input_detour_shim.S"
    "${repo_root}/native/src/glyph_atlas.c"
    "${repo_root}/native/src/layout.c"
    "${repo_root}/native/src/overlay_model.c"
    "${repo_root}/native/src/operation_mode.c"
    "${repo_root}/native/src/module_settings_detour.c"
    "${repo_root}/native/src/module_settings_detour_shim.S"
    "${repo_root}/native/src/overlay_renderer_xbox360.c"
    "${repo_root}/native/src/render_detours.c"
    "${repo_root}/native/src/render_detour_shims.S"
    "${repo_root}/native/src/content_launch_detour.c"
    "${repo_root}/native/src/content_launch_detour_shim.S"
    "${repo_root}/native/src/scene_gate_rev1655.c"
    "${repo_root}/native/src/m2a_input_telemetry.c"
    "${repo_root}/native/src/rev1655_runtime.c"
)

if [[ ! -x "${compiler}" ]]; then
    echo "OpenXeChain compiler not found: ${compiler}" >&2
    exit 1
fi

if [[ ! -x "${packager}" ]]; then
    echo "SynthXEX not found: ${packager}" >&2
    exit 1
fi

mkdir -p "${output_dir}"
python3 "${repo_root}/scripts/generate-embedded-icon.py" \
    "${repo_root}/icon.png" "${embedded_icon_source}" --size 64
python3 "${repo_root}/scripts/generate-embedded-settings.py" \
    "${repo_root}/native/assets/AuroraAZ_Settings.xur" \
    "${embedded_settings_source}"
python3 "${repo_root}/scripts/generate-embedded-settings.py" \
    "${repo_root}/native/assets/AuroraAZ_Settings_Filter.xur" \
    "${embedded_settings_filter_source}" \
    --symbol g_auroraaz_embedded_settings_filter_xur

export LIBRARY_PATH=""
export C_INCLUDE_PATH=""
export CPLUS_INCLUDE_PATH=""

"${compiler}" \
    -std=c99 \
    -Oz \
    -fno-zero-initialized-in-bss \
    -Wall -Wextra -Werror \
    -DAURORAAZ_XBOX360=1 \
    -DAURORAAZ_NETDBG_TITLE_EXIT_SHUTDOWN=1 \
    -I"${repo_root}/native/include" \
    "${embedded_icon_source}" \
    "${embedded_settings_source}" \
    "${embedded_settings_filter_source}" \
    "${m2a_sources[@]}" \
    -Wl,/dll \
    -Wl,/entry:DllMain \
    -Wl,/def:"${repo_root}/native/netdbg_exports.def" \
    -Wl,/base:0x82D50000 \
    -Wl,/filealign:128 \
    -Wl,/align:65536 \
    -Wl,/opt:ref \
    -Xlinker "/section:.xexexp,ER" \
    -Xlinker "/section:.azhook,ERW" \
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
