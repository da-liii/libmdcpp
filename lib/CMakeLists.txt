set(libmdcpp_SRSC markdown.cpp markdown_tokens.cpp)

add_library(mdcpp SHARED ${libmdcpp_SRSC})

target_link_libraries(mdcpp ${Boost_LIBRARIES})

set_target_properties(mdcpp PROPERTIES VERSION 1.0 SOVERSION 1)

install(TARGETS mdcpp LIBRARY DESTINATION lib)

install(FILES markdown.h DESTINATION include)
