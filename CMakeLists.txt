cmake_minimum_required(VERSION 3.16)

option(JUGGLER_USE_VCPKG "Use vcpkg manifest mode to provide SDL" ON)
option(JUGGLER_BUILD_RAYTRACE "Build the SDL Amiga preview raytrace target" ON)
set(JUGGLER_SDL_VERSION "3" CACHE STRING "SDL major version to use: 2 or 3")
set_property(CACHE JUGGLER_SDL_VERSION PROPERTY STRINGS 2 3)

if (NOT JUGGLER_SDL_VERSION STREQUAL "2" AND NOT JUGGLER_SDL_VERSION STREQUAL "3")
    message(FATAL_ERROR "JUGGLER_SDL_VERSION must be 2 or 3.")
endif()

if (JUGGLER_USE_VCPKG AND JUGGLER_BUILD_RAYTRACE AND NOT CMAKE_TOOLCHAIN_FILE)
    set(_vcpkg_roots)

    if (DEFINED ENV{VCPKG_ROOT})
        list(APPEND _vcpkg_roots "$ENV{VCPKG_ROOT}")
    endif()

    if (CMAKE_HOST_WIN32)
        list(APPEND _vcpkg_roots
            "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg"
            "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/vcpkg"
            "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/vcpkg"
            "C:/Program Files/Microsoft Visual Studio/2022/BuildTools/VC/vcpkg"
            "C:/vcpkg"
        )
    endif()

    foreach (_vcpkg_root IN LISTS _vcpkg_roots)
        if (EXISTS "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
            set(CMAKE_TOOLCHAIN_FILE
                "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake"
                CACHE FILEPATH "vcpkg toolchain file"
            )
            message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
            break()
        endif()
    endforeach()
endif()

if (JUGGLER_USE_VCPKG AND JUGGLER_BUILD_RAYTRACE)
    set(VCPKG_MANIFEST_MODE ON CACHE BOOL "Use vcpkg manifest mode")
    set(VCPKG_MANIFEST_INSTALL ON CACHE BOOL "Install vcpkg manifest dependencies")
    set(VCPKG_MANIFEST_NO_DEFAULT_FEATURES
        ON
        CACHE BOOL "Disable vcpkg manifest default features"
    )
    set(VCPKG_MANIFEST_FEATURES
        "sdl${JUGGLER_SDL_VERSION}"
        CACHE STRING "vcpkg manifest features"
    )
endif()

project(juggler_raytrace LANGUAGES C)

include(CTest)

if (JUGGLER_USE_VCPKG AND JUGGLER_BUILD_RAYTRACE AND NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR
        "SDL${JUGGLER_SDL_VERSION} is provided through vcpkg manifest mode. Set VCPKG_ROOT or "
        "configure with -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake. "
        "To use an existing SDL installation instead, configure with -DJUGGLER_USE_VCPKG=OFF."
    )
endif()

if (JUGGLER_BUILD_RAYTRACE)
    enable_language(CXX)

    if (JUGGLER_SDL_VERSION STREQUAL "2")
        find_package(SDL2 CONFIG REQUIRED)
    else()
        find_package(SDL3 CONFIG REQUIRED)
    endif()

    add_executable(raytrace
        src/rt1.c
        src/rt2.c
        src/rt3.c
        src/datparse.c
        src/emulate-amiga.c
        src/png.c
    )

    set_target_properties(raytrace PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    set_source_files_properties(
        src/rt1.c
        src/rt2.c
        src/rt3.c
        src/datparse.c
        src/emulate-amiga.c
        src/png.c
        PROPERTIES LANGUAGE CXX
    )

    target_compile_features(raytrace PRIVATE cxx_std_11)
    target_include_directories(raytrace PRIVATE src)
    if (JUGGLER_SDL_VERSION STREQUAL "2")
        target_compile_definitions(raytrace PRIVATE JUGGLER_USE_SDL2)
        target_link_libraries(raytrace PRIVATE SDL2::SDL2)
    else()
        target_link_libraries(raytrace PRIVATE SDL3::SDL3)
    endif()

    if (UNIX)
        target_link_libraries(raytrace PRIVATE m)
    endif()
endif()

# Source reconstruction of the original Amiga ssg renderer, recovered from the
# 68000 executable: Eric's own names, dense formatting, the binary's scanf
# grammar, single-precision math, and its screen-space-culling renderer.
# Verified to reproduce the original binary's output. See ssg-src-authentic/.
add_executable(ssg_authentic
    ssg-src-authentic/ssg.c
    ssg-src-authentic/raytrace.c
    ssg-src-authentic/ham.c
)

target_include_directories(ssg_authentic PRIVATE ssg-src-authentic)

if (UNIX)
    target_link_libraries(ssg_authentic PRIVATE m)
endif()

if (BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    set(_ssg_validation_files
        "${CMAKE_SOURCE_DIR}/tools/validate-ssg.py"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/robot.dat"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/robot-s2-fresh.rgb"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/robot.rgb"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/ele.dat"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/ele-s2.rgb"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/ele.rgb"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/dragon.dat"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/dragon-s2.rgb"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/robot-s2-v.rgb"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/robot-s2.ssimg"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/robot-s2-v.ssimg"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/ele-s2.ssimg"
        "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code/dragon-s2.ssimg"
    )
    set(_ssg_can_validate TRUE)
    foreach (_ssg_validation_file IN LISTS _ssg_validation_files)
        if (NOT EXISTS "${_ssg_validation_file}")
            set(_ssg_can_validate FALSE)
        endif()
    endforeach()

    if (Python3_Interpreter_FOUND AND _ssg_can_validate)
        add_test(
            NAME ssg_validate
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/validate-ssg.py"
                "--scene-dir"
                "${CMAKE_SOURCE_DIR}/Raytracer_1987_Graham_Source_Code"
                "--exe"
                "$<TARGET_FILE:ssg_authentic>"
        )
    endif()
endif()
