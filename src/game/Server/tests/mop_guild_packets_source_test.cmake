file(READ "${SOURCE_ROOT}/src/game/Object/Guild.h" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" login_sender)
file(READ "${SOURCE_ROOT}/src/game/Object/Guild.cpp" guild_sender)
file(READ "${SOURCE_ROOT}/src/game/Object/GuildBank.cpp" guild_bank_sender)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/NPCHandler.cpp" npc_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GuildHandler.cpp" guild_handler)

if(MUTATION STREQUAL "login_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildMotd(data, guild->GetMOTD())"
        "false /* removed login MOTD builder */"
        login_sender "${login_sender}")
elseif(MUTATION STREQUAL "broadcast_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildMotd(data, motd)"
        "false /* removed broadcast MOTD builder */"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "outbound_registration")
    string(REPLACE
        "DefS(SMSG_GUILD_EVENT_MOTD, \"SMSG_GUILD_EVENT_MOTD\");"
        "/* removed guild MOTD registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "opcode_value")
    string(REPLACE
        "SMSG_GUILD_EVENT_MOTD                        = 0x0B68"
        "SMSG_GUILD_EVENT_MOTD                        = 0x0B69"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "tabard_reader")
    string(REPLACE
        "MopGuildPackets::ReadTabardVendorActivate(recv_data)"
        "uint64(0) /* removed tabard request reader */"
        npc_handler "${npc_handler}")
elseif(MUTATION STREQUAL "tabard_builder")
    string(REPLACE
        "MopGuildPackets::BuildTabardVendorActivate(data, guid.GetRawValue())"
        "/* removed tabard response builder */"
        npc_handler "${npc_handler}")
elseif(MUTATION STREQUAL "save_reader")
    string(REPLACE
        "MopGuildPackets::ReadSaveGuildEmblem(recvPacket)"
        "MopGuildPackets::EmblemDesign{} /* removed emblem request reader */"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "save_builder")
    string(REPLACE
        "MopGuildPackets::BuildSaveGuildEmblemResult(data, msg)"
        "/* removed emblem result builder */"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "tabard_registration")
    string(REPLACE
        "DefC(CMSG_TABARD_VENDOR_ACTIVATE, \"CMSG_TABARD_VENDOR_ACTIVATE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTabardVendorActivateOpcode);"
        "/* removed tabard request registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "save_registration")
    string(REPLACE
        "DefC(CMSG_SAVE_GUILD_EMBLEM, \"CMSG_SAVE_GUILD_EMBLEM\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSaveGuildEmblemOpcode);"
        "/* removed emblem request registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "tabard_allowlist")
    string(REPLACE
        "case SMSG_TABARD_VENDOR_ACTIVATE:"
        "case 0xFFFF: /* removed tabard response allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "design_bounds")
    string(REPLACE
        "design.emblemStyle >= 196"
        "false /* removed emblem design bounds */"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "joined_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildMemberJoined(data"
        "MopGuildPackets::RemovedGuildMemberJoined(data"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "presence_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildPresenceChange(data"
        "MopGuildPackets::RemovedGuildPresenceChange(data"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "rank_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildMemberRankUpdate(data"
        "MopGuildPackets::RemovedGuildMemberRankUpdate(data"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "leader_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildNewLeader(data"
        "MopGuildPackets::RemovedGuildNewLeader(data"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "left_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildPlayerLeft(data"
        "MopGuildPackets::RemovedGuildPlayerLeft(data"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "disband_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildDisbanded(data)"
        "MopGuildPackets::RemovedGuildDisbanded(data)"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "event_registration")
    string(REPLACE
        "DefS(SMSG_GUILD_EVENT_PLAYER_JOINED, \"SMSG_GUILD_EVENT_PLAYER_JOINED\");"
        "/* removed joined-event registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "event_allowlist")
    string(REPLACE
        "case SMSG_GUILD_EVENT_PLAYER_JOINED:"
        "case 0xFFFF: /* removed joined-event allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "event_opcode")
    string(REPLACE
        "SMSG_GUILD_EVENT_PLAYER_JOINED               = 0x0B69"
        "SMSG_GUILD_EVENT_PLAYER_JOINED               = 0x0B6A"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "presence_calls")
    string(REPLACE
        "guild->BroadcastMemberPresence(pCurrChar->GetObjectGuid(), pCurrChar->GetName(), true);"
        "/* removed login presence broadcast */"
        login_sender "${login_sender}")
elseif(MUTATION STREQUAL "removed_member_copy")
    string(REPLACE
        "ObjectGuid const removedGuid = slot->guid;"
        "ObjectGuid const removedGuid; /* removed pre-erase copy */"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "legacy_info")
    string(APPEND opcode_header "\nWorldPacket legacy(SMSG_GUILD_INFO);\n")
elseif(MUTATION STREQUAL "legacy_event")
    string(APPEND guild_sender "\nWorldPacket legacy(SMSG_GUILD_EVENT);\n")
elseif(MUTATION STREQUAL "money_client_registration")
    string(REPLACE
        "DefC(CMSG_GUILD_BANK_MONEY_WITHDRAWN, \"CMSG_GUILD_BANK_MONEY_WITHDRAWN\""
        "/* removed guild-bank money client registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "money_server_registration")
    string(REPLACE
        "DefS(SMSG_GUILD_BANK_MONEY_WITHDRAWN, \"SMSG_GUILD_BANK_MONEY_WITHDRAWN\");"
        "/* removed guild-bank money server registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "money_gate")
    string(REPLACE
        "case SMSG_GUILD_BANK_MONEY_WITHDRAWN:"
        "/* removed guild-bank money framing gate */"
        world_session "${world_session}")
endif()

function(require_once source token context)
    set(remaining "${source}")
    set(count 0)
    while(TRUE)
        string(FIND "${remaining}" "${token}" position)
        if(position EQUAL -1)
            break()
        endif()
        math(EXPR count "${count} + 1")
        string(LENGTH "${token}" token_length)
        math(EXPR next_position "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endwhile()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once("${login_sender}"
    "MopGuildPackets::BuildGuildMotd(data, guild->GetMOTD())"
    "login MOTD builder call")
require_once("${guild_sender}"
    "MopGuildPackets::BuildGuildMotd(data, motd)"
    "broadcast MOTD builder call")
require_once("${opcode_registry}"
    "DefS(SMSG_GUILD_EVENT_MOTD, \"SMSG_GUILD_EVENT_MOTD\");"
    "guild MOTD registration")
require_once("${opcode_header}"
    "SMSG_GUILD_EVENT_MOTD                        = 0x0B68"
    "guild MOTD opcode value")
require_once("${npc_handler}"
    "MopGuildPackets::ReadTabardVendorActivate(recv_data)"
    "tabard-vendor request reader")
require_once("${npc_handler}"
    "MopGuildPackets::BuildTabardVendorActivate(data, guid.GetRawValue())"
    "tabard-vendor response builder")
require_once("${guild_handler}"
    "MopGuildPackets::ReadSaveGuildEmblem(recvPacket)"
    "guild-emblem request reader")
require_once("${guild_handler}"
    "MopGuildPackets::BuildSaveGuildEmblemResult(data, msg)"
    "guild-emblem result builder")
require_once("${opcode_registry}"
    "DefC(CMSG_TABARD_VENDOR_ACTIVATE, \"CMSG_TABARD_VENDOR_ACTIVATE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTabardVendorActivateOpcode);"
    "tabard-vendor request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_TABARD_VENDOR_ACTIVATE, \"SMSG_TABARD_VENDOR_ACTIVATE\");"
    "tabard-vendor response registration")
require_once("${opcode_registry}"
    "DefC(CMSG_SAVE_GUILD_EMBLEM, \"CMSG_SAVE_GUILD_EMBLEM\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSaveGuildEmblemOpcode);"
    "guild-emblem request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_SAVE_GUILD_EMBLEM, \"SMSG_SAVE_GUILD_EMBLEM\");"
    "guild-emblem result registration")
require_once("${world_session}"
    "case SMSG_TABARD_VENDOR_ACTIVATE:"
    "tabard-vendor response allowlist")
require_once("${world_session}"
    "case SMSG_SAVE_GUILD_EMBLEM:"
    "guild-emblem result allowlist")
require_once("${opcode_registry}"
    "DefC(CMSG_GUILD_BANK_MONEY_WITHDRAWN, \"CMSG_GUILD_BANK_MONEY_WITHDRAWN\""
    "guild-bank money client registration")
require_once("${opcode_registry}"
    "DefS(SMSG_GUILD_BANK_MONEY_WITHDRAWN, \"SMSG_GUILD_BANK_MONEY_WITHDRAWN\");"
    "guild-bank money server registration")
require_once("${guild_bank_sender}"
    "data << uint64(GetMemberMoneyWithdrawRem(LowGuid));"
    "guild-bank money uint64 body")
require_once("${world_session}"
    "case SMSG_GUILD_BANK_MONEY_WITHDRAWN:"
    "guild-bank money framing gate")

foreach(token IN ITEMS
        "CMSG_TABARD_VENDOR_ACTIVATE                 = 0x11C3"
        "SMSG_TABARD_VENDOR_ACTIVATE                  = 0x0A3E"
        "CMSG_SAVE_GUILD_EMBLEM                      = 0x1D60"
        "SMSG_SAVE_GUILD_EMBLEM                      = 0x089F")
    string(FIND "${opcode_header}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "tabard opcode missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "motd.size() >= (size_t(1) << 10)"
        "out.WriteBits(motd.size(), 10)"
        "out.FlushBits()"
        "out.append(motd.data(), motd.size())")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "guild MOTD builder missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "uint8 const maskOrder[] = { 2, 1, 4, 6, 3, 5, 0, 7 }"
        "uint8 const byteOrder[] = { 7, 6, 2, 5, 0, 1, 4, 3 }"
        "uint8 const maskOrder[] = { 1, 5, 0, 7, 4, 6, 3, 2 }"
        "uint8 const byteOrder[] = { 5, 4, 2, 3, 6, 0, 1, 7 }"
        "in >> design.borderStyle"
        "in >> design.backgroundColor"
        "in >> design.borderColor"
        "in >> design.emblemColor"
        "in >> design.emblemStyle"
        "uint8 const maskOrder[] = { 0, 7, 4, 6, 5, 1, 2, 3 }"
        "uint8 const byteOrder[] = { 6, 2, 7, 5, 0, 4, 1, 3 }"
        "out.Initialize(SMSG_SAVE_GUILD_EMBLEM, 4)")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "tabard packet codec missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "design.emblemStyle >= 196"
        "design.emblemColor >= 17"
        "design.borderStyle >= 6"
        "design.borderColor >= 17"
        "design.backgroundColor >= 51"
        "SendSaveGuildEmblem(ERR_GUILDEMBLEM_INVALID_TABARD_COLORS)")
    string(FIND "${guild_handler}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "guild-emblem validation missing: ${token}")
    endif()
endforeach()

string(FIND "${login_sender}" "data.Initialize(SMSG_GUILD_EVENT," legacy_login)
if(NOT legacy_login EQUAL -1)
    message(FATAL_ERROR "login still sends the legacy generic guild event")
endif()

string(FIND "${npc_handler}" "MSG_TABARDVENDOR_ACTIVATE" legacy_tabard)
if(NOT legacy_tabard EQUAL -1)
    message(FATAL_ERROR "NPC handler still uses legacy tabard-vendor opcode")
endif()

string(FIND "${guild_handler}" "WorldPacket data(MSG_SAVE_GUILD_EMBLEM" legacy_emblem)
if(NOT legacy_emblem EQUAL -1)
    message(FATAL_ERROR "guild handler still uses legacy emblem opcode")
endif()

foreach(token IN ITEMS
        "MopGuildPackets::BuildGuildMemberJoined(data"
        "MopGuildPackets::BuildGuildPresenceChange(data"
        "MopGuildPackets::BuildGuildMemberRankUpdate(data"
        "MopGuildPackets::BuildGuildNewLeader(data"
        "MopGuildPackets::BuildGuildPlayerLeft(data"
        "MopGuildPackets::BuildGuildDisbanded(data)")
    string(FIND "${guild_sender}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "typed guild-event sender missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "DefS(SMSG_GUILD_EVENT_PLAYER_JOINED, \"SMSG_GUILD_EVENT_PLAYER_JOINED\");"
        "DefS(SMSG_GUILD_EVENT_PRESENCE_CHANGE, \"SMSG_GUILD_EVENT_PRESENCE_CHANGE\");"
        "DefS(SMSG_GUILD_EVENT_PLAYER_LEFT, \"SMSG_GUILD_EVENT_PLAYER_LEFT\");"
        "DefS(SMSG_GUILD_RANKS_UPDATE, \"SMSG_GUILD_RANKS_UPDATE\");"
        "DefS(SMSG_GUILD_EVENT_NEW_LEADER, \"SMSG_GUILD_EVENT_NEW_LEADER\");"
        "DefS(SMSG_GUILD_EVENT_DISBANDED, \"SMSG_GUILD_EVENT_DISBANDED\");")
    string(FIND "${opcode_registry}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "typed guild-event registration missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "case SMSG_GUILD_EVENT_PLAYER_JOINED:"
        "case SMSG_GUILD_EVENT_PRESENCE_CHANGE:"
        "case SMSG_GUILD_EVENT_PLAYER_LEFT:"
        "case SMSG_GUILD_RANKS_UPDATE:"
        "case SMSG_GUILD_EVENT_NEW_LEADER:"
        "case SMSG_GUILD_EVENT_DISBANDED:")
    string(FIND "${world_session}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "typed guild-event allowlist entry missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "SMSG_GUILD_RANKS_UPDATE                      = 0x0A60"
        "SMSG_GUILD_EVENT_PLAYER_JOINED               = 0x0B69"
        "SMSG_GUILD_EVENT_PRESENCE_CHANGE             = 0x0B70"
        "SMSG_GUILD_EVENT_PLAYER_LEFT                 = 0x0BF8"
        "SMSG_GUILD_EVENT_NEW_LEADER                  = 0x0E69"
        "SMSG_GUILD_EVENT_DISBANDED                   = 0x1E68")
    string(FIND "${opcode_header}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "typed guild-event opcode missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "guild->BroadcastMemberPresence(pCurrChar->GetObjectGuid(), pCurrChar->GetName(), true);"
        "guild->BroadcastMemberPresence(_player->GetObjectGuid(), _player->GetName(), false);")
    string(FIND "${login_sender}${world_session}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "guild presence call missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "ObjectGuid const removedGuid = slot->guid;"
        "std::string const removedName = slot->Name;"
        "guild->DelMember(removedGuid)"
        "guild->BroadcastMemberRemoved(removedGuid, removedName")
    string(FIND "${guild_handler}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "safe typed guild removal missing: ${token}")
    endif()
endforeach()

file(GLOB_RECURSE guild_production_sources
    "${SOURCE_ROOT}/src/game/*.cpp"
    "${SOURCE_ROOT}/src/game/*.h")
list(FILTER guild_production_sources EXCLUDE REGEX "/tests/")

set(legacy_patterns
    "(^|[^A-Z0-9_])CMSG_GUILD_INFO([^A-Z0-9_]|$)"
    "(^|[^A-Z0-9_])SMSG_GUILD_INFO([^A-Z0-9_]|$)"
    "(^|[^A-Z0-9_])SMSG_GUILD_EVENT([^A-Z0-9_]|$)"
    "(^|[^A-Za-z0-9_])HandleGuildInfoOpcode([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])BroadcastEvent([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])GuildEvents([^A-Za-z0-9_]|$)")

foreach(path IN LISTS guild_production_sources)
    if(path MATCHES "Opcodes_reference\\.h$")
        continue()
    endif()
    file(READ "${path}" source)
    foreach(pattern IN LISTS legacy_patterns)
        string(REGEX MATCH "${pattern}" found "${source}")
        if(found)
            message(FATAL_ERROR "legacy generic guild packet remains in ${path}: ${pattern}")
        endif()
    endforeach()
endforeach()

set(mutation_source "${opcode_header}\n${guild_sender}")
foreach(pattern IN LISTS legacy_patterns)
    string(REGEX MATCH "${pattern}" found "${mutation_source}")
    if(found)
        message(FATAL_ERROR "legacy guild mutation escaped: ${pattern}")
    endif()
endforeach()
