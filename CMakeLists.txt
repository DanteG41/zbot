cmake_minimum_required(VERSION 3.3)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GTK_CFLAGS}")
set(CMAKE_DISABLE_FIND_PACKAGE_CURL ON)

include_directories(libs
	src/include
	libs/tgbot/include)

set(LIB_LIST
	zbot)

add_subdirectory(libs/tgbot EXCLUDE_FROM_ALL)
add_subdirectory(src/common)
add_subdirectory(src/cli)
add_subdirectory(src/daemon)
