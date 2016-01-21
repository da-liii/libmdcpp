cmake_minimum_required(VERSION 2.8)

project(libmdcpp)

set (CMAKE_CXX_FLAGS "-std=c++11")

if (NOT LIB_INSTALL_DIR)
    set (LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib")
endif (NOT LIB_INSTALL_DIR)

# set(Boost_USE_STATIC_LIBS ON)
find_package(Boost
    COMPONENTS regex
	REQUIRED)
	
include_directories(${Boost_INCLUDE_DIRS})

add_subdirectory(lib)
add_subdirectory(test)
