cmake_minimum_required(VERSION 3.20)

project(AuroraAZCore LANGUAGES C)

add_library(auroraaz_core STATIC
    src/compatibility.c
    src/filters.c
    src/glyph_atlas.c
    src/hook_plan.c
    src/image.c
    src/input.c
    src/input_hook.c
    src/layout.c
    src/overlay_model.c
    src/ppc.c
    src/rev1655_hook_gate.c
    src/selector.c
    src/sha256.c
)

target_include_directories(auroraaz_core PUBLIC include)
target_compile_features(auroraaz_core PUBLIC c_std_99)

if(MSVC)
    target_compile_options(auroraaz_core PRIVATE /W4 /WX)
else()
    target_compile_options(auroraaz_core PRIVATE
        -Wall -Wextra -Wconversion -Werror -Wpedantic
    )
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(auroraaz_core_tests tests/core_tests.c)
    target_link_libraries(auroraaz_core_tests PRIVATE auroraaz_core)

    add_executable(auroraaz_input_tests tests/input_tests.c)
    target_link_libraries(auroraaz_input_tests PRIVATE auroraaz_core)

    add_executable(auroraaz_glyph_atlas_tests tests/glyph_atlas_tests.c)
    target_link_libraries(auroraaz_glyph_atlas_tests PRIVATE auroraaz_core)

    add_executable(auroraaz_hook_plan_tests tests/hook_plan_tests.c)
    target_link_libraries(auroraaz_hook_plan_tests PRIVATE auroraaz_core)

    add_executable(auroraaz_overlay_model_tests tests/overlay_model_tests.c)
    target_link_libraries(auroraaz_overlay_model_tests PRIVATE auroraaz_core)

    add_executable(auroraaz_ppc_tests tests/ppc_tests.c)
    target_link_libraries(auroraaz_ppc_tests PRIVATE auroraaz_core)

    add_executable(auroraaz_rev1655_hook_gate_tests
        tests/rev1655_hook_gate_tests.c)
    target_link_libraries(auroraaz_rev1655_hook_gate_tests
        PRIVATE auroraaz_core)
    target_include_directories(auroraaz_rev1655_hook_gate_tests PRIVATE src)

    add_executable(auroraaz_hook_runtime_tests
        tests/hook_runtime_tests.c
        src/hook_runtime.c)
    target_link_libraries(auroraaz_hook_runtime_tests
        PRIVATE auroraaz_core)
    target_include_directories(auroraaz_hook_runtime_tests
        PRIVATE tests/stubs)
    target_compile_definitions(auroraaz_hook_runtime_tests
        PRIVATE
            AURORAAZ_XBOX360=1
            AURORAAZ_HOOK_RUNTIME_TEST_ALLOCATOR=1)

    add_executable(auroraaz_rev1655_runtime_tests
        tests/rev1655_runtime_tests.c
        src/rev1655_runtime.c
        src/m2a_input_telemetry.c)
    target_link_libraries(auroraaz_rev1655_runtime_tests
        PRIVATE auroraaz_core)
    target_include_directories(auroraaz_rev1655_runtime_tests
        PRIVATE tests/rev1655_runtime_include src)
    target_compile_definitions(auroraaz_rev1655_runtime_tests
        PRIVATE
            AURORAAZ_XBOX360=1
            AURORAAZ_REV1655_THREAD_TEST=1
            AURORAAZ_REV1655_RUNTIME_TEST_IO=1)

    add_executable(auroraaz_netdbg_bootstrap_tests
        tests/netdbg_bootstrap_tests.c
        src/netdbg_bootstrap.c
        src/netdbg_m2a_exports.c)
    target_include_directories(auroraaz_netdbg_bootstrap_tests
        PRIVATE tests/netdbg_bootstrap_include include)
    target_compile_definitions(auroraaz_netdbg_bootstrap_tests
        PRIVATE
            AURORAAZ_XBOX360=1
            AURORAAZ_REV1655_THREAD_TEST=1)

    add_executable(auroraaz_netdbg_lifetime_rev1655_tests
        tests/netdbg_lifetime_rev1655_tests.c
        src/netdbg_lifetime_rev1655.c)
    target_include_directories(auroraaz_netdbg_lifetime_rev1655_tests
        PRIVATE include)

    # Compile the Xbox-facing runtime drafts only into their host test
    # executables. They intentionally remain outside auroraaz_core and the
    # release linkage until their hardware gates have passed.
    add_executable(auroraaz_filter_consumer_xbox360_tests
        tests/filter_consumer_xbox360_tests.c
        src/filter_consumer_xbox360.c)
    target_link_libraries(auroraaz_filter_consumer_xbox360_tests
        PRIVATE auroraaz_core)

    add_executable(auroraaz_input_detour_xbox_tests
        tests/input_detour_xbox_tests.c
        src/input_detour.c)
    target_link_libraries(auroraaz_input_detour_xbox_tests
        PRIVATE auroraaz_core)
    target_include_directories(auroraaz_input_detour_xbox_tests
        PRIVATE tests/include)
    target_compile_definitions(auroraaz_input_detour_xbox_tests
        PRIVATE AURORAAZ_XBOX360=1)

    add_executable(auroraaz_overlay_renderer_xbox360_tests
        tests/overlay_renderer_xbox360_tests.c
        src/overlay_renderer_xbox360.c)
    target_link_libraries(auroraaz_overlay_renderer_xbox360_tests
        PRIVATE auroraaz_core)
    target_compile_definitions(auroraaz_overlay_renderer_xbox360_tests
        PRIVATE AURORAAZ_XBOX360=1)

    add_executable(auroraaz_render_detours_tests
        tests/render_detours_tests.c
        src/render_detours.c)
    target_link_libraries(auroraaz_render_detours_tests
        PRIVATE auroraaz_core)

    add_executable(auroraaz_scene_gate_rev1655_tests
        tests/scene_gate_rev1655_tests.c
        src/scene_gate_rev1655.c)
    target_link_libraries(auroraaz_scene_gate_rev1655_tests
        PRIVATE auroraaz_core)

    add_executable(auroraaz_m2a_input_telemetry_tests
        tests/m2a_input_telemetry_tests.c
        src/m2a_input_telemetry.c)
    target_link_libraries(auroraaz_m2a_input_telemetry_tests
        PRIVATE auroraaz_core)

    add_executable(auroraaz_m2b_scene_telemetry_tests
        tests/m2b_scene_telemetry_tests.c
        src/m2b_scene_telemetry.c)
    target_link_libraries(auroraaz_m2b_scene_telemetry_tests
        PRIVATE auroraaz_core)

    add_executable(auroraaz_render_observe_rev1655_tests
        tests/render_observe_rev1655_tests.c
        src/render_observe_rev1655.c)
    target_link_libraries(auroraaz_render_observe_rev1655_tests
        PRIVATE auroraaz_core)
    target_compile_definitions(auroraaz_render_observe_rev1655_tests
        PRIVATE AURORAAZ_RENDER_OBSERVE_TEST_HOOKS=1)

    set(AURORAAZ_TEST_TARGETS
        auroraaz_core_tests
        auroraaz_input_tests
        auroraaz_glyph_atlas_tests
        auroraaz_hook_plan_tests
        auroraaz_overlay_model_tests
        auroraaz_ppc_tests
        auroraaz_rev1655_hook_gate_tests
        auroraaz_hook_runtime_tests
        auroraaz_rev1655_runtime_tests
        auroraaz_netdbg_bootstrap_tests
        auroraaz_netdbg_lifetime_rev1655_tests
        auroraaz_filter_consumer_xbox360_tests
        auroraaz_input_detour_xbox_tests
        auroraaz_overlay_renderer_xbox360_tests
        auroraaz_render_detours_tests
        auroraaz_scene_gate_rev1655_tests
        auroraaz_m2a_input_telemetry_tests
        auroraaz_m2b_scene_telemetry_tests
        auroraaz_render_observe_rev1655_tests
    )

    # These tests use assert() as their executable check. Keep assertions
    # active even when CI selects a Release build.
    foreach(test_target IN LISTS AURORAAZ_TEST_TARGETS)
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${test_target} PRIVATE -UNDEBUG)
        endif()
    endforeach()

    if(MSVC)
        target_compile_options(auroraaz_core_tests PRIVATE /W4 /WX)
        target_compile_options(auroraaz_input_tests PRIVATE /W4 /WX)
        target_compile_options(auroraaz_glyph_atlas_tests PRIVATE /W4 /WX)
        target_compile_options(auroraaz_hook_plan_tests PRIVATE /W4 /WX)
        target_compile_options(auroraaz_overlay_model_tests PRIVATE /W4 /WX)
        target_compile_options(auroraaz_ppc_tests PRIVATE /W4 /WX)
        target_compile_options(auroraaz_rev1655_hook_gate_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_hook_runtime_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_rev1655_runtime_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_netdbg_bootstrap_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_netdbg_lifetime_rev1655_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_filter_consumer_xbox360_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_input_detour_xbox_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_overlay_renderer_xbox360_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_render_detours_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_scene_gate_rev1655_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_m2a_input_telemetry_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_m2b_scene_telemetry_tests
            PRIVATE /W4 /WX)
        target_compile_options(auroraaz_render_observe_rev1655_tests
            PRIVATE /W4 /WX)
    else()
        target_compile_options(auroraaz_core_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_input_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_glyph_atlas_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_hook_plan_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_overlay_model_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_ppc_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_rev1655_hook_gate_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_hook_runtime_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_rev1655_runtime_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_netdbg_bootstrap_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_netdbg_lifetime_rev1655_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_filter_consumer_xbox360_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_input_detour_xbox_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_overlay_renderer_xbox360_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_render_detours_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_scene_gate_rev1655_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_m2a_input_telemetry_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_m2b_scene_telemetry_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
        target_compile_options(auroraaz_render_observe_rev1655_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
    endif()

    add_test(NAME auroraaz_core_tests COMMAND auroraaz_core_tests)
    add_test(NAME auroraaz_input_tests COMMAND auroraaz_input_tests)
    add_test(NAME auroraaz_glyph_atlas_tests COMMAND auroraaz_glyph_atlas_tests)
    add_test(NAME auroraaz_hook_plan_tests COMMAND auroraaz_hook_plan_tests)
    add_test(NAME auroraaz_overlay_model_tests COMMAND auroraaz_overlay_model_tests)
    add_test(NAME auroraaz_ppc_tests COMMAND auroraaz_ppc_tests)
    add_test(NAME auroraaz_rev1655_hook_gate_tests
        COMMAND auroraaz_rev1655_hook_gate_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/../original/Aurora.exe")
    add_test(NAME auroraaz_hook_runtime_tests
        COMMAND auroraaz_hook_runtime_tests)
    add_test(NAME auroraaz_rev1655_runtime_tests
        COMMAND auroraaz_rev1655_runtime_tests)
    add_test(NAME auroraaz_netdbg_bootstrap_tests
        COMMAND auroraaz_netdbg_bootstrap_tests)
    add_test(NAME auroraaz_netdbg_lifetime_rev1655_tests
        COMMAND auroraaz_netdbg_lifetime_rev1655_tests)
    add_test(NAME auroraaz_filter_consumer_xbox360_tests
        COMMAND auroraaz_filter_consumer_xbox360_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/../original/Aurora.exe")
    add_test(NAME auroraaz_input_detour_xbox_tests
        COMMAND auroraaz_input_detour_xbox_tests)
    add_test(NAME auroraaz_overlay_renderer_xbox360_tests
        COMMAND auroraaz_overlay_renderer_xbox360_tests)
    add_test(NAME auroraaz_render_detours_tests
        COMMAND auroraaz_render_detours_tests)
    add_test(NAME auroraaz_scene_gate_rev1655_tests
        COMMAND auroraaz_scene_gate_rev1655_tests)
    add_test(NAME auroraaz_m2a_input_telemetry_tests
        COMMAND auroraaz_m2a_input_telemetry_tests)
    add_test(NAME auroraaz_m2b_scene_telemetry_tests
        COMMAND auroraaz_m2b_scene_telemetry_tests)
    add_test(NAME auroraaz_render_observe_rev1655_tests
        COMMAND auroraaz_render_observe_rev1655_tests)
endif()
