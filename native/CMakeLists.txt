cmake_minimum_required(VERSION 3.20)

project(AuroraAZCore LANGUAGES C)

add_library(auroraaz_core STATIC
    src/compatibility.c
    src/filters.c
    src/image.c
    src/layout.c
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

    if(MSVC)
        target_compile_options(auroraaz_core_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(auroraaz_core_tests PRIVATE
            -Wall -Wextra -Wconversion -Werror -Wpedantic
        )
    endif()

    add_test(NAME auroraaz_core_tests COMMAND auroraaz_core_tests)
endif()
