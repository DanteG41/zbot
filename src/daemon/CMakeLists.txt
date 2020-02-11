cmake_minimum_required(VERSION 2.8)
project(zbotd)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} ${LIB_LIST})
