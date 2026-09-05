add_library(logger SHARED logger.cc)
target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(logger PUBLIC fmt)
install(TARGETS logger LIBRARY DESTINATION ${UCM_V2_LIB_INSTALL_DIR} COMPONENT ucm)
