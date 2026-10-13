find_package(Threads REQUIRED)

add_library(ftxui STATIC ftxui.cpp)
target_include_directories(ftxui SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ftxui PUBLIC Threads::Threads)
