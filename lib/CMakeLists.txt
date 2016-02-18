set(libmdcpp_SRSC markdown.cpp markdown_tokens.cpp libmdcpp.cpp)

add_library(mdcpp SHARED ${libmdcpp_SRSC})

target_link_libraries(mdcpp ${Boost_LIBRARIES})

set_target_properties(mdcpp PROPERTIES VERSION 0.2 SOVERSION 1)

install(TARGETS mdcpp LIBRARY DESTINATION ${LIB_INSTALL_DIR})

install(FILES libmdcpp.h DESTINATION include)
