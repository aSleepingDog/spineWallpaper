cmake_minimum_required(VERSION 3.20)

project(SpineWallpaper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep repository paths relative in the cache. CMake resolves them to full
# paths only while generating the build tree, so a fresh configure continues
# to work after the whole repository is moved to another directory.
set(CPP_DIST_DIR "dist")
set(CPP_RELEASE_DIR "release")
set(SFML_DEPENDENCY_ROOT "dependencies")

# The cache variables remain overridable for maintainers who want to test a
# different runtime or SFML build. Relative overrides are relative to the
# repository root; absolute overrides are still supported for external builds.
set(SPINE_CPP_ROOT "spine-cpp" CACHE PATH
    "Root of the native spine-cpp source tree")
set(SPINE_SFML_ROOT "spine-sfml" CACHE PATH
    "Root of the native spine-sfml source tree")
if(APPLE)
    set(SFML_ARCHIVE_NAME "SFML-2.4.1-osx-clang.tar.gz")
    set(SFML_URL "https://www.sfml-dev.org/files/${SFML_ARCHIVE_NAME}")
    set(SFML_DEFAULT_ROOT "${SFML_DEPENDENCY_ROOT}/SFML-2.4.1-osx-clang")
elseif(UNIX)
    set(SFML_ARCHIVE_NAME "SFML-2.4.1-linux-gcc-64-bit.tar.gz")
    set(SFML_URL "https://www.sfml-dev.org/files/${SFML_ARCHIVE_NAME}")
    set(SFML_DEFAULT_ROOT "${SFML_DEPENDENCY_ROOT}/SFML-2.4.1")
else()
    # This Windows project is fixed to the x64 SFML package.
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR
            "This project requires a 64-bit Windows target. Configure CMake with -A x64.")
    endif()
    set(SFML_ARCHIVE_NAME "SFML-2.4.1-windows-vc14-64-bit.zip")
    set(SFML_URL "https://www.sfml-dev.org/files/${SFML_ARCHIVE_NAME}")
    set(SFML_DEFAULT_ROOT "${SFML_DEPENDENCY_ROOT}/SFML-2.4.1")
endif()
set(SFML_ROOT "${SFML_DEFAULT_ROOT}" CACHE PATH
    "SFML installation root containing include/ and lib/")
option(SPINEWALLPAPER_DOWNLOAD_SFML
    "Download the default SFML archive when it is not available" ON)

function(resolve_repository_path input output)
    if(IS_ABSOLUTE "${input}")
        set(resolved "${input}")
    else()
        set(resolved "${CMAKE_CURRENT_SOURCE_DIR}/${input}")
    endif()
    set(${output} "${resolved}" PARENT_SCOPE)
endfunction()

resolve_repository_path("${SPINE_CPP_ROOT}" _spine_cpp_root)
resolve_repository_path("${SPINE_SFML_ROOT}" _spine_sfml_root)
resolve_repository_path("${SFML_DEPENDENCY_ROOT}" _sfml_dependency_root)
resolve_repository_path("${SFML_ROOT}" _sfml_root)
resolve_repository_path("${SFML_DEFAULT_ROOT}" _sfml_default_root)
resolve_repository_path("${CPP_DIST_DIR}" _cpp_dist_dir)
resolve_repository_path("${CPP_RELEASE_DIR}" _cpp_release_dir)

if(NOT EXISTS "${_sfml_root}"
   AND SPINEWALLPAPER_DOWNLOAD_SFML
   AND "${_sfml_root}" STREQUAL "${_sfml_default_root}")
    file(MAKE_DIRECTORY "${_sfml_dependency_root}")
    set(_sfml_archive "${_sfml_dependency_root}/${SFML_ARCHIVE_NAME}")

    if(NOT EXISTS "${_sfml_archive}")
        message(STATUS "Downloading default SFML from ${SFML_URL}")
        file(DOWNLOAD "${SFML_URL}" "${_sfml_archive}"
            SHOW_PROGRESS
            STATUS _sfml_download_status
            LOG _sfml_download_log
            TLS_VERIFY ON)
        list(GET _sfml_download_status 0 _sfml_download_code)
        if(NOT _sfml_download_code EQUAL 0)
            message(FATAL_ERROR
                "Failed to download SFML from ${SFML_URL}: ${_sfml_download_log}")
        endif()
    else()
        message(STATUS "Using cached SFML archive ${_sfml_archive}")
    endif()

    message(STATUS "Extracting SFML into ${_sfml_dependency_root}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xf "${_sfml_archive}"
        WORKING_DIRECTORY "${_sfml_dependency_root}"
        RESULT_VARIABLE _sfml_extract_result
        OUTPUT_VARIABLE _sfml_extract_output
        ERROR_VARIABLE _sfml_extract_error)
    if(NOT _sfml_extract_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to extract ${_sfml_archive}: ${_sfml_extract_error}")
    endif()

    if(APPLE
       AND EXISTS "${_sfml_default_root}/extlibs/freetype.framework"
       AND NOT EXISTS "${_sfml_default_root}/Frameworks/freetype.framework")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${_sfml_default_root}/Frameworks")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${_sfml_default_root}/extlibs/freetype.framework"
                "${_sfml_default_root}/Frameworks/freetype.framework"
            RESULT_VARIABLE _sfml_freetype_result)
        if(NOT _sfml_freetype_result EQUAL 0)
            message(FATAL_ERROR "Failed to prepare the SFML freetype framework")
        endif()
    endif()
endif()

if(NOT EXISTS "${_spine_cpp_root}/include/spine/spine.h")
    message(FATAL_ERROR
        "Native Spine C++ source was not found. Set SPINE_CPP_ROOT to the directory containing include/spine/.")
endif()

if(NOT EXISTS "${_spine_sfml_root}/spine/spine-sfml.h")
    message(FATAL_ERROR
        "Native spine-sfml source was not found. Set SPINE_SFML_ROOT to the directory containing spine/.")
endif()

file(GLOB_RECURSE SPINE_CPP_SOURCES CONFIGURE_DEPENDS
    "${_spine_cpp_root}/src/*.cpp")
set(SPINE_SFML_ADAPTER "${_spine_sfml_root}/spine/spine-sfml.cpp")

if(NOT EXISTS "${SPINE_SFML_ADAPTER}")
    message(FATAL_ERROR "The spine-sfml adapter was not found: ${SPINE_SFML_ADAPTER}")
endif()

find_path(SFML_INCLUDE_DIR SFML/Graphics.hpp
    PATHS "${_sfml_root}/include"
    NO_DEFAULT_PATH)

find_library(SFML_GRAPHICS_LIBRARY NAMES sfml-graphics-s sfml-graphics
    PATHS "${_sfml_root}/lib" NO_DEFAULT_PATH)
find_library(SFML_WINDOW_LIBRARY NAMES sfml-window-s sfml-window
    PATHS "${_sfml_root}/lib" NO_DEFAULT_PATH)
find_library(SFML_SYSTEM_LIBRARY NAMES sfml-system-s sfml-system
    PATHS "${_sfml_root}/lib" NO_DEFAULT_PATH)

if(NOT SFML_INCLUDE_DIR OR NOT SFML_GRAPHICS_LIBRARY OR NOT SFML_WINDOW_LIBRARY OR NOT SFML_SYSTEM_LIBRARY)
    message(FATAL_ERROR
        "SFML was not found. Set SFML_ROOT to an SFML installation containing include/ and lib/.")
endif()

get_filename_component(_sfml_graphics_name "${SFML_GRAPHICS_LIBRARY}" NAME)
set(SFML_STATIC FALSE)
if(_sfml_graphics_name MATCHES "-s(\\.lib)?$")
    set(SFML_STATIC TRUE)
endif()

add_library(spine-cpp STATIC ${SPINE_CPP_SOURCES})
target_include_directories(spine-cpp PUBLIC
    "${_spine_cpp_root}/include")
target_compile_definitions(spine-cpp PRIVATE _CRT_SECURE_NO_WARNINGS)

add_library(spine-sfml-cpp STATIC "${SPINE_SFML_ADAPTER}")
target_include_directories(spine-sfml-cpp PUBLIC
    "${_spine_cpp_root}/include"
    "${_spine_sfml_root}"
    "${SFML_INCLUDE_DIR}")
target_link_libraries(spine-sfml-cpp PUBLIC spine-cpp)
target_compile_definitions(spine-sfml-cpp PRIVATE _CRT_SECURE_NO_WARNINGS)

add_executable(SpineWallpaperCore WIN32
    src/configuration.cpp
    src/configuration.h
    src/main.cpp
    src/renderer.cpp
    src/renderer.h
    src/settings_types.h
    src/startup_registration.cpp
    src/startup_registration.h
    src/texture_loader.cpp
    src/texture_loader.h)

target_include_directories(SpineWallpaperCore PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${_spine_cpp_root}/include"
    "${_spine_sfml_root}"
    "${SFML_INCLUDE_DIR}")

target_compile_definitions(SpineWallpaperCore PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Release>:SPINEWALLPAPER_RELEASE>)

if(SFML_STATIC)
    target_compile_definitions(spine-sfml-cpp PRIVATE SFML_STATIC)
    target_compile_definitions(SpineWallpaperCore PRIVATE SFML_STATIC)
endif()

target_link_libraries(SpineWallpaperCore PRIVATE
    spine-sfml-cpp
    "${SFML_GRAPHICS_LIBRARY}"
    "${SFML_WINDOW_LIBRARY}"
    "${SFML_SYSTEM_LIBRARY}"
    opengl32
    gdi32
    winmm
    user32
    shell32
    advapi32
    comctl32
    dwmapi)

if(SFML_STATIC)
    find_library(SFML_FREETYPE_LIBRARY NAMES freetype freetype.lib
        PATHS "${_sfml_root}/lib" NO_DEFAULT_PATH)
    find_library(SFML_JPEG_LIBRARY NAMES jpeg jpeg.lib
        PATHS "${_sfml_root}/lib" NO_DEFAULT_PATH)
    if(SFML_FREETYPE_LIBRARY)
        target_link_libraries(SpineWallpaperCore PRIVATE "${SFML_FREETYPE_LIBRARY}")
    endif()
    if(SFML_JPEG_LIBRARY)
        target_link_libraries(SpineWallpaperCore PRIVATE "${SFML_JPEG_LIBRARY}")
    endif()
endif()

set_target_properties(SpineWallpaperCore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_cpp_dist_dir}")

if(MSVC)
    # The default SFML libraries are Release-built (/MD, iterator debug level 0).
    # Keep Debug symbols and checks in our sources while matching their ABI. This
    # is Debug-only and leaves the Release configuration and package unchanged.
    target_compile_definitions(spine-cpp PRIVATE
        $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
    target_compile_definitions(spine-sfml-cpp PRIVATE
        $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
    target_compile_definitions(SpineWallpaperCore PRIVATE
        $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
    set_property(TARGET spine-cpp PROPERTY MSVC_RUNTIME_LIBRARY
        "MultiThreadedDLL")
    set_property(TARGET spine-sfml-cpp PROPERTY MSVC_RUNTIME_LIBRARY
        "MultiThreadedDLL")
    set_property(TARGET SpineWallpaperCore PROPERTY MSVC_RUNTIME_LIBRARY
        "MultiThreadedDLL")
    target_compile_options(SpineWallpaperCore PRIVATE /W4 /EHsc /utf-8)
else()
    target_compile_options(SpineWallpaperCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The entry point is a small /MT bootstrapper. The actual renderer remains a
# normal /MD application, but the bootstrapper can run even when the VC++
# runtime is absent and install it before launching the renderer.
add_executable(SpineWallpaper WIN32 src/launcher.cpp)
target_compile_definitions(SpineWallpaper PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS)
target_link_libraries(SpineWallpaper PRIVATE
    shell32
    user32
    wininet)
set_target_properties(SpineWallpaper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_cpp_dist_dir}")

if(MSVC)
    set_property(TARGET SpineWallpaper PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(SpineWallpaper PRIVATE /W4 /EHsc /utf-8)
else()
    target_compile_options(SpineWallpaper PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_target(SpineWallpaperPackage ALL
    COMMAND ${CMAKE_COMMAND}
        "-DPACKAGE_CONFIG=$<CONFIG>"
        "-DRELEASE_DIR=${_cpp_release_dir}"
        "-DSOURCE_LAUNCHER=$<TARGET_FILE:SpineWallpaper>"
        "-DSOURCE_CORE=$<TARGET_FILE:SpineWallpaperCore>"
        "-DSOURCE_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/SpineWallpaper.ini"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package-release.cmake"
    DEPENDS SpineWallpaper SpineWallpaperCore
    COMMENT "Packaging native SpineWallpaper into the root release directory")
