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

find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_library(mini_lexical_search_core
    src/document.cpp
    src/document_json.cpp
    src/index_json.cpp
    src/indexer.cpp
    src/inverted_index.cpp
    src/json.cpp
    src/tokenizer.cpp
    src/version.cpp)
target_include_directories(mini_lexical_search_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mini_lexical_search_core PRIVATE OpenSSL::Crypto)

add_executable(mini_lexical_search
    src/main.cpp)
target_link_libraries(mini_lexical_search
    PRIVATE mini_lexical_search_core)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
