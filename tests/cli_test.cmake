set(fixture "${CMAKE_CURRENT_BINARY_DIR}/cli_fixture")
file(REMOVE_RECURSE "${fixture}")
file(MAKE_DIRECTORY "${fixture}")

function(check_cli name input expected_exit expected_output)
    set(input_file "${CMAKE_CURRENT_BINARY_DIR}/cli_${name}_input.txt")
    file(WRITE "${input_file}" "${input}")
    execute_process(
        COMMAND "${APP}" ${ARGN}
        WORKING_DIRECTORY "${fixture}"
        INPUT_FILE "${input_file}"
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        RESULT_VARIABLE result
        TIMEOUT 5)
    file(REMOVE "${input_file}")
    if(NOT "${result}" STREQUAL "${expected_exit}")
        message(FATAL_ERROR "${name}: exit ${result}; stdout=${output}; stderr=${error}")
    endif()
    string(FIND "${output}${error}" "${expected_output}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${name}: missing '${expected_output}'; stdout=${output}; stderr=${error}")
    endif()
    set(last_output "${output}" PARENT_SCOPE)
endfunction()

foreach(option -h --help)
    check_cli("help_${option}" "" 0 "/index" "${option}")
    check_cli("start_help_${option}" "" 0 "/index" start "${option}")
endforeach()
foreach(option -v --version)
    check_cli("version_${option}" "" 0 "mini_lexical_search 버전 0.1.0" "${option}")
    check_cli("start_version_${option}" "" 0 "mini_lexical_search 버전 0.1.0" start "${option}")
endforeach()

check_cli(no_arguments "" 1 "사용법:")
check_cli(old_directory "" 1 "unexpected argument" "${CMAKE_CURRENT_LIST_DIR}")
check_cli(index_argument "" 1 "unexpected argument" index)
check_cli(unknown_option "" 1 "unexpected argument" --unknown)
check_cli(extra_argument "" 1 "unexpected argument" start docs)
check_cli(eof "" 0 "Mini Lexical Search started." start)

check_cli(commands "\n  \t\n /index \n/search 자료구조 알고리즘\n/help\n/search\n/index extra\n/exit extra\n/indexing\n/searching query\nsearch query\n/unknown\n/index\n/exit\n/unknown_after_exit\n"
    0 "error: indexing failed:" start)
foreach(expected
        "Search arrives in P0." "사용법:" "Usage: /search <query>"
        "not a directory: extra" "unexpected arguments for /exit"
        "Unknown command: /indexing" "Unknown command: /searching query"
        "Unknown command: search query" "Unknown command: /unknown")
    string(FIND "${last_output}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "commands: missing '${expected}'; stdout=${last_output}")
    endif()
endforeach()
string(FIND "${last_output}" "unknown_after_exit" found)
if(NOT found EQUAL -1)
    message(FATAL_ERROR "commands: /exit did not stop input processing")
endif()

check_cli(quit " /quit \n/unknown_after_quit\n" 0 "Mini Lexical Search started." start)
string(FIND "${last_output}" "unknown_after_quit" found)
if(NOT found EQUAL -1)
    message(FATAL_ERROR "quit: /quit did not stop input processing")
endif()

file(MAKE_DIRECTORY "${fixture}/documents/recap")
file(WRITE "${fixture}/documents/recap/example.md" "# recap\n")
check_cli(index_relative "/index documents/recap\n/exit\n" 0
    "0 not updated, 0 updated, 1 added, 0 deleted" start)
file(READ "${fixture}/.data/documents.json" json)
string(JSON count LENGTH "${json}")
string(JSON path GET "${json}" 0 path)
string(JSON token_count GET "${json}" 0 tokenCount)
if(NOT count EQUAL 1 OR NOT path STREQUAL "${fixture}/documents/recap/example.md"
        OR NOT token_count EQUAL 0)
    message(FATAL_ERROR "relative input did not save the expected Document: ${json}")
endif()

file(MAKE_DIRECTORY "${fixture}/documents")
file(WRITE "${fixture}/documents/default.txt" "default input\n")
check_cli(index_default "/index\n/exit\n" 0
    "1 not updated, 0 updated, 1 added, 0 deleted" start)
file(READ "${fixture}/.data/documents.json" json)
string(JSON count LENGTH "${json}")
string(JSON title GET "${json}" 0 title)
string(JSON token_count GET "${json}" 0 tokenCount)
if(NOT count EQUAL 2 OR NOT title STREQUAL "default.txt" OR NOT token_count EQUAL 0)
    message(FATAL_ERROR "default input did not replace the saved Document: ${json}")
endif()
check_cli(index_unchanged "/index\n/exit\n" 0
    "2 not updated, 0 updated, 0 added, 0 deleted" start)
file(WRITE "${fixture}/documents/default.txt" "changed input\n")
file(REMOVE "${fixture}/documents/recap/example.md")
file(WRITE "${fixture}/documents/new.md" "new document\n")
check_cli(index_changed "/index\n/exit\n" 0
    "0 not updated, 1 updated, 1 added, 1 deleted" start)
file(REMOVE_RECURSE "${fixture}")
