cmake_minimum_required(VERSION 2.8)

project(libmdcpp)

set (CMAKE_CXX_FLAGS "-std=c++11")

# set(Boost_USE_STATIC_LIBS ON)
find_package(Boost
    COMPONENTS regex
	REQUIRED)
	
include_directories(${Boost_INCLUDE_DIRS})

add_subdirectory(lib)
add_subdirectory(test)
