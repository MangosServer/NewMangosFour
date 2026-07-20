file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" character_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" active_opcodes)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" reference_opcodes)

foreach(source_text IN ITEMS "${character_handler}" "${active_opcodes}" "${reference_opcodes}")
    if(source_text MATCHES "SMSG_LEARNED_DANCE_MOVES")
        message(FATAL_ERROR "obsolete SMSG_LEARNED_DANCE_MOVES remains in the source tree")
    endif()
endforeach()

foreach(name_and_value IN ITEMS
        "SMSG_SPELL_EXECUTE_LOG;0x00D8"
        "SMSG_ATTACKSWING_ERROR;0x11E1"
        "SMSG_RANDOM_ROLL;0x141A"
        "SMSG_INSPECT_RATED_BG_STATS;0x041F")
    list(GET name_and_value 0 opcode_name)
    list(GET name_and_value 1 opcode_value)
    if(NOT reference_opcodes MATCHES "${opcode_name}[ \t]+=[ \t]+${opcode_value}")
        message(FATAL_ERROR "${opcode_name}=${opcode_value} is missing from Opcodes_reference.h")
    endif()
endforeach()

if(reference_opcodes MATCHES "SMSG_UNKNOWN_0x11E1")
    message(FATAL_ERROR "obsolete SMSG_UNKNOWN_0x11E1 remains in Opcodes_reference.h")
endif()
