cmake_minimum_required(VERSION 3.22)
project(oryon LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(ORYON_DEBUG        "Log verbose dan pemeriksaan tambahan" OFF)
option(ORYON_BUILD_TESTS  "Bangun tes host (butuh Mesa EGL + GLES)" OFF)

file(GLOB ORYON_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.cpp")

add_library(oryon SHARED ${ORYON_SOURCES})

target_include_directories(oryon PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Bionic tidak punya lazy binding: semua simbol diikat saat dlopen. Karena itu
# permukaan ekspor dijaga dua lapis - visibility hidden plus version script.
target_compile_options(oryon PRIVATE
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -fno-exceptions
    -fno-rtti
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -ffunction-sections
    -fdata-sections
    -Wall -Wextra -Wno-unused-parameter
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-fomit-frame-pointer>)

if(ORYON_DEBUG)
    target_compile_definitions(oryon PRIVATE ORYON_DEBUG=1)
endif()

target_link_options(oryon PRIVATE
    -Wl,--no-undefined
    -Wl,-z,defs
    -Wl,--gc-sections
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/oryon.map")

target_link_libraries(oryon PRIVATE ${CMAKE_DL_LIBS})
if(NOT ANDROID)
    target_link_libraries(oryon PRIVATE m)
endif()
if(ANDROID)
    target_link_libraries(oryon PRIVATE log)
endif()

set_target_properties(oryon PROPERTIES
    OUTPUT_NAME oryon
    PREFIX "lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# --------------------------------------------------------------------- tes --
if(ORYON_BUILD_TESTS AND NOT ANDROID)
    add_executable(lwjgl_bootstrap "${CMAKE_CURRENT_SOURCE_DIR}/tests/lwjgl_bootstrap.c")
    target_include_directories(lwjgl_bootstrap PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_link_libraries(lwjgl_bootstrap PRIVATE EGL ${CMAKE_DL_LIBS})
    add_dependencies(lwjgl_bootstrap oryon)

    # Tes generator shader menautkan sumber Oryon langsung, supaya bisa
    # memanggil ffp_* tanpa mengekspornya dari .so.
    add_executable(shadergen_test
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/shadergen_test.cpp"
        ${ORYON_SOURCES})
    target_include_directories(shadergen_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_options(shadergen_test PRIVATE -fno-exceptions -fno-rtti)
    if(ORYON_DEBUG)
        target_compile_definitions(shadergen_test PRIVATE ORYON_DEBUG=1)
    endif()
    target_link_libraries(shadergen_test PRIVATE EGL ${CMAKE_DL_LIBS} m)

    add_executable(render_test
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/render_test.cpp"
        ${ORYON_SOURCES})
    target_include_directories(render_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_options(render_test PRIVATE -fno-exceptions -fno-rtti)
    if(ORYON_DEBUG)
        target_compile_definitions(render_test PRIVATE ORYON_DEBUG=1)
    endif()
    target_link_libraries(render_test PRIVATE EGL ${CMAKE_DL_LIBS} m)

    add_executable(stage6_test
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage6_test.cpp"
        ${ORYON_SOURCES})
    target_include_directories(stage6_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_options(stage6_test PRIVATE -fno-exceptions -fno-rtti)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/mc_shaders.inc")
        target_compile_definitions(stage6_test PRIVATE ORYON_HAVE_MC_SHADERS=1)
    endif()
    if(ORYON_DEBUG)
        target_compile_definitions(stage6_test PRIVATE ORYON_DEBUG=1)
    endif()
    target_link_libraries(stage6_test PRIVATE EGL ${CMAKE_DL_LIBS} m)

    add_executable(entity_test
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/entity_test.cpp"
        ${ORYON_SOURCES})
    target_include_directories(entity_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_options(entity_test PRIVATE -fno-exceptions -fno-rtti)
    if(ORYON_DEBUG)
        target_compile_definitions(entity_test PRIVATE ORYON_DEBUG=1)
    endif()
    target_link_libraries(entity_test PRIVATE EGL ${CMAKE_DL_LIBS} m)
endif()
