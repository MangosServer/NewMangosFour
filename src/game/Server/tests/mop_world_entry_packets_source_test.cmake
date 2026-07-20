file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" character_handler)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)

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
        BuildMoveTeleport)
    if(NOT "${character_handler}${player}" MATCHES "MopWorldEntryPackets::${builder}")
        message(FATAL_ERROR "${builder} is not used by a production sender")
    endif()
endforeach()

foreach(server_name IN ITEMS
        SMSG_LOGIN_VERIFY_WORLD
        SMSG_NEW_WORLD
        SMSG_LOGIN_SETTIMESPEED
        SMSG_MOVE_TELEPORT)
    if(NOT opcode_registry MATCHES "DefS\\(${server_name},[ \t]*\"${server_name}\"\\)")
        message(FATAL_ERROR "${server_name} is missing outbound opcode metadata")
    endif()
endforeach()
