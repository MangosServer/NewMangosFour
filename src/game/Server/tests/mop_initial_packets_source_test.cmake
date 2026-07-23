file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerActionButton.cpp" action_buttons)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" character_handler)
file(READ "${SOURCE_ROOT}/src/game/Object/ReputationMgr.cpp" reputation)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerDeath.cpp" player_death)
file(READ "${SOURCE_ROOT}/src/game/Object/Item.h" item_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Item.cpp" item)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ItemHandlerEnchant.cpp" item_enchant)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Weather.cpp" weather)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellEffectTail.cpp" spell_effect)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/NPCHandler.cpp" npc_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/opcode_register.inc" login_registry)

if(MUTATION STREQUAL "builder_call")
    string(REPLACE
        "    MopInitialPackets::BuildInitialSpells(data, spells);"
        "    // MopInitialPackets::BuildInitialSpells(data, spells);"
        player "${player}")
elseif(MUTATION STREQUAL "opcode_metadata")
    string(REPLACE
        "    DefS(SMSG_INITIAL_SPELLS, \"SMSG_INITIAL_SPELLS\");"
        "    // DefS(SMSG_INITIAL_SPELLS, \"SMSG_INITIAL_SPELLS\");"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "binder_reader")
    string(REPLACE
        "MopBindPackets::ReadBinderActivate(recv_data)"
        "uint64(0) /* removed binder request reader */"
        npc_handler "${npc_handler}")
elseif(MUTATION STREQUAL "binder_confirm_builder")
    string(REPLACE
        "MopBindPackets::BuildBinderConfirm(data, guid.GetRawValue())"
        "/* removed binder confirmation builder */"
        player "${player}")
elseif(MUTATION STREQUAL "playerbound_builder")
    string(REPLACE
        "MopBindPackets::BuildPlayerBound(data,"
        "/* removed player-bound builder */ (void)(data), (void)("
        spell_effect "${spell_effect}")
elseif(MUTATION STREQUAL "binder_registration")
    string(REPLACE
        "DefC(CMSG_BINDER_ACTIVATE, \"CMSG_BINDER_ACTIVATE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleBinderActivateOpcode);"
        "/* removed binder request registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "binder_allowlist")
    string(REPLACE
        "case SMSG_BINDER_CONFIRM:"
        "case 0xFFFF: /* removed binder confirmation allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "tutorial_flag_registration")
    string(REPLACE
        "DefC(CMSG_TUTORIAL_FLAG, \"CMSG_TUTORIAL_FLAG\""
        "RemovedC(CMSG_TUTORIAL_FLAG, \"CMSG_TUTORIAL_FLAG\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "tutorial_clear_registration")
    string(REPLACE
        "DefC(CMSG_TUTORIAL_CLEAR, \"CMSG_TUTORIAL_CLEAR\""
        "RemovedC(CMSG_TUTORIAL_CLEAR, \"CMSG_TUTORIAL_CLEAR\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "tutorial_reset_registration")
    string(REPLACE
        "DefC(CMSG_TUTORIAL_RESET, \"CMSG_TUTORIAL_RESET\""
        "RemovedC(CMSG_TUTORIAL_RESET, \"CMSG_TUTORIAL_RESET\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "tutorial_flag_reader")
    string(REPLACE
        "recv_data >> iFlag;"
        "/* removed tutorial flag reader */"
        character_handler "${character_handler}")
elseif(MUTATION STREQUAL "forced_reactions_registration")
    string(REPLACE
        "DefC(CMSG_REQUEST_FORCED_REACTIONS, \"CMSG_REQUEST_FORCED_REACTIONS\""
        "DefC(0xFFFF, \"removed CMSG_REQUEST_FORCED_REACTIONS\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "forced_reactions_handler")
    string(REPLACE
        "GetPlayer()->GetReputationMgr().SendForceReactions();"
        "/* removed forced-reaction response */"
        character_handler "${character_handler}")
endif()

function(strip_cpp_comments output source)
    set(text "${source}")
    while(TRUE)
        string(FIND "${text}" "/*" comment_start)
        if(comment_start EQUAL -1)
            break()
        endif()
        math(EXPR tail_start "${comment_start} + 2")
        string(SUBSTRING "${text}" ${tail_start} -1 tail)
        string(FIND "${tail}" "*/" comment_end)
        if(comment_end EQUAL -1)
            message(FATAL_ERROR "Unterminated block comment while scanning source")
        endif()
        string(SUBSTRING "${text}" 0 ${comment_start} before)
        math(EXPR after_start "${comment_end} + 2")
        string(SUBSTRING "${tail}" ${after_start} -1 after)
        set(text "${before}${after}")
    endwhile()
    string(REGEX REPLACE "//[^\r\n]*" "" text "${text}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

function(require_ordered source context)
    set(remaining "${source}")
    foreach(token IN LISTS ARGN)
        string(FIND "${remaining}" "${token}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR "${context}: missing active ordered token: ${token}")
        endif()
        string(LENGTH "${token}" token_length)
        math(EXPR next_position "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endforeach()
endfunction()

strip_cpp_comments(player "${player}")
strip_cpp_comments(player_header "${player_header}")
strip_cpp_comments(action_buttons "${action_buttons}")
strip_cpp_comments(world_session "${world_session}")
strip_cpp_comments(character_handler "${character_handler}")
strip_cpp_comments(reputation "${reputation}")
strip_cpp_comments(player_death "${player_death}")
strip_cpp_comments(item_header "${item_header}")
strip_cpp_comments(item "${item}")
strip_cpp_comments(item_enchant "${item_enchant}")
strip_cpp_comments(weather "${weather}")
strip_cpp_comments(spell_effect "${spell_effect}")
strip_cpp_comments(npc_handler "${npc_handler}")
strip_cpp_comments(opcode_registry "${opcode_registry}")
strip_cpp_comments(login_registry "${login_registry}")

function(extract_body output source start_marker end_marker)
    string(FIND "${source}" "${start_marker}" start)
    string(FIND "${source}" "${end_marker}" end)
    if(start EQUAL -1 OR end EQUAL -1 OR NOT start LESS end)
        message(FATAL_ERROR "Could not isolate ${start_marker}")
    endif()
    math(EXPR length "${end} - ${start}")
    string(SUBSTRING "${source}" ${start} ${length} body)
    set(${output} "${body}" PARENT_SCOPE)
endfunction()

extract_body(initial_spells "${player}" "void Player::SendInitialSpells()" "void Player::_SetCreateBits")
extract_body(proficiency "${player}" "void Player::SendProficiency(" "void Player::RemovePetitionsAndSigns")
extract_body(initial_before_map "${player}" "void Player::SendInitialPacketsBeforeAddToMap()" "void Player::SendInitialPacketsAfterAddToMap()")
extract_body(initial_actions "${action_buttons}" "void Player::SendInitialActionButtons()" "void Player::SendLockActionButtons()")
extract_body(lock_actions "${action_buttons}" "void Player::SendLockActionButtons()" "bool Player::IsActionButtonDataValid")
extract_body(account_times "${world_session}" "void WorldSession::SendAccountDataTimes(" "void WorldSession::LoadTutorialsData")
extract_body(tutorials "${world_session}" "void WorldSession::SendTutorialsData()" "void WorldSession::SaveTutorialsData")
extract_body(player_login "${character_handler}" "void WorldSession::HandlePlayerLogin(LoginQueryHolder* holder)" "void WorldSession::HandleSetFactionAtWarOpcode")
extract_body(initial_reputation "${reputation}" "void ReputationMgr::SendInitialReputations()" "void ReputationMgr::SendVisible")
extract_body(forced_reactions "${reputation}" "void ReputationMgr::SendForceReactions()" "void ReputationMgr::SendState")
extract_body(single_weather "${weather}" "void Weather::SendWeatherUpdateToPlayer(" "bool Weather::SendWeatherForPlayersInZone")
extract_body(zone_weather "${weather}" "bool Weather::SendWeatherForPlayersInZone(" "void Weather::SetWeather")
extract_body(bind_spell "${spell_effect}" "void Spell::EffectBind(" "void Spell::EffectRestoreItemCharges")
extract_body(set_bind "${player}" "void Player::SetBindPoint(" "void Player::SendTalentWipeConfirm")
extract_body(binder_activate "${npc_handler}" "void WorldSession::HandleBinderActivateOpcode" "void WorldSession::SendBindPoint")
extract_body(send_bind "${npc_handler}" "void WorldSession::SendBindPoint" "void WorldSession::HandleListStabledPetsOpcode")

if(initial_spells MATCHES "GetSpellCooldownMap|put<uint16>|<<[ \t]*uint16\(0\)")
    message(FATAL_ERROR "legacy INITIAL_SPELLS slot/cooldown body remains")
endif()
string(FIND "${initial_before_map}" "    data.Initialize(SMSG_SEND_UNLEARN_SPELLS, 4);" legacy_unlearn)
if(NOT legacy_unlearn EQUAL -1)
    message(FATAL_ERROR "legacy uint32 SEND_UNLEARN_SPELLS count remains")
endif()
if(initial_actions MATCHES "MAX_ACTION_BUTTONS[ \t]*[*][ \t]*4|packedData")
    message(FATAL_ERROR "legacy 144-uint32 ACTION_BUTTONS body remains")
endif()
if(lock_actions MATCHES "WorldPacket[ \t]+data\(SMSG_ACTION_BUTTONS,[ \t]*1\)|data[ \t]*<<[ \t]*uint8\(2\)")
    message(FATAL_ERROR "legacy one-byte lock body remains")
endif()
if(initial_reputation MATCHES "0x00000100")
    message(FATAL_ERROR "legacy INITIALIZE_FACTIONS leading 0x100 remains")
endif()
if(player_login MATCHES "data[ \t]*<<[ \t]*uint8\(2\)[^\n]*status")
    message(FATAL_ERROR "legacy FEATURE_SYSTEM_STATUS field sequence remains")
endif()
string(FIND "${initial_before_map}" "    data << m_homebindZ;" legacy_login_bind)
string(FIND "${bind_spell}" "    data << float(loc.coord_x);" legacy_spell_bind)
if(NOT legacy_login_bind EQUAL -1 OR NOT legacy_spell_bind EQUAL -1)
    message(FATAL_ERROR "inline or incorrectly ordered BINDPOINTUPDATE body remains")
endif()
if(single_weather MATCHES "uint8\(0\)" OR zone_weather MATCHES "uint8\(0\)")
    message(FATAL_ERROR "legacy byte WEATHER flag remains")
endif()

if(NOT initial_spells MATCHES "MopInitialPackets::BuildInitialSpells")
    message(FATAL_ERROR "BuildInitialSpells is not used by its production sender")
endif()
if(NOT initial_before_map MATCHES "MopInitialPackets::BuildSendUnlearnSpells")
    message(FATAL_ERROR "BuildSendUnlearnSpells is not used by its production sender")
endif()
if(NOT initial_actions MATCHES "MopInitialPackets::BuildActionButtons" OR
   NOT lock_actions MATCHES "MopInitialPackets::BuildActionButtons")
    message(FATAL_ERROR "BuildActionButtons is not used by both production senders")
endif()
if(NOT account_times MATCHES "MopInitialPackets::BuildAccountDataTimes")
    message(FATAL_ERROR "BuildAccountDataTimes is not used by its production sender")
endif()
if(NOT player_login MATCHES "MopInitialPackets::BuildFeatureSystemStatus")
    message(FATAL_ERROR "BuildFeatureSystemStatus is not used by its production sender")
endif()
if(NOT initial_reputation MATCHES "MopInitialPackets::BuildInitializeFactions")
    message(FATAL_ERROR "BuildInitializeFactions is not used by its production sender")
endif()
if(NOT tutorials MATCHES "MopInitialPackets::BuildTutorialFlags")
    message(FATAL_ERROR "BuildTutorialFlags is not used by its production sender")
endif()
if(NOT initial_before_map MATCHES "MopInitialPackets::BuildBindPointUpdate" OR
   NOT bind_spell MATCHES "MopInitialPackets::BuildBindPointUpdate")
    message(FATAL_ERROR "BuildBindPointUpdate is not used by both production senders")
endif()
if(NOT binder_activate MATCHES "MopBindPackets::ReadBinderActivate")
    message(FATAL_ERROR "ReadBinderActivate is not used by the binder handler")
endif()
if(NOT set_bind MATCHES "MopBindPackets::BuildBinderConfirm")
    message(FATAL_ERROR "BuildBinderConfirm is not used by Player::SetBindPoint")
endif()
if(NOT bind_spell MATCHES "MopBindPackets::BuildPlayerBound")
    message(FATAL_ERROR "BuildPlayerBound is not used by Spell::EffectBind")
endif()
if(send_bind MATCHES "SMSG_TRAINER_SERVICE")
    message(FATAL_ERROR "legacy SMSG_TRAINER_SERVICE bind send remains")
endif()
if(NOT proficiency MATCHES "MopInitialPackets::BuildSetProficiency")
    message(FATAL_ERROR "BuildSetProficiency is not used by its production sender")
endif()
if(NOT single_weather MATCHES "MopInitialPackets::BuildWeather" OR
   NOT zone_weather MATCHES "MopInitialPackets::BuildWeather")
    message(FATAL_ERROR "BuildWeather is not used by both production senders")
endif()
if(NOT player_login MATCHES "MopInitialPackets::BuildMotd")
    message(FATAL_ERROR "BuildMotd is not used by the login sender")
endif()
if(NOT player_death MATCHES "MopDeathPackets::BuildCorpseReclaimDelay")
    message(FATAL_ERROR "BuildCorpseReclaimDelay is not used by the death sender")
endif()
if(NOT forced_reactions MATCHES "MopReputationPackets::BuildForcedReactions")
    message(FATAL_ERROR "BuildForcedReactions is not used by the reputation sender")
endif()
string(FIND "${character_handler}"
    "GetPlayer()->GetReputationMgr().SendForceReactions();"
    forced_reactions_handler_position)
if(forced_reactions_handler_position EQUAL -1)
    message(FATAL_ERROR "forced-reaction request handler does not send the current state")
endif()
string(FIND "${opcode_registry}"
    "DefC(CMSG_REQUEST_FORCED_REACTIONS, \"CMSG_REQUEST_FORCED_REACTIONS\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestForcedReactionsOpcode);"
    forced_reactions_registration_position)
if(forced_reactions_registration_position EQUAL -1)
    message(FATAL_ERROR "CMSG_REQUEST_FORCED_REACTIONS is missing its logged-in handler registration")
endif()
if(NOT item MATCHES "MopItemPackets::BuildItemTimeUpdate")
    message(FATAL_ERROR "BuildItemTimeUpdate is not used by the item sender")
endif()
if(NOT item_enchant MATCHES "MopItemPackets::BuildItemEnchantTimeUpdate")
    message(FATAL_ERROR "BuildItemEnchantTimeUpdate is not used by the enchant sender")
endif()

require_ordered("${initial_spells}" "INITIAL_SPELLS writer"
    "WorldPacket data(SMSG_INITIAL_SPELLS"
    "MopInitialPackets::BuildInitialSpells(data"
    "GetSession()->SendPacket(&data)")
require_ordered("${initial_actions}" "initial ACTION_BUTTONS writer"
    "WorldPacket data(SMSG_ACTION_BUTTONS"
    "MopInitialPackets::BuildActionButtons(data"
    "GetSession()->SendPacket(&data)")
require_ordered("${lock_actions}" "lock ACTION_BUTTONS writer"
    "WorldPacket data(SMSG_ACTION_BUTTONS"
    "MopInitialPackets::BuildActionButtons(data"
    "GetSession()->SendPacket(&data)")
require_ordered("${account_times}" "ACCOUNT_DATA_TIMES writer"
    "WorldPacket data(SMSG_ACCOUNT_DATA_TIMES"
    "MopInitialPackets::BuildAccountDataTimes(data"
    "SendPacket(&data)")
require_ordered("${tutorials}" "TUTORIAL_FLAGS writer"
    "WorldPacket data(SMSG_TUTORIAL_FLAGS"
    "MopInitialPackets::BuildTutorialFlags(data"
    "SendPacket(&data)")
require_ordered("${initial_reputation}" "INITIALIZE_FACTIONS writer"
    "WorldPacket data(SMSG_INITIALIZE_FACTIONS"
    "MopInitialPackets::BuildInitializeFactions(data"
    "m_player->SendDirectMessage(&data)")
require_ordered("${proficiency}" "SET_PROFICIENCY writer"
    "WorldPacket data(SMSG_SET_PROFICIENCY"
    "MopInitialPackets::BuildSetProficiency(data"
    "GetSession()->SendPacket(&data)")
require_ordered("${single_weather}" "single-player WEATHER writer"
    "WorldPacket data(SMSG_WEATHER"
    "MopInitialPackets::BuildWeather(data"
    "player->GetSession()->SendPacket(&data)")
require_ordered("${zone_weather}" "zone WEATHER writer"
    "WorldPacket data(SMSG_WEATHER"
    "MopInitialPackets::BuildWeather(data"
    "_map->SendToPlayersInZone(&data")
require_ordered("${bind_spell}" "spell BINDPOINTUPDATE writer"
    "WorldPacket data(SMSG_BINDPOINTUPDATE"
    "MopInitialPackets::BuildBindPointUpdate(data"
    "player->SendDirectMessage(&data)")
require_ordered("${binder_activate}" "binder activation reader"
    "MopBindPackets::ReadBinderActivate(recv_data)"
    "GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_INNKEEPER)"
    "SendBindPoint(unit)")
require_ordered("${set_bind}" "binder confirmation writer"
    "MopBindPackets::BuildBinderConfirm(data, guid.GetRawValue())"
    "GetSession()->SendPacket(&data)")
require_ordered("${send_bind}" "bind spell flow"
    "npc->CastSpell(_player, 3286, true)"
    "_player->PlayerTalkClass->CloseGossip()")
require_ordered("${bind_spell}" "player-bound writer"
    "MopBindPackets::BuildPlayerBound(data,"
    "player->SendDirectMessage(&data)")
require_ordered("${initial_before_map}" "pre-map regular UI envelope"
    "WorldPacket data(SMSG_BINDPOINTUPDATE"
    "MopInitialPackets::BuildBindPointUpdate(data"
    "GetSession()->SendPacket(&data)"
    "SendInitialSpells()"
    "data.Initialize(SMSG_SEND_UNLEARN_SPELLS"
    "MopInitialPackets::BuildSendUnlearnSpells(data"
    "GetSession()->SendPacket(&data)"
    "SendInitialActionButtons()"
    "m_reputationMgr.SendInitialReputations()")
require_ordered("${player_login}" "regular character login envelope"
    "SendAccountDataTimes(PER_CHARACTER_CACHE_MASK)"
    "data.Initialize(SMSG_FEATURE_SYSTEM_STATUS"
    "MopInitialPackets::BuildFeatureSystemStatus(data"
    "SendPacket(&data)"
    "pCurrChar->SendInitialPacketsBeforeAddToMap()"
    "pCurrChar->GetMap()->Add(pCurrChar)"
    "sObjectAccessor.AddObject(pCurrChar)"
    "pCurrChar->SendInitialPacketsAfterAddToMap()")

if(NOT player_header MATCHES "#[ \t]*define[ \t]+MAX_ACTION_BUTTONS[ \t]+144")
    message(FATAL_ERROR "MAX_ACTION_BUTTONS changed")
endif()
if(NOT EXISTS "${SOURCE_ROOT}/src/game/Object/Player.h")
    message(FATAL_ERROR "Player.h is missing")
endif()
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" packet_header)
if(NOT packet_header MATCHES "ACTION_BUTTON_COUNT[ \t]*=[ \t]*132")
    message(FATAL_ERROR "wire ACTION_BUTTON_COUNT is not 132")
endif()

foreach(server_name IN ITEMS
        SMSG_INITIAL_SPELLS
        SMSG_SEND_UNLEARN_SPELLS
        SMSG_ACTION_BUTTONS
        SMSG_INITIALIZE_FACTIONS
        SMSG_BINDPOINTUPDATE
        SMSG_SET_PROFICIENCY
        SMSG_WEATHER
        SMSG_MOTD
        SMSG_CORPSE_RECLAIM_DELAY
        SMSG_SET_FORCED_REACTIONS
        SMSG_ITEM_TIME_UPDATE
        SMSG_ITEM_ENCHANT_TIME_UPDATE)
    if(NOT opcode_registry MATCHES "DefS\\(${server_name},[ \t]*\"${server_name}\"\\)")
        message(FATAL_ERROR "${server_name} is missing outbound opcode metadata")
    endif()
endforeach()

foreach(server_name IN ITEMS
        SMSG_MOTD
        SMSG_CORPSE_RECLAIM_DELAY
        SMSG_SET_FORCED_REACTIONS
        SMSG_ITEM_TIME_UPDATE
        SMSG_ITEM_ENCHANT_TIME_UPDATE)
    if(NOT world_session MATCHES "case[ \t]+${server_name}:")
        message(FATAL_ERROR "${server_name} is missing from central enter-world admission")
    endif()
endforeach()

foreach(registration IN ITEMS
        "DefC(CMSG_BINDER_ACTIVATE, \"CMSG_BINDER_ACTIVATE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleBinderActivateOpcode);"
        "DefS(SMSG_BINDER_CONFIRM, \"SMSG_BINDER_CONFIRM\");"
        "DefS(SMSG_PLAYERBOUND, \"SMSG_PLAYERBOUND\");")
    string(FIND "${opcode_registry}" "${registration}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "bind opcode registration missing: ${registration}")
    endif()
endforeach()

foreach(registration IN ITEMS
        "DefC(CMSG_TUTORIAL_FLAG, \"CMSG_TUTORIAL_FLAG\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialFlagOpcode);"
        "DefC(CMSG_TUTORIAL_CLEAR, \"CMSG_TUTORIAL_CLEAR\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialClearOpcode);"
        "DefC(CMSG_TUTORIAL_RESET, \"CMSG_TUTORIAL_RESET\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialResetOpcode);")
    string(FIND "${opcode_registry}" "${registration}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "tutorial opcode registration missing: ${registration}")
    endif()
endforeach()

if(NOT character_handler MATCHES "recv_data[ \t]*>>[ \t]*iFlag;")
    message(FATAL_ERROR "CMSG_TUTORIAL_FLAG must read its uint32 flag index")
endif()

foreach(allowlist IN ITEMS "case SMSG_BINDER_CONFIRM:" "case SMSG_PLAYERBOUND:")
    string(FIND "${world_session}" "${allowlist}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "bind packet suppression allowlist missing: ${allowlist}")
    endif()
endforeach()

foreach(server_name IN ITEMS
        SMSG_ACCOUNT_DATA_TIMES
        SMSG_FEATURE_SYSTEM_STATUS
        SMSG_TUTORIAL_FLAGS)
    string(REGEX MATCHALL "DefS\\(${server_name},[ \t]*\"${server_name}\"\\)" login_matches "${login_registry}")
    list(LENGTH login_matches login_count)
    if(NOT login_count EQUAL 1)
        message(FATAL_ERROR "${server_name} must remain registered exactly once in opcode_register.inc")
    endif()
    if(opcode_registry MATCHES "DefS\\(${server_name},[ \t]*\"${server_name}\"\\)")
        message(FATAL_ERROR "${server_name} is duplicated in Opcodes.cpp")
    endif()
endforeach()
