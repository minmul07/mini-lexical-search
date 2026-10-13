cmake_minimum_required(VERSION 3.20)

project(mini_lexical_search
    VERSION 0.1.0
    DESCRIPTION "Line-aware Inverted Index based local full-text search engine"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(third_party/ftxui)

add_library(mini_lexical_search_domain
    src/domain/inverted_index.cpp
    src/domain/query_processor.cpp
    src/domain/tokenizer.cpp
    src/domain/version.cpp)
target_include_directories(mini_lexical_search_domain
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(mini_lexical_search_infrastructure
    src/infrastructure/document_io.cpp
    src/infrastructure/document_json.cpp
    src/infrastructure/index_json.cpp
    src/infrastructure/json.cpp
    src/infrastructure/matching_lines.cpp
    src/infrastructure/sha256.cpp)
target_link_libraries(mini_lexical_search_infrastructure
    PUBLIC mini_lexical_search_domain)

add_library(mini_lexical_search_application
    src/application/documents.cpp
    src/application/indexer.cpp
    src/application/search_application.cpp)
target_link_libraries(mini_lexical_search_application
    PUBLIC mini_lexical_search_domain
    PRIVATE mini_lexical_search_infrastructure)

# Convenience target for tests of all non-UI layers.
add_library(mini_lexical_search_core INTERFACE)
target_link_libraries(mini_lexical_search_core INTERFACE
    mini_lexical_search_application mini_lexical_search_infrastructure)

add_library(mini_lexical_search_cli
    src/presentation/cli/cli.cpp)
target_link_libraries(mini_lexical_search_cli
    PUBLIC mini_lexical_search_application)

add_library(mini_lexical_search_tui
    src/presentation/tui/tui.cpp)
target_include_directories(mini_lexical_search_tui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mini_lexical_search_tui
    PRIVATE ftxui)

add_executable(mini_lexical_search src/main.cpp)
target_link_libraries(mini_lexical_search PRIVATE mini_lexical_search_cli mini_lexical_search_tui)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
