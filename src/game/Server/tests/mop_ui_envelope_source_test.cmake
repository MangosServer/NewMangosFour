file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerActionButton.cpp" action_buttons)
file(READ "${SOURCE_ROOT}/src/game/Object/ReputationMgr.cpp" reputation)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/WorldSessionMgr.cpp" world_session_mgr)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" character_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/AchievementMgr.cpp" achievement)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Weather.cpp" weather)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellEffectTail.cpp" spell_effect)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/opcode_register.inc" login_registry)

if(MUTATION STREQUAL "pre_map_call")
    string(REPLACE
        "    m_achievementMgr.SendAllAchievementData();"
        "    // m_achievementMgr.SendAllAchievementData();"
        player "${player}")
elseif(MUTATION STREQUAL "after_map_call")
    string(REPLACE
        "    pCurrChar->SendInitialPacketsAfterAddToMap();"
        "    // pCurrChar->SendInitialPacketsAfterAddToMap();"
        character_handler "${character_handler}")
elseif(MUTATION STREQUAL "opcode_metadata")
    string(REPLACE
        "    DefS(SMSG_ALL_ACHIEVEMENT_DATA, \"SMSG_ALL_ACHIEVEMENT_DATA\");"
        "    // DefS(SMSG_ALL_ACHIEVEMENT_DATA, \"SMSG_ALL_ACHIEVEMENT_DATA\");"
        opcode_registry "${opcode_registry}")
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
strip_cpp_comments(reputation "${reputation}")
strip_cpp_comments(world_session "${world_session}")
strip_cpp_comments(world_session_mgr "${world_session_mgr}")
strip_cpp_comments(character_handler "${character_handler}")
strip_cpp_comments(achievement "${achievement}")
strip_cpp_comments(weather "${weather}")
strip_cpp_comments(spell_effect "${spell_effect}")
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
extract_body(all_achievements "${achievement}" "void AchievementMgr::SendAllAchievementData()" "void AchievementMgr::SendRespondInspectAchievements")
extract_body(single_weather "${weather}" "void Weather::SendWeatherUpdateToPlayer(" "bool Weather::SendWeatherForPlayersInZone")
extract_body(zone_weather "${weather}" "bool Weather::SendWeatherForPlayersInZone(" "void Weather::SetWeather")
extract_body(bind_spell "${spell_effect}" "void Spell::EffectBind(" "void Spell::EffectRestoreItemCharges")

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
string(FIND "${all_achievements}" "data.WriteBits(m_criteriaProgress.size(), 21);" legacy_progress_count)
string(FIND "${all_achievements}" "data.WriteBits(m_completedAchievements.size(), 23);" legacy_completed_count)
if(NOT legacy_progress_count EQUAL -1 OR NOT legacy_completed_count EQUAL -1)
    message(FATAL_ERROR "legacy 21/23-bit ALL_ACHIEVEMENT_DATA counts remain")
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
if(NOT proficiency MATCHES "MopInitialPackets::BuildSetProficiency")
    message(FATAL_ERROR "BuildSetProficiency is not used by its production sender")
endif()
if(NOT single_weather MATCHES "MopInitialPackets::BuildWeather" OR
   NOT zone_weather MATCHES "MopInitialPackets::BuildWeather")
    message(FATAL_ERROR "BuildWeather is not used by both production senders")
endif()
if(NOT all_achievements MATCHES "MopAchievementPackets::BuildAllAchievementData")
    message(FATAL_ERROR "BuildAllAchievementData is not used by its production sender")
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
require_ordered("${all_achievements}" "ALL_ACHIEVEMENT_DATA writer"
    "WorldPacket data(SMSG_ALL_ACHIEVEMENT_DATA"
    "MopAchievementPackets::BuildAllAchievementData(data"
    "SendPacket(&data)")

foreach(call IN ITEMS
        "SendInitialSpells()"
        "MopInitialPackets::BuildSendUnlearnSpells"
        "SendInitialActionButtons()"
        "m_reputationMgr.SendInitialReputations()"
        "m_achievementMgr.SendAllAchievementData()"
        "MopInitialPackets::BuildBindPointUpdate")
    if(NOT initial_before_map MATCHES "${call}")
        message(FATAL_ERROR "${call} left SendInitialPacketsBeforeAddToMap")
    endif()
endforeach()
require_ordered("${initial_before_map}" "pre-map UI envelope"
    "WorldPacket data(SMSG_BINDPOINTUPDATE"
    "MopInitialPackets::BuildBindPointUpdate(data"
    "GetSession()->SendPacket(&data)"
    "SendInitialSpells()"
    "data.Initialize(SMSG_SEND_UNLEARN_SPELLS"
    "MopInitialPackets::BuildSendUnlearnSpells(data"
    "GetSession()->SendPacket(&data)"
    "SendInitialActionButtons()"
    "m_reputationMgr.SendInitialReputations()"
    "m_achievementMgr.SendAllAchievementData()")
foreach(call IN ITEMS
        "SendAccountDataTimes(PER_CHARACTER_CACHE_MASK)"
        "MopInitialPackets::BuildFeatureSystemStatus"
        "SendInitialPacketsBeforeAddToMap()"
        "SendInitialPacketsAfterAddToMap()")
    string(FIND "${player_login}" "${call}" login_call)
    if(login_call EQUAL -1)
        message(FATAL_ERROR "${call} left the character login envelope")
    endif()
endforeach()
require_ordered("${player_login}" "character login envelope"
    "SendAccountDataTimes(PER_CHARACTER_CACHE_MASK)"
    "data.Initialize(SMSG_FEATURE_SYSTEM_STATUS"
    "MopInitialPackets::BuildFeatureSystemStatus(data"
    "SendPacket(&data)"
    "pCurrChar->SendInitialPacketsBeforeAddToMap()"
    "pCurrChar->GetMap()->Add(pCurrChar)"
    "sObjectAccessor.AddObject(pCurrChar)"
    "pCurrChar->SendInitialPacketsAfterAddToMap()")
if(NOT world_session_mgr MATCHES "s->SendTutorialsData\(\)" OR
   NOT world_session_mgr MATCHES "pop_sess->SendTutorialsData\(\)")
    message(FATAL_ERROR "tutorial flags left the authenticated session phase")
endif()

if(NOT player_header MATCHES "#[ \t]*define[ \t]+MAX_ACTION_BUTTONS[ \t]+144")
    message(FATAL_ERROR "MAX_ACTION_BUTTONS changed")
endif()
if(NOT EXISTS "${SOURCE_ROOT}/src/game/Object/Player.h")
    message(FATAL_ERROR "Player.h is missing")
endif()
if(NOT EXISTS "${SOURCE_ROOT}/src/game/WorldHandlers/AchievementMgr.h")
    message(FATAL_ERROR "AchievementMgr.h is missing")
endif()
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" initial_header)
if(NOT initial_header MATCHES "ACTION_BUTTON_COUNT[ \t]*=[ \t]*132")
    message(FATAL_ERROR "wire ACTION_BUTTON_COUNT is not 132")
endif()

foreach(server_name IN ITEMS
        SMSG_INITIAL_SPELLS
        SMSG_SEND_UNLEARN_SPELLS
        SMSG_ACTION_BUTTONS
        SMSG_INITIALIZE_FACTIONS
        SMSG_ALL_ACHIEVEMENT_DATA
        SMSG_BINDPOINTUPDATE
        SMSG_SET_PROFICIENCY
        SMSG_WEATHER)
    string(REGEX MATCHALL "DefS\\(${server_name},[ \t]*\"${server_name}\"\\)" registry_matches "${opcode_registry}")
    list(LENGTH registry_matches registry_count)
    if(NOT registry_count EQUAL 1)
        message(FATAL_ERROR "${server_name} must have exactly one outbound metadata row in Opcodes.cpp")
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
