file(READ "${SOURCE_ROOT}/src/game/Object/Guild.h" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" login_sender)
file(READ "${SOURCE_ROOT}/src/game/Object/Guild.cpp" guild_sender)
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
