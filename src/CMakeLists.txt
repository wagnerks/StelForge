# Add source to this project's executable.
cmake_minimum_required (VERSION 3.8)

file(GLOB_RECURSE SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

set(EXCLUDE_DIR_1 ${CMAKE_CURRENT_SOURCE_DIR}/submodules/JoltPhysics)

foreach(EXCLUDE_DIR ${EXCLUDE_DIR_1})
    file(GLOB_RECURSE EXCLUDE_FILES ${EXCLUDE_DIR}/*.cpp ${EXCLUDE_DIR}/*.h ${EXCLUDE_DIR}/*.hpp)
    list(REMOVE_ITEM SRC ${EXCLUDE_FILES})
endforeach()

if (EDITOR)
	message("-- engine with editor")
	add_library(${ENGINE_NAME} SHARED ${SRC})
else()
	message("-- engine without editor")
	add_executable(${ENGINE_NAME} ${SRC})
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

target_include_directories(${ENGINE_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/submodules")
target_include_directories(${ENGINE_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/submodules/JoltPhysics")
target_include_directories(${ENGINE_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/jsoncpp/include")
target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/glm")
target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/stb")

target_link_libraries(${ENGINE_NAME} LINK_PUBLIC Jolt)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${ENGINE_NAME} PROPERTY CXX_STANDARD 20)
endif()

find_package(OpenGL REQUIRED)

target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/glfw/include")
target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/assimp/include")
target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/freetype/include")

if(MSVC)
	target_link_libraries(${ENGINE_NAME}
		PUBLIC
		glad
		imgui
		"${ENGINE_PATH}/lib/glfw/glfw3.lib"
		OpenGL::GL
	debug
		"${ENGINE_PATH}/lib/freetype/lib/win/debug/freetyped.lib"
	optimized
		"${ENGINE_PATH}/lib/freetype/lib/win/release/freetype.lib"
	debug
		"${ENGINE_PATH}/lib/jsoncpp/lib/debug/jsoncpp.lib"
	optimized
		"${ENGINE_PATH}/lib/jsoncpp/lib/release/jsoncpp.lib"
	debug
		"${ENGINE_PATH}/lib/assimp/Debug/assimp-vc143-mtd.lib"
	optimized
		"${ENGINE_PATH}/lib/assimp/Release/assimp-vc143-mt.lib"
	optimized
		MSVCRT
	)
	set(CMAKE_EXE_LINKER_FLAGS "/NODEFAULTLIB:MSVCRT")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

	add_custom_command(TARGET ${ENGINE_NAME}
	POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_PATH}/lib/assimp/Release/assimp-vc143-mt.dll ${ENGINE_PATH}/.
		COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_PATH}/lib/assimp/Debug/assimp-vc143-mtd.dll ${ENGINE_PATH}/.
		COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_PATH}/lib/jsoncpp/lib/debug/jsoncpp_d.dll ${ENGINE_PATH}/.
		COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_PATH}/lib/jsoncpp/lib/release/jsoncpp.dll ${ENGINE_PATH}/.
	)
elseif(XCODE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
	add_definitions(-std=c++20)

	target_link_libraries(${ENGINE_NAME}
		PUBLIC
		imgui
		glad
		${ENGINE_PATH}/lib/glfw/lib-arm64/libglfw.3.dylib
		${ENGINE_PATH}/lib/glfw/lib-arm64/libglfw3.a
		
		OpenGL::GL
		pthread
	debug
		${ENGINE_PATH}/lib/jsoncpp/lib/macos/debug/libjsoncpp.dylib
	optimized
		${ENGINE_PATH}/lib/jsoncpp/lib/macos/release/libjsoncpp.dylib
	debug
		${ENGINE_PATH}/lib/assimp/macos/debug/libassimpd.dylib
	optimized
		${ENGINE_PATH}/lib/assimp/macos/release/libassimp.dylib
	)
endif()

if(VLD)
	include_directories("${ENGINE_PATH}/utils/VisualLeakDetector/include")
	target_link_libraries(${ENGINE_NAME}
		PUBLIC
		"${ENGINE_PATH}/utils/VisualLeakDetector/lib/Win64/vld.lib"
	)
	add_custom_command(TARGET ${ENGINE_NAME}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_PATH}/utils/VisualLeakDetector/bin/Win64/dbghelp.dll ${ENGINE_PATH}/.
	COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_PATH}/utils/VisualLeakDetector/bin/Win64/Microsoft.DTfW.DHL.manifest ${ENGINE_PATH}/.
	COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_PATH}/utils/VisualLeakDetector/bin/Win64/vld_x64.dll ${ENGINE_PATH}/.
	)
	add_definitions(-DVLD=true)

	message(visual leak detector : ON)
endif()
