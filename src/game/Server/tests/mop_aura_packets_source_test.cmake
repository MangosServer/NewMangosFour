file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellAuras.h" aura_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellAuras.cpp" aura_source)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "full_update_mode")
    string(REPLACE
        "target->GetObjectGuid().GetRawValue(), true, updates"
        "target->GetObjectGuid().GetRawValue(), false, updates"
        player_source "${player_source}")
elseif(MUTATION STREQUAL "flag_translation")
    string(REPLACE
        "legacyFlags & AFLAG_NOT_CASTER"
        "legacyFlags & AFLAG_POSITIVE"
        aura_source "${aura_source}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE
        "DefS(SMSG_AURA_UPDATE, \"SMSG_AURA_UPDATE\");"
        "DefS_disabled(SMSG_AURA_UPDATE, \"SMSG_AURA_UPDATE\");"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "whitelist")
    string(REPLACE
        "case SMSG_AURA_UPDATE:"
        "case SMSG_AURA_UPDATE_disabled:"
        session_source "${session_source}")
endif()

function(strip_cpp_comments output source)
    set(text "${source}")
    while(TRUE)
        string(FIND "${text}" "/*" start)
        if(start EQUAL -1)
            break()
        endif()
        math(EXPR tail_start "${start} + 2")
        string(SUBSTRING "${text}" ${tail_start} -1 tail)
        string(FIND "${tail}" "*/" stop)
        if(stop EQUAL -1)
            message(FATAL_ERROR "Unterminated block comment")
        endif()
        string(SUBSTRING "${text}" 0 ${start} before)
        math(EXPR after_start "${stop} + 2")
        string(SUBSTRING "${tail}" ${after_start} -1 after)
        set(text "${before}${after}")
    endwhile()
    string(REGEX REPLACE "//[^\r\n]*" "" text "${text}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

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
        math(EXPR next "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next} -1 remaining)
    endwhile()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected once, found ${count}: ${token}")
    endif()
endfunction()

strip_cpp_comments(aura_header "${aura_header}")
strip_cpp_comments(aura_source "${aura_source}")
strip_cpp_comments(player_source "${player_source}")
strip_cpp_comments(opcode_header "${opcode_header}")
strip_cpp_comments(opcode_registry "${opcode_registry}")
strip_cpp_comments(session_source "${session_source}")

foreach(required IN ITEMS
        "out.WriteBit(GuidByte(targetGuid, 7))"
        "out.WriteBit(fullUpdate)"
        "out.WriteBits(uint32(updates.size()), 24)"
        "out.WriteBits(uint32(itr->effectAmounts.size()), 22)"
        "out.WriteBits(uint32(0), 22)"
        "out.WriteByteSeq(GuidByte(targetGuid, 2))"
        "out.WriteByteSeq(GuidByte(targetGuid, 5))")
    string(FIND "${aura_header}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "18414 aura grammar missing: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "legacyFlags & AFLAG_NOT_CASTER"
        "update.flags |= MopAuraPackets::AURA_WIRE_CASTER"
        "legacyFlags & AFLAG_POSITIVE"
        "update.flags |= MopAuraPackets::AURA_WIRE_POSITIVE"
        "legacyFlags & AFLAG_DURATION"
        "update.flags |= MopAuraPackets::AURA_WIRE_DURATION"
        "legacyFlags & AFLAG_EFFECT_AMOUNT_SEND"
        "update.flags |= MopAuraPackets::AURA_WIRE_EFFECT_AMOUNT"
        "legacyFlags & AFLAG_NEGATIVE"
        "update.flags |= MopAuraPackets::AURA_WIRE_NEGATIVE")
    string(FIND "${aura_source}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "legacy-to-18414 aura flag translation missing: ${required}")
    endif()
endforeach()

require_once("${aura_source}"
    "MopAuraPackets::BuildAuraUpdate(data, m_target->GetObjectGuid().GetRawValue(), false, updates)"
    "incremental aura sender")
require_once("${player_source}"
    "MopAuraPackets::BuildAuraUpdate(data, target->GetObjectGuid().GetRawValue(), true, updates)"
    "full aura snapshot sender")
require_once("${opcode_header}" "SMSG_AURA_UPDATE                             = 0x0072" "18414 aura opcode")
require_once("${opcode_registry}" "DefS(SMSG_AURA_UPDATE, \"SMSG_AURA_UPDATE\");" "aura opcode metadata")
require_once("${session_source}" "case SMSG_AURA_UPDATE:" "aura suppression whitelist")

string(FIND "${opcode_header}${aura_source}${player_source}" "SMSG_AURA_UPDATE_ALL" legacy_opcode)
if(NOT legacy_opcode EQUAL -1)
    message(FATAL_ERROR "legacy SMSG_AURA_UPDATE_ALL remains live")
endif()

string(FIND "${aura_source}${player_source}" "BuildUpdatePacket(" legacy_builder)
if(NOT legacy_builder EQUAL -1)
    message(FATAL_ERROR "legacy aura packet builder remains live")
endif()
