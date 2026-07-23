file(READ "${SOURCE_ROOT}/src/game/Object/Calendar.h" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/Object/Guild.cpp" guild_sender)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CalendarHandler.cpp" calendar_sender)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(MUTATION STREQUAL "initial_builder")
    string(REPLACE
        "MopCalendarPackets::BuildCalendarInitialInvite(data, records)"
        "false /* removed initial-invite builder */"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "status_builder")
    string(REPLACE
        "MopCalendarPackets::BuildCalendarInviteStatus(data, record);"
        "/* removed invite-status builder */"
        calendar_sender "${calendar_sender}")
elseif(MUTATION STREQUAL "moderator_builder")
    string(REPLACE
        "MopCalendarPackets::BuildCalendarModeratorStatus(data, record);"
        "/* removed moderator-status builder */"
        calendar_sender "${calendar_sender}")
elseif(MUTATION STREQUAL "initial_registration")
    string(REPLACE
        "DefS(SMSG_CALENDAR_EVENT_INITIAL_INVITE, \"SMSG_CALENDAR_EVENT_INITIAL_INVITE\");"
        "/* removed initial-invite registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "status_registration")
    string(REPLACE
        "DefS(SMSG_CALENDAR_EVENT_INVITE_STATUS, \"SMSG_CALENDAR_EVENT_INVITE_STATUS\");"
        "/* removed invite-status registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "moderator_registration")
    string(REPLACE
        "DefS(SMSG_CALENDAR_EVENT_MODERATOR_STATUS, \"SMSG_CALENDAR_EVENT_MODERATOR_STATUS\");"
        "/* removed moderator-status registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "pending_client_registration")
    string(REPLACE
        "DefC(CMSG_CALENDAR_GET_NUM_PENDING, \"CMSG_CALENDAR_GET_NUM_PENDING\""
        "/* removed pending-count client registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "pending_server_registration")
    string(REPLACE
        "DefS(SMSG_CALENDAR_SEND_NUM_PENDING, \"SMSG_CALENDAR_SEND_NUM_PENDING\");"
        "/* removed pending-count server registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "pending_gate")
    string(REPLACE
        "case SMSG_CALENDAR_SEND_NUM_PENDING:"
        "/* removed pending-count framing gate */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "list_builder")
    string(REPLACE
        "MopCalendarPackets::BuildCalendarList(data, eventRecords, inviteRecords, lockoutRecords, resetRecords,"
        "false /* removed calendar-list builder */"
        calendar_sender "${calendar_sender}")
elseif(MUTATION STREQUAL "event_builder")
    string(REPLACE
        "MopCalendarPackets::BuildCalendarEvent(data, eventRecord, inviteRecords)"
        "false /* removed selected-event builder */"
        calendar_sender "${calendar_sender}")
elseif(MUTATION STREQUAL "get_calendar_registration")
    string(REPLACE
        "DefC(CMSG_CALENDAR_GET_CALENDAR, \"CMSG_CALENDAR_GET_CALENDAR\""
        "/* removed get-calendar registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "get_event_registration")
    string(REPLACE
        "DefC(CMSG_CALENDAR_GET_EVENT, \"CMSG_CALENDAR_GET_EVENT\""
        "/* removed get-event registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "send_calendar_registration")
    string(REPLACE
        "DefS(SMSG_CALENDAR_SEND_CALENDAR, \"SMSG_CALENDAR_SEND_CALENDAR\");"
        "/* removed send-calendar registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "send_event_registration")
    string(REPLACE
        "DefS(SMSG_CALENDAR_SEND_EVENT, \"SMSG_CALENDAR_SEND_EVENT\");"
        "/* removed send-event registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "send_calendar_gate")
    string(REPLACE
        "case SMSG_CALENDAR_SEND_CALENDAR:"
        "/* removed send-calendar framing gate */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "send_event_gate")
    string(REPLACE
        "case SMSG_CALENDAR_SEND_EVENT:"
        "/* removed send-event framing gate */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "get_event_size_guard")
    string(REPLACE
        "if (recv_data.size() < sizeof(uint64))"
        "if (false /* removed get-event size guard */)"
        calendar_sender "${calendar_sender}")
elseif(MUTATION STREQUAL "legacy_list_body")
    string(REPLACE
        "MopCalendarPackets::BuildCalendarList(data, eventRecords, inviteRecords, lockoutRecords, resetRecords,"
        "MopCalendarPackets::BuildCalendarList(data, eventRecords, inviteRecords, lockoutRecords, resetRecords, /* mutation */ data << uint32(invites.size());"
        calendar_sender "${calendar_sender}")
elseif(MUTATION STREQUAL "legacy_event_body")
    string(REPLACE
        "MopCalendarPackets::BuildCalendarEvent(data, eventRecord, inviteRecords)"
        "MopCalendarPackets::BuildCalendarEvent(data, eventRecord, inviteRecords) /* mutation */; data << uint8(sendType)"
        calendar_sender "${calendar_sender}")
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

require_once("${guild_sender}"
    "MopCalendarPackets::BuildCalendarInitialInvite(data, records)"
    "initial-invite builder call")
require_once("${calendar_sender}"
    "MopCalendarPackets::BuildCalendarInviteStatus(data, record);"
    "invite-status builder call")
require_once("${calendar_sender}"
    "MopCalendarPackets::BuildCalendarModeratorStatus(data, record);"
    "moderator-status builder call")
require_once("${opcode_registry}"
    "DefS(SMSG_CALENDAR_EVENT_INITIAL_INVITE, \"SMSG_CALENDAR_EVENT_INITIAL_INVITE\");"
    "initial-invite registration")
require_once("${opcode_registry}"
    "DefS(SMSG_CALENDAR_EVENT_INVITE_STATUS, \"SMSG_CALENDAR_EVENT_INVITE_STATUS\");"
    "invite-status registration")
require_once("${opcode_registry}"
    "DefS(SMSG_CALENDAR_EVENT_MODERATOR_STATUS, \"SMSG_CALENDAR_EVENT_MODERATOR_STATUS\");"
    "moderator-status registration")
require_once("${opcode_registry}"
    "DefC(CMSG_CALENDAR_GET_NUM_PENDING, \"CMSG_CALENDAR_GET_NUM_PENDING\""
    "pending-count client registration")
require_once("${opcode_registry}"
    "DefS(SMSG_CALENDAR_SEND_NUM_PENDING, \"SMSG_CALENDAR_SEND_NUM_PENDING\");"
    "pending-count server registration")
require_once("${calendar_sender}"
    "WorldPacket data(SMSG_CALENDAR_SEND_NUM_PENDING, 4);"
    "pending-count response body")
require_once("${world_session}"
    "case SMSG_CALENDAR_SEND_NUM_PENDING:"
    "pending-count framing gate")
require_once("${calendar_sender}"
    "MopCalendarPackets::BuildCalendarList(data, eventRecords, inviteRecords, lockoutRecords, resetRecords,"
    "calendar-list builder call")
require_once("${calendar_sender}"
    "MopCalendarPackets::BuildCalendarEvent(data, eventRecord, inviteRecords)"
    "selected-event builder call")
require_once("${opcode_registry}"
    "DefC(CMSG_CALENDAR_GET_CALENDAR, \"CMSG_CALENDAR_GET_CALENDAR\""
    "get-calendar registration")
require_once("${opcode_registry}"
    "DefC(CMSG_CALENDAR_GET_EVENT, \"CMSG_CALENDAR_GET_EVENT\""
    "get-event registration")
require_once("${opcode_registry}"
    "DefS(SMSG_CALENDAR_SEND_CALENDAR, \"SMSG_CALENDAR_SEND_CALENDAR\");"
    "send-calendar registration")
require_once("${opcode_registry}"
    "DefS(SMSG_CALENDAR_SEND_EVENT, \"SMSG_CALENDAR_SEND_EVENT\");"
    "send-event registration")
require_once("${world_session}"
    "case SMSG_CALENDAR_SEND_CALENDAR:"
    "send-calendar framing gate")
require_once("${world_session}"
    "case SMSG_CALENDAR_SEND_EVENT:"
    "send-event framing gate")
require_once("${calendar_sender}"
    "if (recv_data.size() < sizeof(uint64))"
    "get-event minimum-size guard")

foreach(legacy IN ITEMS
        "data << uint32(invites.size())"
        "data << uint8(sendType)")
    string(FIND "${calendar_sender}" "${legacy}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "legacy primary calendar body remains: ${legacy}")
    endif()
endforeach()

foreach(source IN ITEMS "${guild_sender}" "${calendar_sender}" "${opcode_header}")
    foreach(legacy IN ITEMS
            SMSG_CALENDAR_FILTER_GUILD
            SMSG_CALENDAR_EVENT_STATUS
            SMSG_CALENDAR_EVENT_MODERATOR_STATUS_ALERT)
        string(FIND "${source}" "${legacy}" position)
        if(NOT position EQUAL -1)
            message(FATAL_ERROR "obsolete calendar path remains: ${legacy}")
        endif()
    endforeach()
endforeach()

foreach(token IN ITEMS
        "out.WriteBits(entries.size(), 23)"
        "out.WriteBit(record.displayPendingAction)"
        "out << record.status"
        "out << record.eventTime"
        "out << record.eventId")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "calendar builder missing: ${token}")
    endif()
endforeach()
