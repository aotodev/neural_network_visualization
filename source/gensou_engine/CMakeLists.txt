project(gensou VERSION 1.0.0 LANGUAGES CXX)
message("Building ${PROJECT_NAME}")

# ---------------------------------------------------------------------------------------
# Add Lib
# ---------------------------------------------------------------------------------------
file(GLOB_RECURSE BaseSources "src/*.cpp")
file(GLOB_RECURSE box2dSources "deps/box2d/src/*.cpp")

set(DependenciesSources
	${box2dSources}
	deps/miniaudio/miniaudio.cpp
	deps/stb/stb_image.cpp 
	deps/stb/stb_truetype.cpp
	deps/entt/entt/entt.hpp 
	deps/VulkanMemoryAllocator/vk_mem_alloc.cpp
)

if(ANDROID)	
	add_library(
		AndroidNative
		STATIC
		${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
	)
endif()

add_library(
	${PROJECT_NAME}
	STATIC
	${BaseSources}
	${DependenciesSources}
)

target_include_directories(${PROJECT_NAME} PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/res"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE SHADERS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/res/shaders")
target_compile_definitions(${PROJECT_NAME} PUBLIC GAME_NAME="${CMAKE_PROJECT_NAME}")

if(NOT ANDROID) #android performs this task from build.gradle
	file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/engine_res)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/res/shaders DESTINATION ${EXECUTABLE_OUTPUT_PATH}/engine_res)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/res/textures DESTINATION ${EXECUTABLE_OUTPUT_PATH}/engine_res)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/res/fonts DESTINATION ${EXECUTABLE_OUTPUT_PATH}/engine_res)
endif()

# PCH
target_precompile_headers(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/gspch.h")

# ---------------------------------------------------------------------------------------
# Find and link Vulkan
# ---------------------------------------------------------------------------------------
if(ANDROID)
	target_link_libraries(${PROJECT_NAME} vulkan)
	#target_include_directories(${PROJECT_NAME} PRIVATE ${VULKAN_INCLUDE_DIRS})
else()
	message(STATUS "Attempting to find Vulkan...")
	find_package(Vulkan REQUIRED)

	if(NOT Vulkan_FOUND)
		message(FATAL_ERROR "Vulkan SDK not found, make sure that it's installed and the VULKAN_SDK environment variable is properly set")
	else()
		message("...Found")
	endif()

	target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
	target_include_directories(${PROJECT_NAME} PRIVATE ${VULKAN_INCLUDE_DIRS})
endif()

# ---------------------------------------------------------------------------------------
# ThirdParty/dependencies
# ---------------------------------------------------------------------------------------
set(DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")

if(ANDROID)
	target_link_libraries(${PROJECT_NAME} android AndroidNative log)
	target_include_directories(${PROJECT_NAME} PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)

else()
	#GLFW
	set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

	if(USE_WAYLAND)
		set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
		set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
	endif()

	add_subdirectory(deps/glfw)
	target_link_libraries(${PROJECT_NAME} glfw)
	target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/glfw/include")

	#spdlog
	if(NOT BUILD_SHIPPING)
		add_subdirectory(deps/spdlog)
		target_link_libraries(${PROJECT_NAME} spdlog)
		target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/spdlog/include")
	endif()
endif()

#box2d
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/box2d/src")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/box2d/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps/box2d/src")

#VMA
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/VulkanMemoryAllocator")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/VulkanMemoryAllocator")

#KTX
add_compile_definitions(KHRONOS_STATIC)
add_subdirectory(deps/ktx)
target_link_libraries(${PROJECT_NAME} KtxSoftware)
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/ktx/include")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/ktx/other_include")

#stb
target_link_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps/stb")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps/stb")

#glm
add_subdirectory(deps/glm)
target_link_libraries(${PROJECT_NAME} glm)
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/glm")

#entt
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/entt")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/entt")

#miniaudio
add_compile_definitions(MA_NO_RESOURCE_MANAGER)
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/miniaudio")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/miniaudio")