# Add source to this project's executable.
cmake_minimum_required (VERSION 3.10)

file(GLOB_RECURSE SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

set(EXCLUDE_DIR_1 ${CMAKE_CURRENT_SOURCE_DIR}/submodules/JoltPhysics ${CMAKE_CURRENT_SOURCE_DIR}/submodules/ecss)
# file(GLOB_RECURSE SUBMODULES_FILES "${CMAKE_CURRENT_SOURCE_DIR}/submodules/*")
# list(REMOVE_ITEM SRC ${SUBMODULES_FILES})
# file(GLOB_RECURSE SUBMODULES_FILES "${CMAKE_CURRENT_SOURCE_DIR}/submodules/ecss/*")
# list(REMOVE_ITEM SRC ${SUBMODULES_FILES})

foreach(EXCLUDE_DIR ${EXCLUDE_DIR_1})
   	file(GLOB_RECURSE EXCLUDE_FILES ${EXCLUDE_DIR}/*)
    list(REMOVE_ITEM SRC ${EXCLUDE_FILES})
endforeach()

list(REMOVE_ITEM SRC
	"${CMAKE_CURRENT_SOURCE_DIR}/core/main.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/runtime/main.cpp"
)
list(APPEND SRC "${CMAKE_CURRENT_SOURCE_DIR}/assetsModule/modelModule/mikktspace.c")

if (EDITOR)
	message("-- engine with editor")
else()
	message("-- engine without editor")
endif()

message("-- engine as library")
add_library(${ENGINE_NAME} STATIC ${SRC})

if(EDITOR)
	target_compile_definitions(${ENGINE_NAME} PRIVATE EDITOR_ENABLED)
endif()

add_executable(StelForgeRuntime "${CMAKE_CURRENT_SOURCE_DIR}/runtime/main.cpp")
target_link_libraries(StelForgeRuntime PRIVATE ${ENGINE_NAME})
target_include_directories(StelForgeRuntime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(StelForgeRuntime PROPERTIES
	FOLDER Engine
	CXX_STANDARD 23
	VS_DEBUGGER_WORKING_DIRECTORY "${ENGINE_PATH}/bin"
	WORKING_DIRECTORY "${ENGINE_PATH}/bin"
)
if(MSVC)
	set_target_properties(StelForgeRuntime PROPERTIES
		LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
	)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

add_subdirectory(submodules/ecss)
target_link_libraries(${ENGINE_NAME} PUBLIC ecss)

# ecss is header-only and templated, so every translation unit that opens a view has to
# parse the whole thing, and only a fifth of that is ecss's own code -- the rest is the
# <vector>/<atomic>/<algorithm>/<memory> closure underneath it. Thirty-one files here
# include it, most of them transitively through core/ECSHandler.h.
#
# Worth 32% of compiler front-end time over a six-file sample, and not a net loss on any
# single file in it, including files that never name ecss themselves. Re-check with
# scripts/measure-frontend.ps1 -- and note why that script exists rather than a stopwatch
# on a full build: see the warning in the top-level CMakeLists.txt.
#
# Angle brackets so it is looked up on the include path; a bare name would be taken as a
# path relative to this directory. mikktspace.c opts out rather than the header being
# wrapped in a COMPILE_LANGUAGE genex, because the '>' of <ecss/Registry.h> would close the
# genex early -- and a C compile cannot consume a C++ precompiled header either way.
target_precompile_headers(${ENGINE_NAME} PRIVATE <ecss/Registry.h>)
set_source_files_properties(
	"${CMAKE_CURRENT_SOURCE_DIR}/assetsModule/modelModule/mikktspace.c"
	PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

target_include_directories(${ENGINE_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/submodules/myPhysicsEngine")
#target_include_directories(${ENGINE_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/submodules/JoltPhysics")
target_include_directories(${ENGINE_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/jsoncpp/include")
target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/glm")
target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/stb")
target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/vma")

#target_link_libraries(${ENGINE_NAME} LINK_PUBLIC Jolt)

find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
if(MSVC AND TARGET Vulkan::shaderc_combined)
	set_property(TARGET Vulkan::shaderc_combined PROPERTY MAP_IMPORTED_CONFIG_DEBUG Release)
	set_property(TARGET Vulkan::shaderc_combined PROPERTY MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
endif()

target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/glfw/include")
target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/assimp/include")
target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/lib/freetype/include")


if(Vulkan_INCLUDE_DIRS)
    target_include_directories(${ENGINE_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
elseif(Vulkan_INCLUDE_DIR)
    target_include_directories(${ENGINE_NAME} PUBLIC ${Vulkan_INCLUDE_DIR})
endif()

target_link_libraries(${ENGINE_NAME} 
	PRIVATE 
		Vulkan::Vulkan
		Vulkan::shaderc_combined
	PUBLIC
		glad
		imgui
		OpenGL::GL
	)

# Shared third-party DLLs: IMPORTED location is the source of truth.
# CMake stages them into out/<Config>/ThirdParty; Windows has no rpath.
set(SFE_NATIVE_SHARED_LIBS "")
if(MSVC)
	add_library(sfe_assimp SHARED IMPORTED GLOBAL)
	set_target_properties(sfe_assimp PROPERTIES
		IMPORTED_IMPLIB "${ENGINE_PATH}/lib/assimp/Release/assimp-vc143-mt.lib"
		IMPORTED_LOCATION "${ENGINE_PATH}/lib/assimp/Release/assimp-vc143-mt.dll"
	)
	add_library(sfe_jsoncpp SHARED IMPORTED GLOBAL)
	set_target_properties(sfe_jsoncpp PROPERTIES
		IMPORTED_IMPLIB "${ENGINE_PATH}/lib/jsoncpp/lib/release/jsoncpp.lib"
		IMPORTED_LOCATION "${ENGINE_PATH}/lib/jsoncpp/lib/release/jsoncpp.dll"
	)
	set(SFE_NATIVE_SHARED_LIBS sfe_assimp sfe_jsoncpp)
	if(VLD)
		add_library(sfe_vld SHARED IMPORTED GLOBAL)
		set_target_properties(sfe_vld PROPERTIES
			IMPORTED_IMPLIB "${ENGINE_PATH}/utils/VisualLeakDetector/lib/Win64/vld.lib"
			IMPORTED_LOCATION "${ENGINE_PATH}/utils/VisualLeakDetector/bin/Win64/vld_x64.dll"
		)
		list(APPEND SFE_NATIVE_SHARED_LIBS sfe_vld)
		target_include_directories(${ENGINE_NAME} PUBLIC "${ENGINE_PATH}/utils/VisualLeakDetector/include")
		target_compile_definitions(${ENGINE_NAME} PUBLIC VLD=true)
		target_link_libraries(${ENGINE_NAME} PUBLIC sfe_vld)
		message(STATUS "visual leak detector: ON")
	endif()
	set(SFE_NATIVE_SHARED_LIBS "${SFE_NATIVE_SHARED_LIBS}" CACHE INTERNAL "Imported native shared libs")
endif()

macro(sfe_stage_app tgt)
	set_target_properties(${tgt} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${SFE_STAGE_ROOT}/${tgt}/$<CONFIG>"
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${SFE_STAGE_ROOT}/${tgt}/Debug"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${SFE_STAGE_ROOT}/${tgt}/Release"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${SFE_STAGE_ROOT}/${tgt}/RelWithDebInfo"
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${SFE_STAGE_ROOT}/${tgt}/MinSizeRel"
	)
	if(MSVC AND SFE_NATIVE_SHARED_LIBS)
		foreach(_lib ${SFE_NATIVE_SHARED_LIBS})
			target_link_options(${tgt} PRIVATE "/DELAYLOAD:$<TARGET_FILE_NAME:${_lib}>")
		endforeach()
		target_link_libraries(${tgt} PRIVATE delayimp)
		set(_dlls)
		foreach(_lib ${SFE_NATIVE_SHARED_LIBS})
			list(APPEND _dlls "$<TARGET_FILE:${_lib}>")
		endforeach()
		add_custom_command(TARGET ${tgt} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${tgt}>/ThirdParty"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_dlls} "$<TARGET_FILE_DIR:${tgt}>/ThirdParty/"
			COMMENT "Stage ThirdParty DLLs for ${tgt}"
		)
		if(TARGET sfe_vld)
			add_custom_command(TARGET ${tgt} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
					"${ENGINE_PATH}/utils/VisualLeakDetector/bin/Win64/dbghelp.dll"
					"${ENGINE_PATH}/utils/VisualLeakDetector/bin/Win64/Microsoft.DTfW.DHL.manifest"
					"$<TARGET_FILE_DIR:${tgt}>/ThirdParty/"
			)
		endif()
	endif()
endmacro()
sfe_stage_app(StelForgeRuntime)

if(MSVC)
	# Add PDH for SystemLoadHelper + Windows version/UNICODE defines
	target_compile_definitions(${ENGINE_NAME} PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)

	target_link_libraries(${ENGINE_NAME}
	PUBLIC
		"${ENGINE_PATH}/lib/glfw/glfw3.lib"
		ws2_32
		pdh
		"${ENGINE_PATH}/lib/freetype/lib/win/release/freetype.lib"
		sfe_jsoncpp
		sfe_assimp
		MSVCRT
	)

	set(CMAKE_EXE_LINKER_FLAGS "/NODEFAULTLIB:MSVCRT")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
elseif(XCODE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
	add_definitions(-std=c++20)

	target_link_libraries(${ENGINE_NAME}
	PUBLIC
		${ENGINE_PATH}/lib/glfw/lib-arm64/libglfw.3.dylib
		${ENGINE_PATH}/lib/glfw/lib-arm64/libglfw3.a
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


# RHI conformance suite. Off by default: it fetches googletest and needs a GPU, neither of which
# an ordinary build of the engine should pay for. Same shape as the ecss suite next door.
option(SFE_BUILD_RHI_TESTS "Build the RHI conformance tests" OFF)

if(SFE_BUILD_RHI_TESTS)
	include(CTest)
	include(FetchContent)
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_Declare(
		googletest
		URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
		UPDATE_DISCONNECTED TRUE
		DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	)
	set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
	set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)

	file(GLOB RHI_TEST_SOURCES CONFIGURE_DEPENDS "${ENGINE_PATH}/tests/*.cpp")
	add_executable(rhi_tests ${RHI_TEST_SOURCES})
	target_include_directories(rhi_tests PRIVATE "${ENGINE_PATH}/tests")
	target_link_libraries(rhi_tests PRIVATE ${ENGINE_NAME} gtest)
	# A console subsystem, unlike the editor: the suite reports through stdout.
	if(MSVC)
		set_target_properties(rhi_tests PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
	endif()
	# Staged like any other app here, so the native DLLs are delay-loaded and copied next to the
	# exe. Without it the loader cannot resolve assimp or jsoncpp and the suite dies at startup
	# with 0xC0000135, before a single test runs.
	sfe_stage_app(rhi_tests)
	# The tests load their shaders through relative paths, so they only run from the asset root.
	set_target_properties(rhi_tests PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${ENGINE_PATH}/bin")
	add_test(NAME rhi_tests COMMAND rhi_tests WORKING_DIRECTORY "${ENGINE_PATH}/bin")
endif()
