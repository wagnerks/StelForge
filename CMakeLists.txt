cmake_minimum_required (VERSION 3.10)

project ("StelForgeEngine")

if(MSVC)
	foreach(_lang C CXX)
		string(REPLACE "/MDd" "/MD" CMAKE_${_lang}_FLAGS_DEBUG "${CMAKE_${_lang}_FLAGS_DEBUG}")
		string(REPLACE "/MTd" "/MT" CMAKE_${_lang}_FLAGS_DEBUG "${CMAKE_${_lang}_FLAGS_DEBUG}")
	endforeach()
endif()

set(ENGINE_NAME StelForgeEngine)
set(ENGINE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT SFE_STAGE_ROOT)
	set(SFE_STAGE_ROOT "${ENGINE_PATH}/out")
endif()

set(XCODE FALSE)
if(CMAKE_GENERATOR STREQUAL Xcode)
	set(XCODE TRUE)
endif()

add_subdirectory(lib/glad/)
add_subdirectory(lib/imgui/)

add_subdirectory(src)
#add_subdirectory(src/submodules/JoltPhysics/Build)

set_property(TARGET ${ENGINE_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${ENGINE_PATH}/bin)
set_property(TARGET ${ENGINE_NAME} PROPERTY WORKING_DIRECTORY ${ENGINE_PATH}/bin)
set_property(TARGET ${ENGINE_NAME} PROPERTY CXX_STANDARD 23)

if(XCODE)
	set_target_properties(${ENGINE_NAME} PROPERTIES
		XCODE_ATTRIBUTE_CONFIGURATION_BUILD_DIR "${ENGINE_PATH}/bin")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set_target_properties(glad imgui PROPERTIES FOLDER Dependencies)

set_target_properties(${ENGINE_NAME} PROPERTIES
	FOLDER Engine
	DEBUG_POSTFIX "-d"
)

#set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${CMAKE_PROJECT_NAME})