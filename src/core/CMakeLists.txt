add_library(graphlab_core STATIC
    graph.c
    edmonds_karp.c
)

target_include_directories(graphlab_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(graphlab_core PUBLIC c_std_99)

if(UNIX)
    target_link_libraries(graphlab_core PUBLIC m)
endif()

set_target_properties(graphlab_core PROPERTIES
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
    LINKER_LANGUAGE C
)
