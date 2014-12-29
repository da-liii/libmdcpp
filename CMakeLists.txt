cmake_minimum_required(VERSION 3.0)

project(libmarkdown-cpp)

# set(Boost_USE_STATIC_LIBS ON)
find_package(Boost
	COMPONENTS regex
	REQUIRED)
	
include_directories(${Boost_INCLUDE_DIRS})

add_subdirectory(lib)
