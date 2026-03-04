file(GLOB_RECURSE UCM_ASYNC_STORE_CC_SOURCE_FILES "./cc/*.cc")
add_library(asyncstore SHARED ${UCM_ASYNC_STORE_CC_SOURCE_FILES})
target_include_directories(asyncstore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cc)
target_link_libraries(asyncstore PUBLIC storeintf infra_logger)

file(RELATIVE_PATH INSTALL_REL_PATH ${UCM_ROOT_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
install(TARGETS asyncstore LIBRARY DESTINATION ${INSTALL_REL_PATH} COMPONENT ucm)
