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
    src/ppc.c
    src/selector.c
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

    add_executable(auroraaz_ppc_tests tests/ppc_tests.c)
    target_link_libraries(auroraaz_ppc_tests PRIVATE auroraaz_core)

    if(MSVC)
        target_compile_options(auroraaz_core_tests PRIVATE /W4 /WX)
        target_compile_options(auroraaz_input_tests PRIVATE /W4 /WX)
        target_compile_options(auroraaz_glyph_atlas_tests PRIVATE /W4 /WX)
        target_compile_options(auroraaz_hook_plan_tests PRIVATE /W4 /WX)
        target_compile_options(auroraaz_ppc_tests PRIVATE /W4 /WX)
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
        target_compile_options(auroraaz_ppc_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
    endif()

    add_test(NAME auroraaz_core_tests COMMAND auroraaz_core_tests)
    add_test(NAME auroraaz_input_tests COMMAND auroraaz_input_tests)
    add_test(NAME auroraaz_glyph_atlas_tests COMMAND auroraaz_glyph_atlas_tests)
    add_test(NAME auroraaz_hook_plan_tests COMMAND auroraaz_hook_plan_tests)
    add_test(NAME auroraaz_ppc_tests COMMAND auroraaz_ppc_tests)
endif()
