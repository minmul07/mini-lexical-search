add_executable(version_test
    version_test.cpp)
target_link_libraries(version_test
    PRIVATE mini_lexical_search_core)

add_test(
    NAME version_test
    COMMAND version_test)

add_test(
    NAME cli_test
    COMMAND ${CMAKE_COMMAND}
        -DAPP=$<TARGET_FILE:mini_lexical_search>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_test.cmake)
