cmake_minimum_required(VERSION 2.8)
project(test.out)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GTK_CFLAGS} -g")

include_directories(libs
	src/include
	libs/tgbot/include)

## boost
set(Boost_USE_MULTITHREADED ON)
if (ENABLE_TESTS)
    find_package(Boost 1.59.0 COMPONENTS system unit_test_framework REQUIRED)
else()
    find_package(Boost 1.59.0 COMPONENTS system REQUIRED)
endif()
include_directories(${Boost_INCLUDE_DIR})

set(LIB_LIST
	zbot
	TgBot
	${Boost_LIBRARIES})

add_subdirectory(libs/tgbot)	
add_subdirectory(src/common)
add_subdirectory(src/cli)
add_subdirectory(src/daemon)
add_executable(${PROJECT_NAME} src/test.cpp)

target_link_libraries(${PROJECT_NAME} ${LIB_LIST})
