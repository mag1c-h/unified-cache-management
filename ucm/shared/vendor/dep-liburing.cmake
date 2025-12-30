set(LIBURING_INSTALL OFF CACHE INTERNAL "" FORCE)
set(LIBURING_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(LIBURING_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)

if(DOWNLOAD_DEPENDENCE)
    set(DEP_LIBURING_NAME liburing)
    set(DEP_LIBURING_TAG liburing-2.12)
    set(DEP_LIBURING_GIT_URLS
        https://github.com/axboe/liburing.git
        https://gitcode.com/gh_mirrors/li/liburing.git
    )
    include(helper.cmake)
    find_reachable_git_url(REACHABLE_URL DEP_LIBURING_GIT_URLS)
    include(FetchContent)
    message(STATUS "Fetching ${DEP_LIBURING_NAME}(${DEP_LIBURING_TAG}) from ${REACHABLE_URL}")
    FetchContent_Declare(
        ${DEP_LIBURING_NAME}
        GIT_REPOSITORY ${REACHABLE_URL}
        GIT_TAG ${DEP_LIBURING_TAG}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(${DEP_LIBURING_NAME})
    execute_process(
        COMMAND ${${DEP_LIBURING_NAME}_SOURCE_DIR}/configure
        WORKING_DIRECTORY ${${DEP_LIBURING_NAME}_SOURCE_DIR}
        OUTPUT_QUIET
    )
    execute_process(
        COMMAND make -C src -j${CMAKE_BUILD_PARALLEL_LEVEL}
        WORKING_DIRECTORY ${${DEP_LIBURING_NAME}_SOURCE_DIR}
        OUTPUT_QUIET
    )
    add_library(${DEP_LIBURING_NAME} STATIC IMPORTED GLOBAL)
    set_target_properties(${DEP_LIBURING_NAME} PROPERTIES
        IMPORTED_LOCATION ${${DEP_LIBURING_NAME}_SOURCE_DIR}/src/${DEP_LIBURING_NAME}.a
        INTERFACE_INCLUDE_DIRECTORIES ${${DEP_LIBURING_NAME}_SOURCE_DIR}/src/include
    )
else()
    add_subdirectory(liburing)
endif()
