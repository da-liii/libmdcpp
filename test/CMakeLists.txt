include_directories(/tmp/usr/include/)
link_directories(/tmp/usr/lib)

add_executable(markdown main.cpp)

target_link_libraries(markdown mdcpp)
