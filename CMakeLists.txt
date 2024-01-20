cmake_minimum_required (VERSION 3.8)

project ("MyGameEngine")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

add_subdirectory(lib/glad/)
add_subdirectory(lib/imgui/)


add_subdirectory(src)
add_subdirectory(src/submodules/JoltPhysics/Build)

set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set_target_properties(glad imgui Jolt PROPERTIES FOLDER Dependencies)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES FOLDER AppTools)

#set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "HelloWorld")