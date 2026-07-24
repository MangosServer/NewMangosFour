file(READ "${SOURCE_ROOT}/cmake/MangosParams.cmake" mangos_params)
file(READ "${SOURCE_ROOT}/src/shared/CMakeLists.txt" shared_cmake)
file(READ "${SOURCE_ROOT}/src/shared/Utilities/Util.cpp" util)

string(REGEX MATCH "set\\(MANGOS_EXP[ \t]+\"MISTS\"\\)" expansion_define
    "${mangos_params}")
if(NOT expansion_define)
    message(FATAL_ERROR "NewMangosFour no longer exports the MISTS expansion define")
endif()

string(REGEX MATCH
    "#elif defined\\(MOP\\) \\|\\| defined\\(MISTS\\)"
    mop_banner_branch "${util}")
if(NOT mop_banner_branch)
    message(FATAL_ERROR
        "return_iCoreNumber does not map the canonical MISTS build define to Four")
endif()

string(REGEX MATCH
    "PUBLIC[\r\n\t ]+[$][{]CMAKE_CURRENT_BINARY_DIR[}][\r\n\t ]+[$][{]CMAKE_CURRENT_SOURCE_DIR[}]"
    generated_headers_first "${shared_cmake}")
if(NOT generated_headers_first)
    message(FATAL_ERROR
        "shared target does not prefer generated headers over stale source-tree copies")
endif()
