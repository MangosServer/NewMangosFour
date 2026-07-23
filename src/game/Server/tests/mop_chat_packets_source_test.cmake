file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Chat.h" chat_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ChatMessage.cpp" chat_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ChatHandler.cpp" chat_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ChannelHandler.cpp" channel_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Channel.cpp" channel_sender)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerChat.cpp" player_chat)
file(READ "${SOURCE_ROOT}/src/game/Object/Guild.cpp" guild_chat)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Group.cpp" group_chat)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit_emote)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(MUTATION STREQUAL "builder")
    string(REPLACE
        "MopChatPackets::BuildMessage(data, packet)"
        "false /* removed chat builder */"
        chat_builder "${chat_builder}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE
        "DefS(SMSG_MESSAGECHAT, \"SMSG_MESSAGECHAT\");"
        "/* removed chat registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE
        "case SMSG_MESSAGECHAT:"
        "case 0xFFFF: /* removed chat allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "emote_builder")
    string(REPLACE
        "MopChatPackets::BuildTextEmote(data,"
        "false /* removed text-emote builder */ (data,"
        chat_handler "${chat_handler}")
elseif(MUTATION STREQUAL "emote_reader")
    string(REPLACE
        "MopChatPackets::ReadTextEmoteRequest(recv_data)"
        "MopChatPackets::TextEmoteRequest{} /* removed text-emote reader */"
        chat_handler "${chat_handler}")
elseif(MUTATION STREQUAL "emote_registration")
    string(REPLACE
        "DefC(CMSG_TEXT_EMOTE, \"CMSG_TEXT_EMOTE\""
        "DefC(0xFFFF, \"removed CMSG_TEXT_EMOTE\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "emote_allowlist")
    string(REPLACE
        "case SMSG_TEXT_EMOTE:"
        "case 0xFFFF: /* removed text-emote allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "emote_body")
    string(REPLACE
        "data << uint32(emote_id);"
        "data << uint16(emote_id); /* damaged emote body */"
        unit_emote "${unit_emote}")
elseif(MUTATION STREQUAL "cross_faction_denial")
    string(REPLACE
        "if (GetPlayer()->GetTeam() != player->GetTeam())"
        "if (false /* removed cross-faction denial */)"
        chat_handler "${chat_handler}")
elseif(MUTATION STREQUAL "text_bound")
    string(REPLACE
        "message.text.size() >= (size_t(1) << 12)"
        "false /* removed text bound */"
        chat_header "${chat_header}")
elseif(MUTATION STREQUAL "receiver_identity")
    string(REPLACE
        "GetObjectGuid(), GetName(), GetObjectGuid(), GetName()"
        "GetObjectGuid(), GetName(), ObjectGuid(), NULL"
        player_chat "${player_chat}")
elseif(MUTATION STREQUAL "addon_identity")
    string(REPLACE
        "receiver->GetSession()->SendPacket(&data);"
        "_player->GetSession()->SendPacket(&data); /* wrong addon destination */"
        chat_handler "${chat_handler}")
elseif(MUTATION STREQUAL "addon_count_width")
    string(REPLACE
        "uint32 const count = in.ReadBits(24);"
        "uint32 const count = in.ReadBits(16); /* damaged addon count */"
        chat_header "${chat_header}")
elseif(MUTATION STREQUAL "addon_length_width")
    string(REPLACE
        "lengths.push_back(uint8(in.ReadBits(5)));"
        "lengths.push_back(uint8(in.ReadBits(6))); /* damaged addon length */"
        chat_header "${chat_header}")
elseif(MUTATION STREQUAL "addon_softcap")
    string(REPLACE
        "count > 64 || prefixes.size() + count > 64"
        "false /* removed addon softcap */"
        chat_header "${chat_header}")
elseif(MUTATION STREQUAL "addon_unregister_registration")
    string(REPLACE
        "DefC(CMSG_UNREGISTER_ALL_ADDON_PREFIXES, \"CMSG_UNREGISTER_ALL_ADDON_PREFIXES\""
        "DefC(0xFFFF, \"removed CMSG_UNREGISTER_ALL_ADDON_PREFIXES\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "addon_batch_registration")
    string(REPLACE
        "DefC(CMSG_ADDON_REGISTERED_PREFIXES, \"CMSG_ADDON_REGISTERED_PREFIXES\""
        "DefC(0xFFFF, \"removed CMSG_ADDON_REGISTERED_PREFIXES\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "addon_unregister_handler")
    string(REPLACE
        "m_registeredAddonPrefixes.clear();"
        "/* removed registered-prefix clear */"
        chat_handler "${chat_handler}")
elseif(MUTATION STREQUAL "addon_batch_handler")
    string(REPLACE
        "MopChatPackets::ReadAddonPrefixBatch("
        "false /* removed registered-prefix reader */ && ("
        chat_handler "${chat_handler}")
elseif(MUTATION STREQUAL "addon_group_filter")
    string(REPLACE
        "group->BroadcastAddonMessagePacket(&data, prefix, false);"
        "group->BroadcastPacket(&data, false); /* removed addon group filter */"
        chat_handler "${chat_handler}")
elseif(MUTATION STREQUAL "addon_group_delivery")
    string(REPLACE
        "session->IsAddonRegistered(prefix)"
        "true /* removed addon group delivery filter */"
        group_chat "${group_chat}")
elseif(MUTATION STREQUAL "addon_guild_filter")
    string(REPLACE
        "pl->GetSession()->IsAddonRegistered(prefix)"
        "true /* removed addon guild filter */"
        guild_chat "${guild_chat}")
elseif(MUTATION STREQUAL "addon_whisper_filter")
    string(REPLACE
        "if (receiver->GetSession()->IsAddonRegistered(prefix))"
        "if (true /* removed addon whisper filter */)"
        chat_handler "${chat_handler}")
elseif(MUTATION STREQUAL "afk_registration")
    string(REPLACE
        "DefC(CMSG_MESSAGECHAT_AFK, \"CMSG_MESSAGECHAT_AFK\""
        "DefC(0xFFFF, \"removed CMSG_MESSAGECHAT_AFK\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "afk_parser_wiring")
    string(REPLACE
        "MopChatPackets::ReadAfkMessageRequest(recv_data, msg)"
        "false /* removed AFK parser */"
        chat_handler "${chat_handler}")
elseif(MUTATION STREQUAL "afk_parser_layout")
    string(REPLACE
        "uint8 const length = in.ReadUInt8();"
        "uint8 const length = uint8(in.ReadBits(9)); /* damaged AFK length */"
        chat_header "${chat_header}")
elseif(MUTATION STREQUAL "legacy_wrong_faction")
    string(APPEND opcode_header "\nWorldPacket legacy(SMSG_CHAT_WRONG_FACTION);\n")
elseif(MUTATION STREQUAL "legacy_gm_chat")
    string(APPEND chat_builder "\nWorldPacket legacy(SMSG_GM_MESSAGECHAT);\n")
elseif(MUTATION STREQUAL "legacy_member_count")
    string(APPEND channel_handler "\nvoid WorldSession::HandleGetChannelMemberCountOpcode(WorldPacket&) {}\n")
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

function(require_count source token expected context)
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
    if(NOT count EQUAL expected)
        message(FATAL_ERROR "${context}: expected ${expected} active occurrences, found ${count}")
    endif()
endfunction()

function(forbid source token context)
    string(FIND "${source}" "${token}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden token remains: ${token}")
    endif()
endfunction()

require_once("${chat_builder}"
    "MopChatPackets::BuildMessage(data, packet)"
    "production chat builder call")
require_once("${opcode_registry}"
    "DefS(SMSG_MESSAGECHAT, \"SMSG_MESSAGECHAT\");"
    "generic chat opcode registration")
require_once("${world_session}"
    "case SMSG_MESSAGECHAT:"
    "generic chat suppression allowlist")
require_once("${chat_handler}"
    "MopChatPackets::BuildTextEmote(data,"
    "text-emote response builder call")
require_once("${chat_handler}"
    "MopChatPackets::ReadTextEmoteRequest(recv_data)"
    "text-emote request reader call")
require_once("${opcode_registry}"
    "DefC(CMSG_TEXT_EMOTE, \"CMSG_TEXT_EMOTE\""
    "text-emote request registration")
require_once("${opcode_registry}"
    "DefC(CMSG_EMOTE, \"CMSG_EMOTE\""
    "emote request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_TEXT_EMOTE, \"SMSG_TEXT_EMOTE\");"
    "text-emote response registration")
require_once("${opcode_registry}"
    "DefS(SMSG_EMOTE, \"SMSG_EMOTE\");"
    "emote response registration")
require_once("${world_session}"
    "case SMSG_TEXT_EMOTE:"
    "text-emote response allowlist")
require_once("${world_session}"
    "case SMSG_EMOTE:"
    "emote response allowlist")
require_once("${unit_emote}"
    "data << uint32(emote_id);"
    "emote response uint32 field")

string(FIND "${unit_emote}" "data << uint32(emote_id);" emote_id_position)
string(FIND "${unit_emote}" "data << GetObjectGuid();" emote_guid_position)
if(emote_id_position EQUAL -1 OR emote_guid_position EQUAL -1 OR
        NOT emote_id_position LESS emote_guid_position)
    message(FATAL_ERROR "emote response must write uint32 before plain GUID")
endif()
require_once("${chat_handler}"
    "if (GetPlayer()->GetTeam() != player->GetTeam())"
    "cross-faction whisper denial")
require_once("${player_chat}"
    "ChatHandler::BuildChatPacket(data, CHAT_MSG_SAY, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName(), GetObjectGuid(), GetName());"
    "player chat sender/receiver identity")
require_once("${channel_sender}"
    "guid, player->GetName(), guid, player->GetName(), m_name.c_str()"
    "channel chat sender/receiver identity")
require_once("${chat_handler}"
    "receiver->GetSession()->SendPacket(&data);"
    "addon whisper recipient")
require_once("${chat_header}"
    "uint32 const count = in.ReadBits(24);"
    "registered-addon-prefix 24-bit count")
require_once("${chat_header}"
    "lengths.push_back(uint8(in.ReadBits(5)));"
    "registered-addon-prefix 5-bit lengths")
require_once("${chat_header}"
    "count > 64 || prefixes.size() + count > 64"
    "registered-addon-prefix soft cap")
require_once("${opcode_registry}"
    "DefC(CMSG_UNREGISTER_ALL_ADDON_PREFIXES, \"CMSG_UNREGISTER_ALL_ADDON_PREFIXES\""
    "unregister-addon-prefix opcode registration")
require_once("${opcode_registry}"
    "DefC(CMSG_ADDON_REGISTERED_PREFIXES, \"CMSG_ADDON_REGISTERED_PREFIXES\""
    "registered-addon-prefix opcode registration")
require_once("${opcode_registry}"
    "DefC(CMSG_MESSAGECHAT_AFK, \"CMSG_MESSAGECHAT_AFK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMessagechatOpcode);"
    "AFK request registration")
require_once("${chat_handler}"
    "MopChatPackets::ReadAfkMessageRequest(recv_data, msg)"
    "AFK request parser wiring")
require_once("${chat_header}"
    "uint8 const length = in.ReadUInt8();"
    "AFK request uint8 length")
require_once("${chat_header}"
    "bool ReadAfkMessageRequest"
    "AFK request parser")

string(FIND "${chat_handler}" "case CHAT_MSG_AFK:" afk_case_position)
string(FIND "${chat_handler}" "case CHAT_MSG_DND:" dnd_case_position)
if(afk_case_position EQUAL -1 OR dnd_case_position EQUAL -1 OR NOT afk_case_position LESS dnd_case_position)
    message(FATAL_ERROR "AFK/DND chat branches missing or reordered")
endif()
math(EXPR afk_length "${dnd_case_position} - ${afk_case_position}")
string(SUBSTRING "${chat_handler}" ${afk_case_position} ${afk_length} afk_branch)
forbid("${afk_branch}" "ReadBits(9)" "AFK branch must not use generic 9-bit length")
require_once("${chat_handler}"
    "m_registeredAddonPrefixes.clear();"
    "unregister-addon-prefix state clear")
require_once("${chat_handler}"
    "MopChatPackets::ReadAddonPrefixBatch("
    "registered-addon-prefix handler reader")
require_once("${chat_handler}"
    "if (receiver->GetSession()->IsAddonRegistered(prefix))"
    "addon whisper recipient filter")
require_once("${chat_handler}"
    "group->BroadcastAddonMessagePacket(&data, prefix, false);"
    "addon battleground-group recipient filter")
require_count("${chat_handler}"
    "BroadcastAddonMessagePacket(&data, prefix, false"
    2
    "addon group recipient filters")
require_count("${guild_chat}"
    "pl->GetSession()->IsAddonRegistered(prefix)"
    2
    "addon guild recipient filters")
require_once("${group_chat}"
    "session->IsAddonRegistered(prefix)"
    "addon group delivery filter")
require_once("${opcode_header}"
    "CMSG_UNREGISTER_ALL_ADDON_PREFIXES           = 0x029F"
    "unregister-addon-prefix opcode value")
require_once("${opcode_header}"
    "CMSG_ADDON_REGISTERED_PREFIXES               = 0x040E"
    "registered-addon-prefix opcode value")

foreach(source IN ITEMS "${chat_handler}" "${guild_chat}")
    string(FIND "${source}" "prefix.c_str()" prefix_position)
    string(FIND "${source}" "GetObjectGuid()" sender_position)
    if(prefix_position EQUAL -1 OR sender_position EQUAL -1)
        message(FATAL_ERROR "addon chat sender/prefix wiring missing")
    endif()
endforeach()

foreach(token IN ITEMS
        "message.senderName.size() >= (size_t(1) << 11)"
        "message.receiverName.size() >= (size_t(1) << 11)"
        "message.channelName.size() >= (size_t(1) << 7)"
        "message.addonPrefix.size() >= (size_t(1) << 5)"
        "message.text.size() >= (size_t(1) << 12)"
        "message.chatTag >= (uint32(1) << 9)"
        "message.chatType == CHAT_MSG_ACHIEVEMENT"
        "message.chatType == CHAT_MSG_GUILD_ACHIEVEMENT"
        "sObjectAccessor.FindPlayer(senderGuid)"
        "packet.groupGuid = sender->GetGroup()->GetObjectGuid().GetRawValue()"
        "packet.guildGuid = sender->GetGuildGuid().GetRawValue()")
    string(FIND "${chat_header}${chat_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "chat migration invariant missing: ${token}")
    endif()
endforeach()

set(production "${chat_header}${chat_builder}${chat_handler}${channel_handler}${channel_sender}${player_chat}${session_header}${opcode_header}")
foreach(token IN ITEMS
        "SMSG_CHAT_WRONG_FACTION"
        "SMSG_GM_MESSAGECHAT"
        "SMSG_CHANNEL_MEMBER_COUNT"
        "SendWrongFactionNotice"
        "HandleGetChannelMemberCountOpcode"
        "isGM ? SMSG")
    forbid("${production}" "${token}" "legacy chat/channel cleanup")
endforeach()
