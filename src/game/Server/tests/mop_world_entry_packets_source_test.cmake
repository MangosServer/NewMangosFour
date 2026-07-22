file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" character_handler)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(character_handler MATCHES "WorldPacket[ \t]+data\\(SMSG_LOGIN_VERIFY_WORLD")
    message(FATAL_ERROR "inline SMSG_LOGIN_VERIFY_WORLD writer remains")
endif()
if(player MATCHES "WorldPacket[ \t]+data\\(SMSG_NEW_WORLD")
    message(FATAL_ERROR "inline SMSG_NEW_WORLD writer remains")
endif()
if(player MATCHES "Initialize\\(SMSG_LOGIN_SETTIMESPEED")
    message(FATAL_ERROR "inline SMSG_LOGIN_SETTIMESPEED writer remains")
endif()
if(player MATCHES "WorldPacket[ \t]+data\\(SMSG_MOVE_TELEPORT")
    message(FATAL_ERROR "inline SMSG_MOVE_TELEPORT writer remains")
endif()

foreach(builder IN ITEMS
        BuildLoginVerifyWorld
        BuildNewWorld
        BuildLoginSetTimeSpeed
        BuildMoveTeleport
        BuildTimeSync
        BuildTriggerCinematic
        BuildWorldServerInfo)
    if(NOT "${character_handler}${player}" MATCHES "MopWorldEntryPackets::${builder}")
        message(FATAL_ERROR "${builder} is not used by a production sender")
    endif()
endforeach()

foreach(server_name IN ITEMS
        SMSG_LOGIN_VERIFY_WORLD
        SMSG_NEW_WORLD
        SMSG_LOGIN_SETTIMESPEED
        SMSG_MOVE_TELEPORT
        SMSG_TIME_SYNC_REQ
        SMSG_TRIGGER_CINEMATIC
        SMSG_WORLD_SERVER_INFO)
    if(NOT opcode_registry MATCHES "DefS\\(${server_name},[ \t]*\"${server_name}\"\\)")
        message(FATAL_ERROR "${server_name} is missing outbound opcode metadata")
    endif()
endforeach()

foreach(server_name IN ITEMS
        SMSG_TIME_SYNC_REQ
        SMSG_TRIGGER_CINEMATIC
        SMSG_WORLD_SERVER_INFO)
    if(NOT world_session MATCHES "case[ \t]+${server_name}:")
        message(FATAL_ERROR "${server_name} is missing from central enter-world admission")
    endif()
endforeach()

string(FIND "${character_handler}" "WorldPacket ts(SMSG_TIME_SYNC_REQ" inline_time_sync)
if(NOT inline_time_sync EQUAL -1)
    message(FATAL_ERROR "duplicate inline login SMSG_TIME_SYNC_REQ sender remains")
endif()
