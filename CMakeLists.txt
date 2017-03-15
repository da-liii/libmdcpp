cmake_minimum_required(VERSION 2.8)

project(libmdcpp)

if (WIN32)
    set (CMAKE_CXX_FLAGS "/EHsc")
    set(Boost_USE_STATIC_LIBS ON)
    #set(BOOST_ROOT "C:\local\boost_1_60_0")
    #set(BOOST_LIBRARYDIR "C:\local\boost_1_60_0\lib64-msvc-14.0")
else(WIN32)
    set (CMAKE_CXX_FLAGS "-std=c++11")
endif(WIN32)

if (NOT LIB_INSTALL_DIR)
    set (LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib")
endif (NOT LIB_INSTALL_DIR)

find_package(Boost
    COMPONENTS regex
	REQUIRED)
	
include_directories(${Boost_INCLUDE_DIRS})

add_subdirectory(lib)
# add_subdirectory(test)
