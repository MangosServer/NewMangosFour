file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Channel.cpp" channel_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Channel.h" channel_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "list_realm")
    string(REPLACE
        "members.push_back({i->first.GetRawValue(), realmID, i->second.flags});"
        "members.push_back({i->first.GetRawValue(), 0, i->second.flags});"
        channel_source "${channel_source}")
elseif(MUTATION STREQUAL "notify_guid_identity")
    string(REPLACE
        "MopChannelPackets::WriteGuidIdentity(*data, guid.GetRawValue(), realmID);"
        "*data << guid.GetRawValue();"
        channel_source "${channel_source}")
elseif(MUTATION STREQUAL "wrong_faction_tail")
    string(REPLACE
        "MANGOS_ASSERT(MopChannelPackets::WriteNameIdentity(*data, name, realmID));"
        "/* removed wrong-faction identity */"
        channel_source "${channel_source}")
elseif(MUTATION STREQUAL "you_joined_tail")
    string(REPLACE
        "*data << uint8(GetFlags());"
        "*data << uint32(GetFlags());"
        channel_source "${channel_source}")
elseif(MUTATION STREQUAL "you_left_tail")
    string(REPLACE
        "*data << uint8(0);                                      // can be 0x00 and 0x01"
        "*data << uint32(0);                                     // mutated width"
        channel_source "${channel_source}")
elseif(MUTATION STREQUAL "list_registration")
    string(REPLACE
        "DefS(SMSG_CHANNEL_LIST, \"SMSG_CHANNEL_LIST\");"
        "/* removed list registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "notify_registration")
    string(REPLACE
        "DefS(SMSG_CHANNEL_NOTIFY, \"SMSG_CHANNEL_NOTIFY\");"
        "/* removed notify registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "list_gate")
    string(REPLACE
        "case SMSG_CHANNEL_LIST:"
        "/* removed list gate */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "notify_gate")
    string(REPLACE
        "case SMSG_CHANNEL_NOTIFY:"
        "/* removed notify gate */"
        session_source "${session_source}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once("${channel_header}"
    "static size_t const CHANNEL_NAME_MAX[ \t]*=[ \t]*127"
    "channel-name reader bound")
require_once("${channel_header}"
    "static size_t const PLAYER_NAME_MAX[ \t]*=[ \t]*512"
    "player-name reader bound")
require_once("${channel_source}"
    "members\\.push_back\\(\\{i->first\\.GetRawValue\\(\\), realmID, i->second\\.flags\\}\\)"
    "channel-list realm identity")
require_once("${channel_source}"
    "MopChannelPackets::BuildList\\(data, GetName\\(\\), GetFlags\\(\\), members\\)"
    "channel-list production builder")
require_once("${channel_source}"
    "MakeInviteWrongFaction\\(&data, targetName\\)"
    "wrong-faction caller supplies target name")
string(FIND "${channel_source}" "void Channel::MakeYouJoined" you_joined_start)
string(FIND "${channel_source}" "void Channel::MakeYouLeft" you_joined_end)
if(you_joined_start EQUAL -1 OR you_joined_end EQUAL -1 OR
        you_joined_end LESS_EQUAL you_joined_start)
    message(FATAL_ERROR "could not isolate you-joined serializer")
endif()
math(EXPR you_joined_length "${you_joined_end} - ${you_joined_start}")
string(SUBSTRING "${channel_source}" ${you_joined_start}
    ${you_joined_length} you_joined_serializer)
require_once("${you_joined_serializer}"
    "\\*data << uint8\\(GetFlags\\(\\)\\)"
    "you-joined flags field")
require_once("${you_joined_serializer}"
    "\\*data << uint32\\(GetChannelId\\(\\)\\)"
    "you-joined channel ID")
require_once("${you_joined_serializer}"
    "\\*data << uint32\\(0\\)"
    "you-joined final field")

string(FIND "${channel_source}" "void Channel::MakeYouLeft" you_left_start)
string(FIND "${channel_source}" "void Channel::MakeWrongPassword" you_left_end)
if(you_left_start EQUAL -1 OR you_left_end EQUAL -1 OR
        you_left_end LESS_EQUAL you_left_start)
    message(FATAL_ERROR "could not isolate you-left serializer")
endif()
math(EXPR you_left_length "${you_left_end} - ${you_left_start}")
string(SUBSTRING "${channel_source}" ${you_left_start}
    ${you_left_length} you_left_serializer)
require_once("${you_left_serializer}"
    "\\*data << uint32\\(GetChannelId\\(\\)\\)"
    "you-left channel ID")
require_once("${you_left_serializer}"
    "\\*data << uint8\\(0\\)"
    "you-left suspended field")
string(FIND "${channel_source}"
    "void Channel::MakeInviteWrongFaction" wrong_faction_start)
string(FIND "${channel_source}"
    "void Channel::MakeWrongFaction" wrong_faction_end)
if(wrong_faction_start EQUAL -1 OR wrong_faction_end EQUAL -1 OR
        wrong_faction_end LESS_EQUAL wrong_faction_start)
    message(FATAL_ERROR "could not isolate wrong-faction invite serializer")
endif()
math(EXPR wrong_faction_length "${wrong_faction_end} - ${wrong_faction_start}")
string(SUBSTRING "${channel_source}" ${wrong_faction_start}
    ${wrong_faction_length} wrong_faction_serializer)
require_once("${wrong_faction_serializer}"
    "MopChannelPackets::WriteNameIdentity\\(\\*data, name, realmID\\)"
    "wrong-faction name identity")

string(REGEX MATCHALL
    "MopChannelPackets::WriteGuidIdentity"
    guid_identity_calls "${channel_source}")
list(LENGTH guid_identity_calls guid_identity_count)
if(NOT guid_identity_count EQUAL 19)
    message(FATAL_ERROR
        "expected 19 production GUID identity writes, found ${guid_identity_count}")
endif()

require_once("${opcode_registry}"
    "DefS\\(SMSG_CHANNEL_LIST,[ \t]*\"SMSG_CHANNEL_LIST\"\\)"
    "channel-list registration")
require_once("${opcode_registry}"
    "DefS\\(SMSG_CHANNEL_NOTIFY,[ \t]*\"SMSG_CHANNEL_NOTIFY\"\\)"
    "channel-notify registration")
require_once("${session_source}"
    "case[ \t]+SMSG_CHANNEL_LIST:"
    "channel-list suppression gate")
require_once("${session_source}"
    "case[ \t]+SMSG_CHANNEL_NOTIFY:"
    "channel-notify suppression gate")
