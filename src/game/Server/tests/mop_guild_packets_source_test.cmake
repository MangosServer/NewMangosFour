file(READ "${SOURCE_ROOT}/src/game/Object/Guild.h" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" login_sender)
file(READ "${SOURCE_ROOT}/src/game/Object/Guild.cpp" guild_sender)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)

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

string(FIND "${login_sender}" "data.Initialize(SMSG_GUILD_EVENT," legacy_login)
if(NOT legacy_login EQUAL -1)
    message(FATAL_ERROR "login still sends the legacy generic guild event")
endif()
