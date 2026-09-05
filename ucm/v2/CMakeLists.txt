add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/_libs")
set(UCM_V2_PYB_INSTALL_DIR "ucm/v2")
set(UCM_V2_LIB_INSTALL_DIR "ucm/v2/_libs")

add_subdirectory(detail)
add_subdirectory(infra)
add_subdirectory(transfer)
add_subdirectory(store)
add_subdirectory(proxy)

if(BUILD_UNIT_TESTS)
    add_subdirectory(test)
endif()
