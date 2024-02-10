# Add source to this project's executable.
cmake_minimum_required (VERSION 3.8)

file(GLOB_RECURSE SRC
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/src/*.hpp"
	"${CMAKE_SOURCE_DIR}/src/*.cc"
)

set(EXCLUDE_DIR_1 ${CMAKE_SOURCE_DIR}/src/submodules/JoltPhysics)

foreach(EXCLUDE_DIR ${EXCLUDE_DIR_1})
    file(GLOB_RECURSE EXCLUDE_FILES ${EXCLUDE_DIR}/*.cpp ${EXCLUDE_DIR}/*.h ${EXCLUDE_DIR}/*.hpp)
    list(REMOVE_ITEM SRC ${EXCLUDE_FILES})
endforeach()

add_executable(${CMAKE_PROJECT_NAME} ${SRC})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/src/submodules")
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/src/submodules/JoltPhysics")
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/lib/jsoncpp/include")
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/lib/glm")
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/lib/stb")

target_link_libraries(${CMAKE_PROJECT_NAME} LINK_PUBLIC Jolt)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD 20)
endif()

find_package(OpenGL REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}/lib/glfw/include")
include_directories("${CMAKE_SOURCE_DIR}/lib/assimp/include")
include_directories("${CMAKE_SOURCE_DIR}/lib/freetype/include")

if(MSVC)
	target_link_libraries(${CMAKE_PROJECT_NAME}
		PUBLIC
		glad
		imgui
		"${CMAKE_SOURCE_DIR}/lib/glfw/glfw3.lib"
		OpenGL::GL
	debug
		"${CMAKE_SOURCE_DIR}/lib/freetype/lib/win/debug/freetyped.lib"
	optimized
		"${CMAKE_SOURCE_DIR}/lib/freetype/lib/win/release/freetype.lib"
	debug
		"${CMAKE_SOURCE_DIR}/lib/jsoncpp/lib/debug/jsoncpp.lib"
	optimized
		"${CMAKE_SOURCE_DIR}/lib/jsoncpp/lib/release/jsoncpp.lib"
	debug
		"${CMAKE_SOURCE_DIR}/lib/assimp/Debug/assimp-vc143-mtd.lib"
	optimized
		"${CMAKE_SOURCE_DIR}/lib/assimp/Release/assimp-vc143-mt.lib"
	optimized
		MSVCRT
	)
	set(CMAKE_EXE_LINKER_FLAGS "/NODEFAULTLIB:MSVCRT")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

	add_custom_command(TARGET ${CMAKE_PROJECT_NAME}
	POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/assimp/Release/assimp-vc143-mt.dll ${CMAKE_SOURCE_DIR}/.
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/assimp/Debug/assimp-vc143-mtd.dll ${CMAKE_SOURCE_DIR}/.
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/jsoncpp/lib/debug/jsoncpp_d.dll ${CMAKE_SOURCE_DIR}/.
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/jsoncpp/lib/release/jsoncpp.dll ${CMAKE_SOURCE_DIR}/.
	)
elseif(XCODE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
	add_definitions(-std=c++20)

	target_link_libraries(${CMAKE_PROJECT_NAME}
		PUBLIC
		imgui
		glad
		${CMAKE_SOURCE_DIR}/lib/glfw/lib-arm64/libglfw.3.dylib
		${CMAKE_SOURCE_DIR}/lib/glfw/lib-arm64/libglfw3.a
		
		OpenGL::GL
		pthread
	debug
		${CMAKE_SOURCE_DIR}/lib/jsoncpp/lib/macos/debug/libjsoncpp.dylib
	optimized
		${CMAKE_SOURCE_DIR}/lib/jsoncpp/lib/macos/release/libjsoncpp.dylib
	debug
		${CMAKE_SOURCE_DIR}/lib/assimp/macos/debug/libassimpd.dylib
	optimized
		${CMAKE_SOURCE_DIR}/lib/assimp/macos/release/libassimp.dylib
	)
endif()

if(VLD)
	include_directories("${CMAKE_SOURCE_DIR}/utils/VisualLeakDetector/include")
	target_link_libraries(${CMAKE_PROJECT_NAME}
		PUBLIC
		"${CMAKE_SOURCE_DIR}/utils/VisualLeakDetector/lib/Win64/vld.lib"
	)
	add_custom_command(TARGET ${CMAKE_PROJECT_NAME}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/utils/VisualLeakDetector/bin/Win64/dbghelp.dll ${CMAKE_SOURCE_DIR}/.
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/utils/VisualLeakDetector/bin/Win64/Microsoft.DTfW.DHL.manifest ${CMAKE_SOURCE_DIR}/.
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/utils/VisualLeakDetector/bin/Win64/vld_x64.dll ${CMAKE_SOURCE_DIR}/.
	)
	add_definitions(-DVLD=true)

	message(visual leak detector : ON)
endif()
