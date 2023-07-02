file(GLOB_RECURSE ProjectSourceFiles "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

if(ANDROID)
	add_library(engine_cpp SHARED ${ProjectSourceFiles})
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate -Xlinker -v")

	target_link_libraries(engine_cpp gensou)
	target_include_directories(engine_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
	target_include_directories(engine_cpp PUBLIC ${CMAKE_SOURCE_DIR}/source/gensou_engine/include)

else()
	if(WIN32)

		if(BUILD_SHIPPING)
			add_executable(
				${CMAKE_PROJECT_NAME} WIN32
				${ProjectSourceFiles}
				${CMAKE_SOURCE_DIR}/resources/windows/icon.rc
			)
		else()
			add_executable(
				${CMAKE_PROJECT_NAME}
				${ProjectSourceFiles}
				${CMAKE_SOURCE_DIR}/resources/windows/icon.rc
			)
		endif()

	else()
		add_executable(${CMAKE_PROJECT_NAME} ${ProjectSourceFiles})

	endif()

	target_link_libraries(${CMAKE_PROJECT_NAME} gensou)
	target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
	target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/source/gensou_engine/include)

endif()
