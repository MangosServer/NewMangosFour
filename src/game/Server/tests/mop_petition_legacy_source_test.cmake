file(GLOB_RECURSE game_sources
    "${SOURCE_ROOT}/src/game/*.cpp"
    "${SOURCE_ROOT}/src/game/*.h")

set(forbidden_patterns
    "(^|[^A-Z0-9_])MSG_PETITION_DECLINE([^A-Z0-9_]|$)"
    "(^|[^A-Z0-9_])MSG_PETITION_RENAME([^A-Z0-9_]|$)"
    "(^|[^A-Za-z0-9_])HandlePetitionDeclineOpcode([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])HandlePetitionRenameOpcode([^A-Za-z0-9_]|$)")

set(required_patterns
    CMSG_PETITION_SIGN
    SMSG_PETITION_SIGN_RESULTS
    HandlePetitionSignOpcode
    "`petition_sign`")

foreach(required IN LISTS required_patterns)
    string(MAKE_C_IDENTIFIER "${required}" required_id)
    set("found_${required_id}" FALSE)
endforeach()

foreach(path IN LISTS game_sources)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "missing petition cleanup source: ${path}")
    endif()

    file(READ "${path}" source)

    foreach(forbidden IN LISTS forbidden_patterns)
        string(REGEX MATCH "${forbidden}" found "${source}")
        if(found)
            message(FATAL_ERROR "retired petition endpoint remains in ${path}: ${forbidden}")
        endif()
    endforeach()

    foreach(required IN LISTS required_patterns)
        string(FIND "${source}" "${required}" found)
        if(NOT found EQUAL -1)
            string(MAKE_C_IDENTIFIER "${required}" required_id)
            set("found_${required_id}" TRUE)
        endif()
    endforeach()
endforeach()

if(MUTATION STREQUAL "legacy_sender")
    set(mutation_source "WorldPacket data(MSG_PETITION_RENAME);")
elseif(MUTATION STREQUAL "legacy_handler")
    set(mutation_source "void HandlePetitionDeclineOpcode(WorldPacket&);")
endif()

foreach(forbidden IN LISTS forbidden_patterns)
    string(REGEX MATCH "${forbidden}" found "${mutation_source}")
    if(found)
        message(FATAL_ERROR "mutation escaped petition cleanup: ${forbidden}")
    endif()
endforeach()

foreach(required IN LISTS required_patterns)
    string(MAKE_C_IDENTIFIER "${required}" required_id)
    if(NOT found_${required_id})
        message(FATAL_ERROR "shared petition behavior was removed: ${required}")
    endif()
endforeach()
