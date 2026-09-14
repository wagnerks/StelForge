# The render hardware interface: the interface and its OpenGL and Vulkan backends, with no dependency on
# engine code. Its include root is the engine directory, where only rhi/ holds sources: an include of
# core/, renderModule/ or any other engine module does not resolve here, so the module cannot grow a
# dependency on the engine without the build failing.

file(GLOB SFE_RHI_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(sfe_rhi STATIC ${SFE_RHI_SOURCES})
set_target_properties(sfe_rhi PROPERTIES CXX_STANDARD 23 FOLDER Engine)

target_include_directories(sfe_rhi
	PUBLIC
		"${ENGINE_PATH}"
		"${ENGINE_PATH}/lib/glfw/include"
	PRIVATE
		"${ENGINE_PATH}/lib/vma"
)

find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
if(MSVC AND TARGET Vulkan::shaderc_combined)
	set_property(TARGET Vulkan::shaderc_combined PROPERTY MAP_IMPORTED_CONFIG_DEBUG Release)
	set_property(TARGET Vulkan::shaderc_combined PROPERTY MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
endif()

target_link_libraries(sfe_rhi
	PRIVATE
		Vulkan::Vulkan
		Vulkan::shaderc_combined
		glad
		OpenGL::GL
)

if(MSVC)
	target_link_libraries(sfe_rhi PUBLIC "${ENGINE_PATH}/lib/glfw/glfw3.lib")
elseif(XCODE)
	target_link_libraries(sfe_rhi PUBLIC
		${ENGINE_PATH}/lib/glfw/lib-arm64/libglfw.3.dylib
		${ENGINE_PATH}/lib/glfw/lib-arm64/libglfw3.a
	)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SFE_RHI_SOURCES})
