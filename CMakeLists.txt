cmake_minimum_required(VERSION 3.22)
project(CCOrange_TinyWebServer)

set(CMAKE_CXX_STANDARD 14)

file(COPY staticResources DESTINATION ${CMAKE_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(Threads REQUIRED)
find_package(MYSQL REQUIRED)

add_subdirectory(code)