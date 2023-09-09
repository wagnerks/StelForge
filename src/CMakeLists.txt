# Add source to this project's executable.
cmake_minimum_required (VERSION 3.8)

file(GLOB_RECURSE SRC
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
	"${CMAKE_SOURCE_DIR}/src/*.cc"
)

add_executable(${CMAKE_PROJECT_NAME} ${SRC})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/lib/jsoncpp/include")
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/lib/glm")
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/lib/stb")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD 20)
endif()

find_package(OpenGL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME}
	PUBLIC
	glad
	imgui
	"${CMAKE_SOURCE_DIR}/lib/glfw/glfw3.lib"
	OpenGL::GL
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

include_directories("${CMAKE_SOURCE_DIR}/lib/glfw/include")
include_directories("${CMAKE_SOURCE_DIR}/lib/assimp/include")

set(CMAKE_EXE_LINKER_FLAGS "/NODEFAULTLIB:MSVCRT")
if(MSVC)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()


add_custom_command(TARGET ${CMAKE_PROJECT_NAME}
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/assimp/Release/assimp-vc143-mt.dll ${CMAKE_SOURCE_DIR}/.
COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/assimp/Debug/assimp-vc143-mtd.dll ${CMAKE_SOURCE_DIR}/.
COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/jsoncpp/lib/debug/jsoncpp.dll ${CMAKE_SOURCE_DIR}/.
)

