#
# Phase 1 tests: minimal assert-style C tests, no external deps.
# Each test file is a self-contained executable registered with CTest.
#

function(graphlab_add_c_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE graphlab_core)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        TIMEOUT 60
        LABELS "core;c"
    )
endfunction()

graphlab_add_c_test(test_graph)
graphlab_add_c_test(test_edmonds_karp)
