function(check_cli name input expected_exit expected_output)
    set(input_file "${CMAKE_CURRENT_BINARY_DIR}/cli_${name}_input.txt")
    file(WRITE "${input_file}" "${input}")
    execute_process(
        COMMAND "${APP}" ${ARGN}
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
    0 "Indexing arrives in P0." start)
foreach(expected
        "Search arrives in P0." "사용법:" "Usage: /search <query>"
        "unexpected arguments for /index" "unexpected arguments for /exit"
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
