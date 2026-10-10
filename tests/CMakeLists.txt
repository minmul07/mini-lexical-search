add_executable(version_test
    version_test.cpp)
target_link_libraries(version_test
    PRIVATE mini_lexical_search_core)

add_test(
    NAME version_test
    COMMAND version_test)

add_executable(tokenizer_test tokenizer_test.cpp)
target_link_libraries(tokenizer_test PRIVATE mini_lexical_search_core)
add_test(
    NAME tokenizer_test
    COMMAND tokenizer_test)

add_executable(sha256_test sha256_test.cpp)
target_link_libraries(sha256_test PRIVATE mini_lexical_search_core)
add_test(
    NAME sha256_test
    COMMAND sha256_test)

add_executable(inverted_index_test inverted_index_test.cpp)
target_link_libraries(inverted_index_test PRIVATE mini_lexical_search_core)
add_test(
    NAME inverted_index_test
    COMMAND inverted_index_test)

add_executable(indexer_test indexer_test.cpp)
target_link_libraries(indexer_test PRIVATE mini_lexical_search_core)
add_test(
    NAME indexer_test
    COMMAND indexer_test ${CMAKE_CURRENT_BINARY_DIR}/indexer_fixture)

add_test(
    NAME cli_test
    COMMAND ${CMAKE_COMMAND}
        -DAPP=$<TARGET_FILE:mini_lexical_search>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_test.cmake)

add_executable(document_test document_test.cpp)
target_link_libraries(document_test PRIVATE mini_lexical_search_core)
add_test(
    NAME document_test
    COMMAND ${CMAKE_COMMAND}
        -DTEST_APP=$<TARGET_FILE:document_test>
        -DFIXTURE=${CMAKE_CURRENT_BINARY_DIR}/document_fixture
        -P ${CMAKE_CURRENT_SOURCE_DIR}/document_test.cmake)
