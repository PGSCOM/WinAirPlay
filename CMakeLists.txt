cmake_minimum_required(VERSION 3.4.1)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
		STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

project(rpiplay)

include_directories(lib/pthreads)

set (CMAKE_CXX_STANDARD 11)

set (RENDERER_FLAGS "")

add_subdirectory(lib/playfair)
add_subdirectory(lib/llhttp)
add_subdirectory(lib/plist)
add_subdirectory(lib/aac_decoder)
add_subdirectory(renderers/fdk-aac)
add_subdirectory(lib/fdkaac_decoder)
add_subdirectory(lib/h264_decoder)
add_subdirectory(lib/pthreads)
add_subdirectory(lib)
add_subdirectory(renderers)

# Make sure the main executable is aware of the available renderers
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RENDERER_FLAGS}" )

add_executable( rpiplay rpiplay.cpp)
target_link_libraries ( rpiplay renderers airplay fdk-aac)
target_link_libraries ( rpiplay "mf.lib" )
target_link_libraries ( rpiplay "mfplat.lib" )
target_link_libraries ( rpiplay "mfuuid.lib" )
target_link_libraries ( rpiplay "mfreadwrite.lib" )
target_link_libraries ( rpiplay "mfplay.lib" )
target_link_libraries ( rpiplay "wmcodecdspuuid.lib" )

install(TARGETS rpiplay RUNTIME DESTINATION bin)
