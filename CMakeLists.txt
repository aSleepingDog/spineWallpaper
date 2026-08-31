project(SpineSFML)
cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 23)
#
# First download and extract SFML 2.3.2 for the respective OS we are on
#
set(DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/dependencies/")
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(SFML_URL "http://www.sfml-dev.org/files/SFML-2.4.1-osx-clang.tar.gz")
    set(SFML_DIR ${DEPS_DIR}/SFML-2.4.1-osx-clang)
	if (NOT EXISTS "${SFML_DIR}")
		message("Downloading SFML for Mac OS X")
		file(DOWNLOAD "${SFML_URL}" "${DEPS_DIR}/sfml.tar.gz")
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar xzf  ${DEPS_DIR}/sfml.tar.gz
			WORKING_DIRECTORY ${DEPS_DIR}
    	)
		# copy freetype over to Frameworks/ so rpath resoultion works
		execute_process(
				COMMAND ${CMAKE_COMMAND} -E copy_directory ${SFML_DIR}/extlibs/freetype.framework ${SFML_DIR}/Frameworks/freetype.framework
				WORKING_DIRECTORY ${SFML_DIR}
		)
	endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(SFML_URL "http://www.sfml-dev.org/files/SFML-2.4.1-linux-gcc-64-bit.tar.gz")
    set(SFML_DIR ${DEPS_DIR}/SFML-2.4.1)
	if (NOT EXISTS ${SFML_DIR})
		message("Downloading SFML for Linux 64-bit")
		file(DOWNLOAD "${SFML_URL}" "${DEPS_DIR}/sfml.tar.gz")
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar xzf  ${DEPS_DIR}/sfml.tar.gz
			WORKING_DIRECTORY ${DEPS_DIR}
    	)
	endif()
else()
	# 这里可能得自己下
	set(SFML_URL "http://www.sfml-dev.org/files/SFML-2.4.1-windows-vc14-32-bit.zip")
	set(SFML_DIR ${DEPS_DIR}/SFML-2.4.1)
	if (NOT EXISTS ${SFML_DIR})
		message("Downloading SFML for Windows 32-bit")
		# 你网络好就CMake给你下
		#file(DOWNLOAD "${SFML_URL}" "${DEPS_DIR}/sfml.zip")
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar x  ${DEPS_DIR}/sfml.zip
			WORKING_DIRECTORY ${DEPS_DIR}
    	)
	endif()
endif()

if(MSVC)
	message("MSCV detected")
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_CRT_SECURE_NO_WARNINGS")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS")
else()
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -std=c89")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wnon-virtual-dtor -pedantic -std=c++11 -fno-exceptions -fno-rtti")
endif()

# Define spine-sfml-cpp library
include_directories(spine-sfml ${SFML_DIR}/include)
include_directories(spine-sfml ${CMAKE_SOURCE_DIR}/spine-cpp/include)
file(GLOB INCLUDES "spine-sfml/**/*.h")
file(GLOB SOURCES "spine-sfml/**/*.cpp")
add_library(spine-sfml-cpp STATIC ${SOURCES} ${INCLUDES})
target_link_libraries(spine-sfml-cpp LINK_PUBLIC spine-cpp)
install(TARGETS spine-sfml-cpp DESTINATION dist/lib)
install(FILES ${INCLUDES} DESTINATION dist/include)


add_executable(spineWallpaper 
	src/main.cpp
	src/json.h
	src/json.cpp
)

# Link in SFML libraries and OS dependencies like OpenGL
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_library(SFML SFML PATHS ${SFML_DIR}/Frameworks)
	find_library(SFML_SYSTEM "sfml-system" PATHS ${SFML_DIR}/Frameworks)
	find_library(SFML_WINDOW sfml-window PATHS ${SFML_DIR}/Frameworks)
	find_library(SFML_GRAPHICS sfml-graphics PATHS ${SFML_DIR}/Frameworks)
	target_link_libraries(spineWallpaper ${SFML} ${SFML_SYSTEM} ${SFML_WINDOW} ${SFML_GRAPHICS})
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	target_link_libraries(spineWallpaper sfml-graphics sfml-window sfml-system)
else()
	set(SFML_LIBS ${SFML_DIR}/lib)

	if(CMAKE_CONFIGURATION_TYPES)   # 多配置生成器（VS、Xcode）
		message(STATUS "多配置生成器，可用构建类型: ${CMAKE_CONFIGURATION_TYPES}")
		target_link_libraries(spineWallpaper $<$<CONFIG:Debug>:${SFML_LIBS}/sfml-main-d.lib> $<$<CONFIG:Release>:${SFML_LIBS}/sfml-main.lib>)
		target_link_libraries(spineWallpaper $<$<CONFIG:Debug>:${SFML_LIBS}/sfml-graphics-s-d.lib> $<$<CONFIG:Release>:${SFML_LIBS}/sfml-graphics-s.lib>)
		target_link_libraries(spineWallpaper $<$<CONFIG:Debug>:${SFML_LIBS}/sfml-window-s-d.lib> $<$<CONFIG:Release>:${SFML_LIBS}//sfml-window-s.lib>)
		target_link_libraries(spineWallpaper $<$<CONFIG:Debug>:${SFML_LIBS}/sfml-system-s-d.lib> $<$<CONFIG:Release>:${SFML_LIBS}/sfml-system-s.lib>)
		target_link_options(spineWallpaper PRIVATE $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>)
	else()
		message(STATUS "单配置生成器，构建类型: ${CMAKE_BUILD_TYPE}")
		
		if(CMAKE_BUILD_TYPE STREQUAL "Release")
			target_link_libraries(spineWallpaper ${SFML_LIBS}/sfml-main.lib)
			target_link_libraries(spineWallpaper ${SFML_LIBS}/sfml-graphics-s.lib)
			target_link_libraries(spineWallpaper ${SFML_LIBS}/sfml-window-s.lib)
			target_link_libraries(spineWallpaper ${SFML_LIBS}/sfml-system-s.lib)
			target_link_options(spineWallpaper PRIVATE /SUBSYSTEM:WINDOWS)
		else()
			target_link_libraries(spineWallpaper ${SFML_LIBS}/sfml-main-d.lib)
			target_link_libraries(spineWallpaper ${SFML_LIBS}/sfml-graphics-s-d.lib)
			target_link_libraries(spineWallpaper ${SFML_LIBS}/sfml-window-s-d.lib)
			target_link_libraries(spineWallpaper ${SFML_LIBS}/sfml-system-s-d.lib)
		endif()
	endif()

	target_link_libraries(spineWallpaper ${SFML_LIBS}/freetype.lib)
	target_link_libraries(spineWallpaper ${SFML_LIBS}/jpeg.lib)
	target_link_libraries(spineWallpaper opengl32)
	target_link_libraries(spineWallpaper gdi32)
	target_link_libraries(spineWallpaper winmm)
	add_definitions(-DSFML_STATIC)
endif()


if(MSVC)
	message("MSCV detected")
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	set (CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS}")
else()
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -std=c89")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wnon-virtual-dtor -pedantic -std=c++03 -fno-exceptions -fno-rtti")
endif()

include_directories(include)
file(GLOB INCLUDES "spine-cpp/include/**/*.h")
file(GLOB SOURCES "spine-cpp/src/**/*.cpp")

add_library(spine-cpp STATIC ${SOURCES} ${INCLUDES})
target_include_directories(spine-cpp PUBLIC spine-cpp/include)
install(TARGETS spine-cpp DESTINATION dist/lib)
install(FILES ${INCLUDES} DESTINATION dist/include)

target_link_libraries(spineWallpaper spine-cpp)
target_link_libraries(spineWallpaper spine-sfml-cpp)
