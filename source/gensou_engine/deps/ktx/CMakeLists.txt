project(KtxSoftware VERSION 1.0.0)

file(GLOB_RECURSE dfdutils_source "dfdutils/*.c")

add_library(
	KtxSoftware
	STATIC
	include/ktx.h
    lib/basis_sgd.h
    lib/basis_transcode.cpp
    lib/basisu/transcoder/basisu_containers.h
    lib/basisu/transcoder/basisu_containers_impl.h
    lib/basisu/transcoder/basisu_file_headers.h
    lib/basisu/transcoder/basisu_transcoder_internal.h
    lib/basisu/transcoder/basisu_transcoder_uastc.h
    lib/basisu/transcoder/basisu_transcoder.cpp
    lib/basisu/transcoder/basisu_transcoder.h
    lib/basisu/transcoder/basisu.h
    lib/basisu/zstd/zstd.c
    lib/checkheader.c
    lib/dfdutils/createdfd.c
    lib/dfdutils/colourspaces.c
    lib/dfdutils/dfd.h
    lib/dfdutils/interpretdfd.c
    lib/dfdutils/printdfd.c
    lib/dfdutils/queries.c
    lib/dfdutils/vk2dfd.c
    lib/dfdutils/vk2dfd.inl
    lib/dfdutils/vulkan/vk_platform.h
    lib/dfdutils/vulkan/vulkan_core.h
    lib/etcdec.cxx
    lib/etcunpack.cxx
    lib/filestream.c
    lib/filestream.h
    lib/formatsize.h
    lib/gl_format.h
    lib/hashlist.c
    lib/info.c
    lib/ktxint.h
    lib/memstream.c
    lib/memstream.h
    lib/strings.c
    lib/swap.c
    lib/texture.c
    lib/texture.h
    lib/texture1.c
    lib/texture1.h
    lib/texture2.c
    lib/texture2.h
    lib/uthash.h
    lib/vk_format.h
    lib/vkformat_check.c
    lib/vkformat_enum.h
    lib/vkformat_str.c
    lib/vkloader.c
    lib/vk_funcs.c
    lib/vk_funcs.h


	####################################

	#${dfdutils_source}
	#lib/basisu/zstd/zstd.c
	#lib/basisu/zstd/zstddeclib.c
	#lib/basisu/zstd/zstd_errors.h

	#lib/texture.c
	#lib/texture1.c
	#lib/texture2.c  
	#lib/hashlist.c
	#lib/info.c
	#lib/checkheader.c 
	#lib/swap.c
	#lib/strings.c
	#lib/memstream.c 
	#lib/filestream.c
	#lib/vkloader.c
	#lib/vk_funcs.c
	#lib/vkformat_check.c
	#lib/vkformat_enum.h
	#lib/vkformat_str.c
	#lib/writer1.c
	#lib/writer2.c
)

if(ANDROID)
	target_link_libraries(${PROJECT_NAME} vulkan)
	#target_include_directories(${PROJECT_NAME} PRIVATE ${VULKAN_INCLUDE_DIRS})
else()
	target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
	target_include_directories(${PROJECT_NAME} PRIVATE ${VULKAN_INCLUDE_DIRS})
endif()

target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lib/dfdutils")
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lib/basisu/zstd")
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lib")

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/utils")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/other_include")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lib")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lib/basisu/zstd")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lib/dfdutils")