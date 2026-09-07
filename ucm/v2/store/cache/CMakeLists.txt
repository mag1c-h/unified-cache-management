file(GLOB_RECURSE UCM_V2_CACHE_STORE_SOURCE_FILES "./*.cc")
add_library(cachestore SHARED ${UCM_V2_CACHE_STORE_SOURCE_FILES})
target_link_libraries(cachestore PUBLIC istore logger pthread)
install(TARGETS cachestore LIBRARY DESTINATION ${UCM_V2_LIB_INSTALL_DIR} COMPONENT ucm)
