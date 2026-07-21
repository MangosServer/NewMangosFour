file(READ "${SOURCE_ROOT}/src/game/Object/Calendar.h" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/Object/Guild.cpp" guild_sender)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CalendarHandler.cpp" calendar_sender)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)

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
