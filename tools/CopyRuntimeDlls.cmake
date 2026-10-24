# Copies the DLLs EXE depends on from SEARCH_DIR (the MinGW bin directory) next to it, so it runs
# without MinGW on PATH. Run with cmake -P, passing EXE, SEARCH_DIR and OBJDUMP
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW) # Normalize paths before matching SEARCH_DIR
endif()

set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${OBJDUMP}")

file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${EXE}"
        RESOLVED_DEPENDENCIES_VAR dlls
        DIRECTORIES "${SEARCH_DIR}"
        PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
        POST_INCLUDE_REGEXES "^${SEARCH_DIR}"
        POST_EXCLUDE_REGEXES ".*"
)

get_filename_component(destination "${EXE}" DIRECTORY)
file(COPY ${dlls} DESTINATION "${destination}")
