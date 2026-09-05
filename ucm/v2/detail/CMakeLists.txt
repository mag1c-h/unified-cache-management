add_library(detail INTERFACE)
target_include_directories(detail INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(detail INTERFACE fmt)
