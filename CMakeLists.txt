cmake_minimum_required(VERSION 3.22.1 FATAL_ERROR)

include(CMakeDependentOption)

project(neural_network_visualization VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ---------------------------------------------------------------------------------------
# Build type & definitions
# ---------------------------------------------------------------------------------------
option(BUILD_DEBUG "debug" ON)
option(BUILD_RELEASE "release" OFF)
option(BUILD_SHIPPING "shipping" OFF)

option(COMPRESS_ASTC "AstcCompressionForAndroid" ON)
option(INVERT_VIEWPORT_Y "InvertViewport" OFF)
option(VULKAN_GLSL_1_2 "VulkanGlslVersion" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
	set(BASE_BIN_PATH "${CMAKE_SOURCE_DIR}/build/windows")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(BASE_BIN_PATH "${CMAKE_SOURCE_DIR}/build/linux")
elseif(ANDROID)
	set(BASE_BIN_PATH "${CMAKE_SOURCE_DIR}/build/android/app/build/outputs/libs/${ANDROID_ABI}")
endif()

macro(set_outputs_macro buildType)
	set(LIBRARY_OUTPUT_PATH "${BASE_BIN_PATH}/${buildType}")
	set(EXECUTABLE_OUTPUT_PATH "${BASE_BIN_PATH}/${buildType}")

	# MSVC appends Debug/Release folders at the end of the specified path
	if(MSVC)
		foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
			string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
			set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${BASE_BIN_PATH}/${buildType}" )
			set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${BASE_BIN_PATH}/${buildType}" )
			set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${BASE_BIN_PATH}/${buildType}" )
		endforeach()
	endif()

	message(STATUS "Building ${CMAKE_PROJECT_NAME} in ${buildType} Mode")
endmacro()

if(BUILD_DEBUG)
	add_compile_definitions(APP_DEBUG)
	set(CMAKE_BUILD_TYPE Debug)
	set_outputs_macro(debug)

elseif(BUILD_RELEASE)
	add_compile_definitions(APP_RELEASE)
	add_compile_definitions(NDEBUG)
	set(CMAKE_BUILD_TYPE Release)
	set_outputs_macro(release)

elseif(BUILD_SHIPPING)
	add_compile_definitions(APP_SHIPPING)
	add_compile_definitions(NDEBUG)
	set(CMAKE_BUILD_TYPE Release)
	set_outputs_macro(shipping)

endif()

file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/save/backup)
file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/resources)
file(COPY ${CMAKE_SOURCE_DIR}/resources/ DESTINATION ${EXECUTABLE_OUTPUT_PATH}/resources
	PATTERN "windows" EXCLUDE)

if(WIN32)
	file(COPY ${CMAKE_SOURCE_DIR}/resources/windows DESTINATION ${EXECUTABLE_OUTPUT_PATH}/resources)
endif()


if(ANDROID AND COMPRESS_ASTC)
	add_compile_definitions(USE_ASTC=1)
else()
	add_compile_definitions(USE_ASTC=0)
endif()

if(INVERT_VIEWPORT_Y)
	add_compile_definitions(INVERT_VIEWPORT=1)
else()
	add_compile_definitions(INVERT_VIEWPORT=0)
endif()

# ---------------------------------------------------------------------------------------
# Build Platform
# ---------------------------------------------------------------------------------------
if(WIN32)
	message(STATUS "Building for windows")
	add_compile_definitions(APP_WINDOWS)
endif()


if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
	if (${ANDROID_PLATFORM_LEVEL} LESS 24)
		message(FATAL_ERROR "Vulkan requires API level 24 or higher (currently using ${ANDROID_PLATFORM_LEVEL}).")
	endif()

	message(STATUS "Building for android")
	add_compile_definitions(APP_ANDROID)
endif()

cmake_dependent_option(USE_WAYLAND "wayland protocol for surface creation" OFF "UNIX;NOT APPLE" OFF)
cmake_dependent_option(USE_XLIB "xlib protocol for surface creation" OFF "UNIX;NOT APPLE" OFF)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	message(STATUS "Building for linux")
	add_compile_definitions(APP_LINUX)

	if(USE_WAYLAND)
		message(STATUS "using wayland protocol")
		add_compile_definitions(APP_WAYLAND)

	else()
		message(STATUS "using x11")
		add_compile_definitions(APP_X11)
	endif()
endif()

# ---------------------------------------------------------------------------------------
# Compiler
# ---------------------------------------------------------------------------------------
if(MSVC)
add_compile_definitions(APP_COMPILER_MSVC)
	if(BUILD_DEBUG)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} /Od /Z7 /FA /MTd /arch:AVX") #FAc for machine code
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Od /Z7 /FA /MTd /arch:AVX")

	else()
		add_compile_options($<$<CONFIG:Release>:/MT>)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /FA /MT /arch:AVX")

		#windows app
		if(BUILD_SHIPPING)
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup ")
		endif()

	endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_definitions(APP_COMPILER_CLANG)

	if(BUILD_DEBUG)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmove -Wshadow -msimd128 -pthread")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -Xlinker -v -v") # remove xLinker ?

	elseif(BUILD_RELEASE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -msimd128 -pthread")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

	else() #BUILD_SHIPPING
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -msimd128 -pthread -s") #--strip-all
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

	endif()


elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_definitions(APP_COMPILER_GNUC)

	if(BUILD_DEBUG)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow -msse -mavx -march=native -pthread") #-g for debug symbols
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

	elseif(BUILD_RELEASE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -msse -mavx -march=native -pthread")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

	else() #BUILD_SHIPPING
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -msse -mavx -march=native -pthread -s") #--strip-all or -s
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

	endif()

endif() #COMPILER

# ---------------------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------------------
set(APP_LOGO_PATH "engine_res/textures/logo_small.png" CACHE STRING "logo path relative to executable" FORCE)
set(APP_ASPECT_RATIO_NUM 0)
set(APP_ASPECT_RATIO_DEN 0)
add_compile_definitions(LOGO_PATH="${APP_LOGO_PATH}")
add_compile_definitions(ASPECT_RATIO_NUM=${APP_ASPECT_RATIO_NUM})
add_compile_definitions(ASPECT_RATIO_DEN=${APP_ASPECT_RATIO_DEN})

set(BUILD_SHARED_LIBS=OFF)
add_subdirectory(source/gensou_engine)
add_subdirectory(source/${CMAKE_PROJECT_NAME})
