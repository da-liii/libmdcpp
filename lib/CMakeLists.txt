set(libmdcpp_SRSC markdown.cpp markdown_tokens.cpp libmdcpp.cpp)

add_library(mdcppshared SHARED ${libmdcpp_SRSC})

SET_TARGET_PROPERTIES(mdcppshared PROPERTIES OUTPUT_NAME "mdcpp")

target_link_libraries(mdcppshared ${Boost_LIBRARIES})

set_target_properties(mdcppshared PROPERTIES VERSION 0.2.0 SOVERSION 0)

install(TARGETS mdcppshared ARCHIVE DESTINATION ${LIB_INSTALL_DIR} LIBRARY DESTINATION ${LIB_INSTALL_DIR})

if (WIN32)
add_library(mdcppstatic STATIC ${libmdcpp_SRSC})
SET_TARGET_PROPERTIES(mdcppstatic PROPERTIES OUTPUT_NAME "mdcpp")
target_link_libraries(mdcppstatic ${Boost_LIBRARIES})
install(TARGETS mdcppstatic ARCHIVE DESTINATION ${LIB_INSTALL_DIR} LIBRARY DESTINATION ${LIB_INSTALL_DIR})
endif (WIN32)

install(FILES libmdcpp.h DESTINATION include)
