file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Spell.h" spell_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Spell.cpp" spell_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellPackets.cpp" spell_packets)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellHandler.cpp" spell_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/ChatCommands/DebugCommands.cpp" debug_commands)

if(MUTATION STREQUAL "reader")
    string(REPLACE
        "MopSpellPackets::ReadCastSpellRequest(recvPacket, request)"
        "false /* removed 18414 cast reader */"
        spell_handler "${spell_handler}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE
        "DefC(CMSG_CAST_SPELL, \"CMSG_CAST_SPELL\""
        "DefC(0xFFFF, \"removed CMSG_CAST_SPELL\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "target_initializer")
    string(REPLACE
        "InitializeForCastRequest("
        "InitializeForRemovedCastRequest("
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "target_mapping")
    string(REPLACE
        "TARGET_FLAG_UNIT | TARGET_FLAG_UNK2"
        "TARGET_FLAG_UNIT | TARGET_FLAG_UNK2 | TARGET_FLAG_UNIT_UNK"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "movement_count_width")
    string(REPLACE
        "movement.forceCount = in.ReadBits(22);"
        "movement.forceCount = in.ReadBits(16); /* damaged movement count */"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "movement_count_bound")
    string(REPLACE
        "movement.forceCount > (in.size() - in.rpos()) / sizeof(uint32)"
        "false /* removed movement-count bound */"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "movement_flag_order")
    string(REPLACE
        "movement.hasMovementFlags = !in.ReadBit();\n        movement.hasTimestamp = !in.ReadBit();\n        movement.hasUnknownUInt32 = !in.ReadBit();\n        if (movement.hasMovementFlags)\n            in.ReadBits(30);"
        "movement.hasMovementFlags = !in.ReadBit();\n        if (movement.hasMovementFlags)\n            in.ReadBits(30); /* moved before presence bits */\n        movement.hasTimestamp = !in.ReadBit();\n        movement.hasUnknownUInt32 = !in.ReadBit();"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "missile_guard")
    string(REPLACE
        "if (hasMissileSpeed)\n            in >> missileSpeed;"
        "if (hasElevation)\n            in >> missileSpeed; /* swapped missile guard */"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "elevation_guard")
    string(REPLACE
        "if (hasElevation)\n            in >> elevation;"
        "if (hasMissileSpeed)\n            in >> elevation; /* swapped elevation guard */"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "full_consumption")
    string(REPLACE
        "if (in.rpos() != in.size())"
        "if (false /* removed full-consumption gate */)"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "string_bound")
    string(REPLACE
        "targetStringLength > in.size() - in.rpos()"
        "false /* removed target-string bound */"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "legacy_prefix")
    string(REPLACE
        "if (!MopSpellPackets::ReadCastSpellRequest(recvPacket, request))"
        "uint8 cast_count; uint32 spellId, glyphIndex; uint8 cast_flags;\n    recvPacket >> cast_count;\n    recvPacket >> spellId >> glyphIndex;\n    recvPacket >> cast_flags;\n    if (!MopSpellPackets::ReadCastSpellRequest(recvPacket, request))"
        spell_handler "${spell_handler}")
elseif(MUTATION STREQUAL "cast_failed_sender")
    string(REPLACE
        "MopSpellPackets::BuildCastFailed(data, spellInfo->ID, reportedResult,"
        "/* removed 18414 cast-failure builder */"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "cast_failed_registration")
    string(REPLACE
        "DefS(SMSG_CAST_FAILED, \"SMSG_CAST_FAILED\");"
        "/* removed SMSG_CAST_FAILED registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "pet_cast_failed_registration")
    string(REPLACE
        "DefS(SMSG_PET_CAST_FAILED, \"SMSG_PET_CAST_FAILED\");"
        "/* removed SMSG_PET_CAST_FAILED registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "cast_failed_gate")
    string(REPLACE
        "case SMSG_CAST_FAILED:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "pet_cast_failed_gate")
    string(REPLACE
        "case SMSG_PET_CAST_FAILED:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "cast_result_translation")
    string(REPLACE
        "if (result <= 118)"
        "if (result <= 117)"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "pet_presence_order")
    string(REPLACE
        "out.WriteBit(!arguments.hasArg18);\n        out.WriteBit(!arguments.hasArg10);"
        "out.WriteBit(!arguments.hasArg10);\n        out.WriteBit(!arguments.hasArg18);"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "debug_cast_failed_sender")
    string(REPLACE
        "MopSpellPackets::BuildCastFailed(data, 133, SpellCastResult(failnum), 0, false, arguments);"
        "WorldPacket data(SMSG_CAST_FAILED, 5);"
        debug_commands "${debug_commands}")
elseif(MUTATION STREQUAL "spell_start_sender")
    string(REPLACE
        "MopSpellPackets::BuildSpellStart(data, spell)"
        "false /* removed 18414 spell-start builder */"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_start_registration")
    string(REPLACE
        "DefS(SMSG_SPELL_START, \"SMSG_SPELL_START\");"
        "/* removed SMSG_SPELL_START registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "spell_start_gate")
    string(REPLACE
        "case SMSG_SPELL_START:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "spell_start_target_mask_width")
    string(REPLACE
        "out.WriteBits(spell.targetMask, 20);"
        "out.WriteBits(spell.targetMask, 21);"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_start_caster_mask")
    string(REPLACE
        "out.WriteGuidMask<2, 6>(spell.casterUnitGuid);"
        "out.WriteGuidMask<6, 2>(spell.casterUnitGuid);"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_start_string_bound")
    string(REPLACE
        "spell.targetString.size() > 0x7F"
        "spell.targetString.size() > 0xFF"
        spell_packets "${spell_packets}")
endif()

string(FIND "${spell_handler}" "void WorldSession::HandleCastSpellOpcode" cast_start)
string(FIND "${spell_handler}" "void WorldSession::HandleCancelCastOpcode" cast_end)
if(cast_start EQUAL -1 OR cast_end EQUAL -1 OR cast_end LESS_EQUAL cast_start)
    message(FATAL_ERROR "could not isolate HandleCastSpellOpcode")
endif()
math(EXPR cast_length "${cast_end} - ${cast_start}")
string(SUBSTRING "${spell_handler}" ${cast_start} ${cast_length} cast_handler)

string(FIND "${spell_packets}" "void Spell::SendSpellStart()" spell_start_begin)
string(FIND "${spell_packets}" "void Spell::SendSpellGo()" spell_start_end)
if(spell_start_begin EQUAL -1 OR spell_start_end EQUAL -1 OR spell_start_end LESS_EQUAL spell_start_begin)
    message(FATAL_ERROR "could not isolate SendSpellStart")
endif()
math(EXPR spell_start_length "${spell_start_end} - ${spell_start_begin}")
string(SUBSTRING "${spell_packets}" ${spell_start_begin} ${spell_start_length} spell_start_sender_body)

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

function(forbid source token context)
    string(FIND "${source}" "${token}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden token remains: ${token}")
    endif()
endfunction()

require_once("${opcode_header}"
    "CMSG_CAST_SPELL                              = 0x0206"
    "CMSG_CAST_SPELL opcode value")
require_once("${opcode_registry}"
    "DefC(CMSG_CAST_SPELL, \"CMSG_CAST_SPELL\""
    "CMSG_CAST_SPELL registration")
require_once("${cast_handler}"
    "MopSpellPackets::ReadCastSpellRequest(recvPacket, request)"
    "18414 cast reader wiring")
require_once("${spell_source}"
    "movement.forceCount = in.ReadBits(22);"
    "22-bit embedded movement count")
require_once("${spell_source}"
    "movement.forceCount > (in.size() - in.rpos()) / sizeof(uint32)"
    "embedded movement allocation bound")
require_once("${spell_source}"
    "if (hasMissileSpeed)\n            in >> missileSpeed;"
    "binary-proven missile-speed guard")
require_once("${spell_source}"
    "if (hasElevation)\n            in >> elevation;"
    "binary-proven elevation guard")
require_once("${spell_source}"
    "if (in.rpos() != in.size())"
    "full request consumption gate")
require_once("${spell_source}"
    "InitializeForCastRequest("
    "owning target initializer")
require_once("${spell_source}"
    "request.targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_UNK2)"
    "unit target mapping")
require_once("${spell_source}"
    "request.targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_GAMEOBJECT_ITEM)"
    "game-object target mapping")
require_once("${spell_source}"
    "request.targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE)"
    "corpse target mapping")
require_once("${spell_source}"
    "request.targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM)"
    "item target mapping")
require_once("${spell_source}"
    "targetStringLength > in.size() - in.rpos()"
    "target-string allocation bound")
require_once("${opcode_header}"
    "SMSG_CAST_FAILED                             = 0x143A"
    "SMSG_CAST_FAILED opcode value")
require_once("${opcode_header}"
    "SMSG_PET_CAST_FAILED                         = 0x149B"
    "SMSG_PET_CAST_FAILED opcode value")
require_once("${opcode_registry}"
    "DefS(SMSG_CAST_FAILED, \"SMSG_CAST_FAILED\");"
    "SMSG_CAST_FAILED registration")
require_once("${opcode_registry}"
    "DefS(SMSG_PET_CAST_FAILED, \"SMSG_PET_CAST_FAILED\");"
    "SMSG_PET_CAST_FAILED registration")
require_once("${world_session}"
    "case SMSG_CAST_FAILED:"
    "SMSG_CAST_FAILED suppression gate")
require_once("${world_session}"
    "case SMSG_PET_CAST_FAILED:"
    "SMSG_PET_CAST_FAILED suppression gate")
require_once("${spell_packets}"
    "MopSpellPackets::BuildCastFailed(data, spellInfo->ID, reportedResult,"
    "18414 cast-failure sender wiring")
require_once("${spell_packets}"
    "if (result <= 118)"
    "18414 cast-result enum translation")
require_once("${spell_packets}"
    "out.WriteBit(!arguments.hasArg18);\n        out.WriteBit(!arguments.hasArg10);"
    "pet cast-failure presence-bit order")
require_once("${debug_commands}"
    "MopSpellPackets::BuildCastFailed(data, 133, SpellCastResult(failnum), 0, false, arguments);"
    "debug cast-failure sender wiring")
forbid("${debug_commands}"
    "WorldPacket data(SMSG_CAST_FAILED"
    "legacy debug cast-failure body")
require_once("${opcode_registry}"
    "DefS(SMSG_SPELL_START, \"SMSG_SPELL_START\");"
    "SMSG_SPELL_START registration")
require_once("${world_session}"
    "case SMSG_SPELL_START:"
    "SMSG_SPELL_START suppression gate")
require_once("${spell_packets}"
    "MopSpellPackets::BuildSpellStart(data, spell)"
    "18414 spell-start sender wiring")
require_once("${spell_packets}"
    "out.WriteBits(spell.targetMask, 20);"
    "spell-start 20-bit target mask")
require_once("${spell_packets}"
    "out.WriteGuidMask<2, 6>(spell.casterUnitGuid);"
    "spell-start caster-unit mask order")
require_once("${spell_packets}"
    "spell.targetString.size() > 0x7F"
    "spell-start target-string bound")
forbid("${spell_start_sender_body}"
    "data << m_targets;"
    "legacy spell-start target serializer")

string(FIND "${cast_handler}" "MopSpellPackets::ReadCastSpellRequest(recvPacket, request)" reader_position)
string(FIND "${cast_handler}" "sSpellStore.LookupEntry(spellId)" lookup_position)
string(FIND "${cast_handler}" "targets.InitializeForCastRequest(mover, request)" initializer_position)
string(FIND "${cast_handler}" "sSpellMgr.SelectAuraRankForLevel" rank_position)
if(reader_position EQUAL -1 OR lookup_position EQUAL -1 OR initializer_position EQUAL -1 OR rank_position EQUAL -1
        OR NOT reader_position LESS lookup_position
        OR NOT lookup_position LESS initializer_position
        OR NOT initializer_position LESS rank_position)
    message(FATAL_ERROR "cast parser/validation/target-resolution order drifted")
endif()

string(FIND "${spell_source}" "movement.hasMovementFlags = !in.ReadBit();" movement_flags_presence)
string(FIND "${spell_source}" "movement.hasTimestamp = !in.ReadBit();" movement_timestamp_presence)
string(FIND "${spell_source}" "movement.hasUnknownUInt32 = !in.ReadBit();" movement_uint32_presence)
string(FIND "${spell_source}" "in.ReadBits(30);" movement_flags_payload)
if(movement_flags_presence EQUAL -1 OR movement_timestamp_presence EQUAL -1
        OR movement_uint32_presence EQUAL -1 OR movement_flags_payload EQUAL -1
        OR NOT movement_flags_presence LESS movement_timestamp_presence
        OR NOT movement_timestamp_presence LESS movement_uint32_presence
        OR NOT movement_uint32_presence LESS movement_flags_payload)
    message(FATAL_ERROR "embedded movement flags/presence-bit order drifted")
endif()

forbid("${cast_handler}"
    "recvPacket >> cast_count;"
    "legacy byte-aligned cast-count prefix")
forbid("${cast_handler}"
    "recvPacket >> spellId >> glyphIndex;"
    "legacy byte-aligned spell/glyph prefix")
forbid("${cast_handler}"
    "targets.ReadAdditionalData(recvPacket, cast_flags);"
    "legacy additional-data parser")
