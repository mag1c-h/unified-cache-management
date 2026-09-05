add_library(store_iface INTERFACE)
target_include_directories(store_iface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(store_iface INTERFACE detail)

add_subdirectory(cache)
add_subdirectory(posix)
