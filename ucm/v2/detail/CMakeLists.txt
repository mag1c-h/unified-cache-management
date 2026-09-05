add_library(detail_v2 INTERFACE)
target_include_directories(detail_v2 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(detail_v2 INTERFACE fmt)
