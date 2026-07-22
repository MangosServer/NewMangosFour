file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcodes)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ChannelHandler.cpp" channel)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandlerCustomize.cpp" character)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ItemHandler.cpp" item)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MovementHandler.cpp" movement)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session)

foreach(route IN ITEMS
        "CMSG_REQUEST_HOTFIX;HandleRequestHotfix"
        "CMSG_JOIN_CHANNEL;HandleJoinChannelOpcode"
        "CMSG_CANCEL_TRADE;HandleCancelTradeOpcode"
        "CMSG_UI_TIME_REQUEST;HandleUITimeRequestOpcode"
        "CMSG_LOAD_SCREEN;HandleLoadScreenOpcode")
    list(GET route 0 opcode)
    list(GET route 1 handler)
    if(NOT opcodes MATCHES "DefC\\(${opcode},[^\n]*&WorldSession::${handler}\\)")
        message(FATAL_ERROR "${opcode} is not registered to ${handler}")
    endif()
endforeach()

foreach(route IN ITEMS
        "CMSG_MOVE_TIME_SKIPPED;HandleMoveTimeSkippedOpcode"
        "CMSG_SET_ACTIVE_MOVER;HandleSetActiveMoverOpcode")
    list(GET route 0 opcode)
    list(GET route 1 handler)
    if(NOT opcodes MATCHES "DefC\\(${opcode},[^\n]*&WorldSession::${handler}\\)")
        message(FATAL_ERROR "${opcode} is not registered to ${handler}")
    endif()
endforeach()

string(FIND "${misc}" "MopHotfixPackets::ReadHotfixRequest(recv_data, request)" hotfix_parser)
if(hotfix_parser EQUAL -1)
    message(FATAL_ERROR "hotfix handler does not use the 18414 parser")
endif()

foreach(response IN ITEMS SMSG_UI_TIME SMSG_DB_REPLY)
    if(NOT opcodes MATCHES "DefS\\(${response},")
        message(FATAL_ERROR "${response} is not registered for server sends")
    endif()
    if(NOT session MATCHES "case ${response}:")
        message(FATAL_ERROR "${response} is not admitted through world-send suppression")
    endif()
endforeach()

string(FIND "${item}" "MopHotfixPackets::BuildDbReply(data, entry," db_reply_builder)
if(db_reply_builder EQUAL -1)
    message(FATAL_ERROR "item hotfix replies do not use the 18414 response builder")
endif()
if(misc MATCHES "ReadBits\\(23\\)")
    message(FATAL_ERROR "legacy 23-bit hotfix count remains")
endif()
string(FIND "${channel}" "MopChannelPackets::ReadJoinChannelRequest(recvPacket, request)" channel_parser)
if(channel_parser EQUAL -1)
    message(FATAL_ERROR "join-channel handler does not use the 18414 parser")
endif()
string(FIND "${character}" "MopClientRequestPackets::ReadLoadScreenRequest(recvPacket)" load_screen_parser)
if(load_screen_parser EQUAL -1)
    message(FATAL_ERROR "load-screen handler does not use the 18414 parser")
endif()

string(FIND "${misc}" "MopControlPackets::ReadMoveTimeSkipped(recv_data)" time_skipped_parser)
if(time_skipped_parser EQUAL -1)
    message(FATAL_ERROR "move-time-skipped handler does not use the 18414 parser")
endif()
string(FIND "${movement}" "MopControlPackets::ReadSetActiveMover(recv_data, request)" active_mover_parser)
if(active_mover_parser EQUAL -1)
    message(FATAL_ERROR "active-mover handler does not use the 18414 parser")
endif()
