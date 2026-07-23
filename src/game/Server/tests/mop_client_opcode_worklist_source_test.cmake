file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcodes)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CombatHandler.cpp" combat)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ChannelHandler.cpp" channel)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandlerCustomize.cpp" character)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ItemHandler.cpp" item)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MovementHandler.cpp" movement)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/VoiceChatHandler.cpp" voice)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerLoad.cpp" player_load)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GroupHandler.cpp" group)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit)

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

if(NOT opcodes MATCHES "DefC\\(CMSG_REQUEST_RAID_INFO,[^\n]*&WorldSession::HandleRequestRaidInfoOpcode\\)")
    message(FATAL_ERROR "CMSG_REQUEST_RAID_INFO is not registered to its handler")
endif()
if(NOT opcodes MATCHES "DefS\\(SMSG_RAID_INSTANCE_INFO,")
    message(FATAL_ERROR "SMSG_RAID_INSTANCE_INFO is not registered")
endif()
foreach(token IN ITEMS
        "namespace MopRaidInstancePackets"
        "BuildRaidInstanceInfo")
    string(FIND "${player_header}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "raid-instance builder missing: ${token}")
    endif()
endforeach()
string(FIND "${player_load}" "MopRaidInstancePackets::BuildRaidInstanceInfo(data, records)" raid_builder)
if(raid_builder EQUAL -1)
    message(FATAL_ERROR "Player::SendRaidInfo does not use the 18414 builder")
endif()
string(FIND "${group}" "_player->SendRaidInfo();" raid_handler)
if(raid_handler EQUAL -1)
    message(FATAL_ERROR "raid-info request handler is disconnected")
endif()
string(FIND "${session}" "case SMSG_RAID_INSTANCE_INFO:" raid_gate)
if(raid_gate EQUAL -1)
    message(FATAL_ERROR "raid-instance response is not admitted through suppression")
endif()

foreach(route IN ITEMS
        "CMSG_MOVE_TIME_SKIPPED;HandleMoveTimeSkippedOpcode"
        "CMSG_SET_ACTIVE_MOVER;HandleSetActiveMoverOpcode")
    list(GET route 0 opcode)
    list(GET route 1 handler)
    if(NOT opcodes MATCHES "DefC\\(${opcode},[^\n]*&WorldSession::${handler}\\)")
        message(FATAL_ERROR "${opcode} is not registered to ${handler}")
    endif()
endforeach()

foreach(route IN ITEMS
        "CMSG_SET_ACTIONBAR_TOGGLES;HandleSetActionBarTogglesOpcode"
        "CMSG_VOICE_SESSION_ENABLE;HandleVoiceSessionEnableOpcode")
    list(GET route 0 opcode)
    list(GET route 1 handler)
    if(NOT opcodes MATCHES "DefC\\(${opcode},[^\n]*&WorldSession::${handler}\\)")
        message(FATAL_ERROR "${opcode} is not registered to ${handler}")
    endif()
endforeach()

foreach(route IN ITEMS
        "CMSG_SETSHEATHED;HandleSetSheathedOpcode"
        "CMSG_SET_SELECTION;HandleSetSelectionOpcode"
        "CMSG_STANDSTATECHANGE;HandleStandStateChangeOpcode")
    list(GET route 0 opcode)
    list(GET route 1 handler)
    if(NOT opcodes MATCHES "DefC\\(${opcode},[^\n]*&WorldSession::${handler}\\)")
        message(FATAL_ERROR "${opcode} is not registered to ${handler}")
    endif()
endforeach()
if(NOT opcodes MATCHES "DefS\\(SMSG_STANDSTATE_UPDATE,")
    message(FATAL_ERROR "SMSG_STANDSTATE_UPDATE is not registered")
endif()
string(FIND "${session}" "case SMSG_STANDSTATE_UPDATE:" stand_response_gate)
if(stand_response_gate EQUAL -1)
    message(FATAL_ERROR "SMSG_STANDSTATE_UPDATE is not admitted through world-send suppression")
endif()

set(selection_tokens
    "guid[7] = recv_data.ReadBit()"
    "guid[6] = recv_data.ReadBit()"
    "guid[5] = recv_data.ReadBit()"
    "guid[4] = recv_data.ReadBit()"
    "guid[3] = recv_data.ReadBit()"
    "guid[2] = recv_data.ReadBit()"
    "guid[1] = recv_data.ReadBit()"
    "guid[0] = recv_data.ReadBit()"
    "recv_data.ReadByteSeq(guid[0])"
    "recv_data.ReadByteSeq(guid[7])"
    "recv_data.ReadByteSeq(guid[3])"
    "recv_data.ReadByteSeq(guid[5])"
    "recv_data.ReadByteSeq(guid[1])"
    "recv_data.ReadByteSeq(guid[4])"
    "recv_data.ReadByteSeq(guid[6])"
    "recv_data.ReadByteSeq(guid[2])")
string(FIND "${misc}" "void WorldSession::HandleSetSelectionOpcode" selection_start)
string(FIND "${misc}" "void WorldSession::HandleStandStateChangeOpcode" selection_end)
if(selection_start EQUAL -1 OR selection_end LESS_EQUAL selection_start)
    message(FATAL_ERROR "set-selection handler body is missing")
endif()
math(EXPR selection_length "${selection_end} - ${selection_start}")
string(SUBSTRING "${misc}" ${selection_start} ${selection_length} selection_body)
set(previous_position -1)
foreach(token IN LISTS selection_tokens)
    string(FIND "${selection_body}" "${token}" position)
    if(position LESS_EQUAL previous_position)
        message(FATAL_ERROR "set-selection 18414 sequence missing or out of order: ${token}")
    endif()
    set(previous_position ${position})
endforeach()

set(sheathed_tokens
    "recv_data >> sheathed"
    "bool const hasSheathState = recv_data.ReadBit()"
    "if (!hasSheathState)"
    "if (sheathed >= MAX_SHEATH_STATE)"
    "GetPlayer()->SetSheath(SheathState(sheathed))")
string(FIND "${combat}" "void WorldSession::HandleSetSheathedOpcode" sheathed_start)
if(sheathed_start EQUAL -1)
    message(FATAL_ERROR "set-sheathed handler body is missing")
endif()
string(SUBSTRING "${combat}" ${sheathed_start} -1 sheathed_body)
set(previous_position -1)
foreach(token IN LISTS sheathed_tokens)
    string(FIND "${sheathed_body}" "${token}" position)
    if(position LESS_EQUAL previous_position)
        message(FATAL_ERROR "set-sheathed 18414 sequence missing or out of order: ${token}")
    endif()
    set(previous_position ${position})
endforeach()

string(FIND "${misc}" "void WorldSession::HandleStandStateChangeOpcode" stand_start)
string(FIND "${misc}" "void WorldSession::HandleBugOpcode" stand_end)
if(stand_start EQUAL -1 OR stand_end LESS_EQUAL stand_start)
    message(FATAL_ERROR "stand-state handler body is missing")
endif()
math(EXPR stand_length "${stand_end} - ${stand_start}")
string(SUBSTRING "${misc}" ${stand_start} ${stand_length} stand_body)
string(FIND "${stand_body}" "recv_data >> animstate;" stand_request)
string(FIND "${stand_body}" "_player->SetStandState(animstate);" stand_apply)
if(stand_request EQUAL -1 OR stand_apply LESS_EQUAL stand_request)
    message(FATAL_ERROR "stand-state request is not the 18414 uint32 body")
endif()
string(FIND "${unit}" "void Unit::SetStandState" stand_response_start)
string(FIND "${unit}" "bool Unit::IsPolymorphed" stand_response_end)
if(stand_response_start EQUAL -1 OR stand_response_end LESS_EQUAL stand_response_start)
    message(FATAL_ERROR "stand-state response sender body is missing")
endif()
math(EXPR stand_response_length "${stand_response_end} - ${stand_response_start}")
string(SUBSTRING "${unit}" ${stand_response_start} ${stand_response_length} stand_response_source)
string(FIND "${stand_response_source}" "WorldPacket data(SMSG_STANDSTATE_UPDATE, 1);" stand_response)
string(FIND "${stand_response_source}" "data << (uint8)state;" stand_response_body)
if(stand_response EQUAL -1 OR stand_response_body LESS_EQUAL stand_response)
    message(FATAL_ERROR "stand-state response is not the 18414 uint8 body")
endif()

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

string(FIND "${voice}" "bool const voiceEnabled = recv_data.ReadBit();" voice_bit)
string(FIND "${voice}" "bool const microphoneEnabled = recv_data.ReadBit();" microphone_bit)
if(voice_bit EQUAL -1 OR microphone_bit EQUAL -1)
    message(FATAL_ERROR "voice-session handler does not consume the two 18414 bits")
endif()
if(voice MATCHES "read_skip<uint8>\\(\\)")
    message(FATAL_ERROR "legacy two-byte voice-session body remains")
endif()
