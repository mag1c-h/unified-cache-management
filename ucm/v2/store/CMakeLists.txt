add_library(istore INTERFACE)
target_include_directories(istore INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(istore INTERFACE detail)

add_subdirectory(cache)
add_subdirectory(posix)
