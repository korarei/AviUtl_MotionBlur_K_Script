cmake_minimum_required(VERSION 3.20.0)
project(MotionBlur_K VERSION 1.0.1 LANGUAGES CXX)

# Set the project version.
set(PROJECT_VERSION_SUFFIX "" CACHE STRING "Version suffix (e.g., alpha, beta, rc, etc.)")

if(PROJECT_VERSION_SUFFIX)
    set(FULL_VERSION "${PROJECT_VERSION} - ${PROJECT_VERSION_SUFFIX}")
else()
    set(FULL_VERSION "${PROJECT_VERSION}")
endif()

# Path Settings.
set(LUA_DIR "${CMAKE_SOURCE_DIR}/../.lua")
set(SDK_DIR "${CMAKE_SOURCE_DIR}/aviutl_exedit_sdk")

# Importing Lua Libraries.
add_library(lua51 STATIC IMPORTED)
set_target_properties(lua51 PROPERTIES
    IMPORTED_LOCATION "${LUA_DIR}/lib/lua51.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${LUA_DIR}/include"
)

# Main target definition.
add_library(${PROJECT_NAME} SHARED
    main.cpp
    object_motion_blur.cpp
    aul_utils.cpp
    lua_func.cpp
    shared_memory.cpp
    transform_utils.cpp
    utils.cpp
)

# Def file specification.
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/main.def"
    PREFIX ""
)

# Include Path.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${LUA_DIR}/include
    ${SDK_DIR}
)

# Library Link.
target_link_libraries(${PROJECT_NAME} PRIVATE
    lua51
)

# C++ version specification.
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

# Definitions.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PROJECT_VERSION="${FULL_VERSION}"
)

# Compiler Dependent Options.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /source-charset:utf-8
        /execution-charset:shift_jis
        /arch:SSE2
        /W4 # Warning Level 4.
        /permissive- # Standards-based mode.

        # Release-only optimizations.
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL> # Link Optimization.
        $<$<CONFIG:Release>:/Gy> # Split by function. (Remove unused functions.)

        # Debug settings.
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi> # Debugging Information.
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/LTCG> # Runtime Optimization.
        $<$<CONFIG:Debug>:/DEBUG> # Link with debug information.
    )

    # Runtime library switching.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
# Functionality has not been confirmed.
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic

        # Release-only
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-ffunction-sections>
        $<$<CONFIG:Release>:-fdata-sections>

        # Debug-only
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g>
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:-Wl,--gc-sections>
        $<$<CONFIG:Debug>:-g>
    )
endif()