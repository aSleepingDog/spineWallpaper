if(NOT PACKAGE_CONFIG STREQUAL "Release")
    message(STATUS "Skipping release packaging for ${PACKAGE_CONFIG} configuration")
    return()
endif()

function(run_cmake_command)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E ${ARGV}
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Packaging command failed with exit code ${result}")
    endif()
endfunction()

run_cmake_command(make_directory "${RELEASE_DIR}")
run_cmake_command(copy_if_different
    "${SOURCE_LAUNCHER}" "${RELEASE_DIR}/SpineWallpaper.exe")
run_cmake_command(make_directory "${RELEASE_DIR}/bin")
run_cmake_command(copy_if_different
    "${SOURCE_CORE}" "${RELEASE_DIR}/bin/SpineWallpaperCore.exe")
run_cmake_command(remove_directory "${RELEASE_DIR}/SPINE")
run_cmake_command(remove "${RELEASE_DIR}/SpineWallpaper.ini.example")
run_cmake_command(copy_if_different
    "${SOURCE_CONFIG}" "${RELEASE_DIR}/SpineWallpaper.ini")
