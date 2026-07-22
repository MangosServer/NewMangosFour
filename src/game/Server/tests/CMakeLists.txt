# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

add_executable(mop_framing_test mop_framing_test.cpp)
target_include_directories(mop_framing_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(mop_framing_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME mop_framing COMMAND mop_framing_test)

add_executable(mop_auth_test
    mop_auth_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MopAuthSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MopAuthProof.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MopAuthResponse.cpp)
target_include_directories(mop_auth_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_auth_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# MopAuthResponse.cpp includes Opcodes.h, which (per its own comment, ":39") includes the FULL
# WorldSession.h so the opcode table's OpcodeHandler struct size matches across translation units.
# That chain pulls in Item.h/AuctionHouseMgr.h/LFGMgr.h/DBCStructure.h/Object.h/Eluna's LuaValue.h --
# i.e. most of the game module's own header surface. Adding those include dirs one at a time here
# (Object, then AuctionHouseBot, then vmap, then Eluna, ...) chases an unbounded list; linking the
# real 'game' target instead inherits its already-correct PUBLIC include set (and, when
# SCRIPT_LIB_ELUNA is on, Eluna's own include dirs) via CMake usage requirements, which is bounded
# and matches how Server/*.cpp already compiles inside the real game library. mop_auth_test itself
# calls nothing from 'game' -- only enum values and struct layouts are used -- so no game .obj
# beyond what the headers require gets pulled into the test binary.
target_link_libraries(mop_auth_test PRIVATE game)
add_test(NAME mop_auth COMMAND mop_auth_test)

add_executable(mop_charenum_test mop_charenum_test.cpp)
target_include_directories(mop_charenum_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_charenum_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# Same rationale as mop_auth_test: WorldPacket/Opcodes pull the game header surface; link 'game'.
target_link_libraries(mop_charenum_test PRIVATE game)
add_test(NAME mop_charenum COMMAND mop_charenum_test)

add_executable(mop_trainerbuyfailed_test mop_trainerbuyfailed_test.cpp)
target_include_directories(mop_trainerbuyfailed_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_trainerbuyfailed_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# Same rationale as mop_auth_test: WorldPacket/Opcodes pull the game header surface; link 'game'.
target_link_libraries(mop_trainerbuyfailed_test PRIVATE game)
add_test(NAME mop_trainerbuyfailed COMMAND mop_trainerbuyfailed_test)

add_executable(mop_creategating_test mop_creategating_test.cpp)
target_include_directories(mop_creategating_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_creategating_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# Same rationale as mop_auth_test: Common.h/SharedDefines.h pull the game header surface; link 'game'.
target_link_libraries(mop_creategating_test PRIVATE game)
add_test(NAME mop_creategating COMMAND mop_creategating_test)

add_executable(mop_updateobject_test
    mop_updateobject_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MopUpdateObject.cpp)
target_include_directories(mop_updateobject_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_updateobject_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# Same rationale as mop_auth_test: WorldPacket/Opcodes pull the game header surface; link 'game'.
target_link_libraries(mop_updateobject_test PRIVATE game)
add_test(NAME mop_updateobject COMMAND mop_updateobject_test)

add_executable(mop_opcode_foundation_test mop_opcode_foundation_test.cpp)
target_include_directories(mop_opcode_foundation_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_opcode_foundation_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_opcode_foundation_test PRIVATE game)
add_test(NAME mop_opcode_foundation COMMAND mop_opcode_foundation_test)

add_test(NAME mop_opcode_foundation_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_opcode_foundation_source_test.cmake)

add_executable(mop_compact_packets_test mop_compact_packets_test.cpp)
target_include_directories(mop_compact_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_compact_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_compact_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_compact_packets_test PRIVATE shared)
add_test(NAME mop_compact_packets COMMAND mop_compact_packets_test)

add_test(NAME mop_compact_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_compact_packets_source_test.cmake)

add_executable(mop_combat_log_packets_test mop_combat_log_packets_test.cpp)
target_include_directories(mop_combat_log_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_combat_log_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_combat_log_packets_test PRIVATE game)
add_test(NAME mop_combat_log_packets COMMAND mop_combat_log_packets_test)

add_executable(mop_aura_packets_test mop_aura_packets_test.cpp)
target_include_directories(mop_aura_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_aura_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_aura_packets_test PRIVATE game)
add_test(NAME mop_aura_packets COMMAND mop_aura_packets_test)

add_test(NAME mop_aura_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_aura_packets_source_test.cmake)
foreach(mutation IN ITEMS full_update_mode flag_translation registration whitelist)
    add_test(NAME mop_aura_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR} -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_aura_packets_source_test.cmake)
    set_tests_properties(mop_aura_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_combat_log_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_combat_log_packets_source_test.cmake)

add_executable(mop_respec_packets_test mop_respec_packets_test.cpp)
target_include_directories(mop_respec_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_respec_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_respec_packets_test PRIVATE game)
add_test(NAME mop_respec_packets COMMAND mop_respec_packets_test)

add_test(NAME mop_respec_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_respec_packets_source_test.cmake)

add_executable(mop_party_stats_packets_test mop_party_stats_packets_test.cpp)
target_include_directories(mop_party_stats_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_party_stats_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_party_stats_packets_test PRIVATE game)
add_test(NAME mop_party_stats_packets COMMAND mop_party_stats_packets_test)

add_test(NAME mop_party_stats_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_party_stats_packets_source_test.cmake)

add_executable(mop_stable_packets_test mop_stable_packets_test.cpp)
target_include_directories(mop_stable_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_stable_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_stable_packets_test PRIVATE game)
add_test(NAME mop_stable_packets COMMAND mop_stable_packets_test)

add_test(NAME mop_stable_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_stable_packets_source_test.cmake)

add_executable(mop_ready_check_packets_test mop_ready_check_packets_test.cpp)
target_include_directories(mop_ready_check_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_ready_check_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_ready_check_packets_test PRIVATE game)
add_test(NAME mop_ready_check_packets COMMAND mop_ready_check_packets_test)

add_test(NAME mop_ready_check_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_ready_check_source_test.cmake)

add_executable(mop_group_marker_packets_test mop_group_marker_packets_test.cpp)
target_include_directories(mop_group_marker_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_group_marker_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_group_marker_packets_test PRIVATE game)
add_test(NAME mop_group_marker_packets COMMAND mop_group_marker_packets_test)

add_test(NAME mop_group_marker_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_group_marker_source_test.cmake)

add_executable(mop_auction_packets_test mop_auction_packets_test.cpp)
target_include_directories(mop_auction_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_auction_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_auction_packets_test PRIVATE game)
add_test(NAME mop_auction_packets COMMAND mop_auction_packets_test)

add_test(NAME mop_auction_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_auction_packets_source_test.cmake)

add_executable(mop_world_entry_packets_test mop_world_entry_packets_test.cpp)
target_include_directories(mop_world_entry_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_world_entry_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_world_entry_packets_test PRIVATE game)
add_test(NAME mop_world_entry_packets COMMAND mop_world_entry_packets_test)

add_test(NAME mop_world_entry_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_world_entry_packets_source_test.cmake)

add_executable(mop_control_packets_test mop_control_packets_test.cpp)
target_include_directories(mop_control_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_control_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_control_packets_test PRIVATE game)
add_test(NAME mop_control_packets COMMAND mop_control_packets_test)

add_executable(mop_currency_packets_test mop_currency_packets_test.cpp)
target_include_directories(mop_currency_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_currency_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_currency_packets_test PRIVATE game)
add_test(NAME mop_currency_packets COMMAND mop_currency_packets_test)

add_test(NAME mop_currency_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_currency_packets_source_test.cmake)

add_test(NAME mop_initial_mover_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_initial_mover_source_test.cmake)

add_executable(mop_initial_packets_test mop_initial_packets_test.cpp)
target_include_directories(mop_initial_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_initial_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_initial_packets_test PRIVATE game)
add_test(NAME mop_initial_packets COMMAND mop_initial_packets_test)

add_test(NAME mop_initial_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_initial_packets_source_test.cmake)

add_executable(mop_achievement_packets_test mop_achievement_packets_test.cpp)
target_include_directories(mop_achievement_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_achievement_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_achievement_packets_test PRIVATE game)
add_test(NAME mop_achievement_packets COMMAND mop_achievement_packets_test)

add_test(NAME mop_ui_envelope_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_ui_envelope_source_test.cmake)

foreach(mutation IN ITEMS pre_map_call after_map_call opcode_metadata)
    add_test(NAME mop_ui_envelope_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_ui_envelope_source_test.cmake)
    set_tests_properties(mop_ui_envelope_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

foreach(mutation IN ITEMS builder_call opcode_metadata binder_reader
        binder_confirm_builder playerbound_builder binder_registration
        binder_allowlist)
    add_test(NAME mop_initial_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_initial_packets_source_test.cmake)
    set_tests_properties(mop_initial_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_creature_query_test mop_creature_query_test.cpp)
target_include_directories(mop_creature_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_creature_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_creature_query_test PRIVATE game)
add_test(NAME mop_creature_query COMMAND mop_creature_query_test)

add_test(NAME mop_creature_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_creature_query_source_test.cmake)

foreach(mutation IN ITEMS builder_call inbound_registration)
    add_test(NAME mop_creature_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_creature_query_source_test.cmake)
    set_tests_properties(mop_creature_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_gameobject_query_test mop_gameobject_query_test.cpp)
target_include_directories(mop_gameobject_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_gameobject_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_gameobject_query_test PRIVATE game)
add_test(NAME mop_gameobject_query COMMAND mop_gameobject_query_test)

add_test(NAME mop_gameobject_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gameobject_query_source_test.cmake)

foreach(mutation IN ITEMS request_parser_call response_builder_call inbound_registration)
    add_test(NAME mop_gameobject_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gameobject_query_source_test.cmake)
    set_tests_properties(mop_gameobject_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_corpse_query_test mop_corpse_query_test.cpp)
target_include_directories(mop_corpse_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_corpse_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_corpse_query_test PRIVATE game)
add_test(NAME mop_corpse_query COMMAND mop_corpse_query_test)

add_test(NAME mop_corpse_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_corpse_query_source_test.cmake)

foreach(mutation IN ITEMS query_builder map_reader map_builder
        inbound_registration outbound_registration allowlist)
    add_test(NAME mop_corpse_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_corpse_query_source_test.cmake)
    set_tests_properties(mop_corpse_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_name_query_test mop_name_query_test.cpp)
target_include_directories(mop_name_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_name_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_name_query_test PRIVATE game)
add_test(NAME mop_name_query COMMAND mop_name_query_test)

add_test(NAME mop_name_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_name_query_source_test.cmake)

add_executable(mop_realmquery_test mop_realmquery_test.cpp)
target_include_directories(mop_realmquery_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_realmquery_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_realmquery_test PRIVATE game)
add_test(NAME mop_realmquery COMMAND mop_realmquery_test)

foreach(mutation IN ITEMS request_parser_call response_builder_call inbound_registration
        early_result_delete tail_result_delete)
    add_test(NAME mop_name_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_name_query_source_test.cmake)
    set_tests_properties(mop_name_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_time_query_test mop_time_query_test.cpp)
target_include_directories(mop_time_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_time_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_time_query_test PRIVATE game)
add_test(NAME mop_time_query COMMAND mop_time_query_test)

add_test(NAME mop_time_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_time_query_source_test.cmake)

foreach(mutation IN ITEMS query_builder_call played_builder_call query_registration played_registration)
    add_test(NAME mop_time_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_time_query_source_test.cmake)
    set_tests_properties(mop_time_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_mail_query_test mop_mail_query_test.cpp)
target_include_directories(mop_mail_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_mail_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_mail_query_test PRIVATE game)
add_test(NAME mop_mail_query COMMAND mop_mail_query_test)

add_test(NAME mop_mail_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mail_query_source_test.cmake)

foreach(mutation IN ITEMS builder_call inbound_registration outbound_registration)
    add_test(NAME mop_mail_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mail_query_source_test.cmake)
    set_tests_properties(mop_mail_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_rated_bg_packets_test mop_rated_bg_packets_test.cpp)
target_include_directories(mop_rated_bg_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_rated_bg_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_rated_bg_packets_test PRIVATE game)
add_test(NAME mop_rated_bg_packets COMMAND mop_rated_bg_packets_test)

add_test(NAME mop_rated_bg_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_rated_bg_packets_source_test.cmake)

foreach(mutation IN ITEMS builder_call inbound_registration outbound_registration)
    add_test(NAME mop_rated_bg_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_rated_bg_packets_source_test.cmake)
    set_tests_properties(mop_rated_bg_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_calendar_packets_test mop_calendar_packets_test.cpp)
target_include_directories(mop_calendar_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_calendar_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_calendar_packets_test PRIVATE game)
add_test(NAME mop_calendar_packets COMMAND mop_calendar_packets_test)

add_test(NAME mop_calendar_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_calendar_packets_source_test.cmake)

foreach(mutation IN ITEMS initial_builder status_builder moderator_builder initial_registration status_registration moderator_registration)
    add_test(NAME mop_calendar_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_calendar_packets_source_test.cmake)
    set_tests_properties(mop_calendar_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_party_update_packets_test mop_party_update_packets_test.cpp)
target_include_directories(mop_party_update_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_party_update_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_party_update_packets_test PRIVATE game)
add_test(NAME mop_party_update_packets COMMAND mop_party_update_packets_test)

add_executable(mop_guild_packets_test mop_guild_packets_test.cpp)
target_include_directories(mop_guild_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_guild_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_guild_packets_test PRIVATE game)
add_test(NAME mop_guild_packets COMMAND mop_guild_packets_test)

add_executable(mop_chat_packets_test mop_chat_packets_test.cpp)
target_include_directories(mop_chat_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_chat_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_chat_packets_test PRIVATE game)
add_test(NAME mop_chat_packets COMMAND mop_chat_packets_test)

add_test(NAME mop_chat_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_chat_packets_source_test.cmake)

foreach(mutation IN ITEMS builder registration allowlist cross_faction_denial text_bound receiver_identity addon_identity
        legacy_wrong_faction legacy_gm_chat legacy_member_count)
    add_test(NAME mop_chat_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_chat_packets_source_test.cmake)
    set_tests_properties(mop_chat_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_pet_unlearn_legacy_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_unlearn_legacy_source_test.cmake)

foreach(mutation IN ITEMS hidden_option legacy_sender legacy_handler legacy_opcodes)
    add_test(NAME mop_pet_unlearn_legacy_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_unlearn_legacy_source_test.cmake)
    set_tests_properties(mop_pet_unlearn_legacy_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_lfg_packets_test mop_lfg_packets_test.cpp)
target_include_directories(mop_lfg_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_lfg_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_lfg_packets_test PRIVATE game)
add_test(NAME mop_lfg_packets COMMAND mop_lfg_packets_test)

add_test(NAME mop_lfg_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_lfg_packets_source_test.cmake)

foreach(mutation IN ITEMS builder_call outbound_registration opcode_value countdown
        status_builder_call status_registration status_opcode status_field_mapping status_reason_value)
    add_test(NAME mop_lfg_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_lfg_packets_source_test.cmake)
    set_tests_properties(mop_lfg_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_guild_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_guild_packets_source_test.cmake)

foreach(mutation IN ITEMS login_builder broadcast_builder outbound_registration opcode_value
        tabard_reader tabard_builder save_reader save_builder tabard_registration
        save_registration tabard_allowlist design_bounds joined_builder presence_builder
        rank_builder leader_builder left_builder disband_builder event_registration
        event_allowlist event_opcode presence_calls removed_member_copy legacy_info legacy_event)
    add_test(NAME mop_guild_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_guild_packets_source_test.cmake)
    set_tests_properties(mop_guild_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_party_update_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_party_update_packets_source_test.cmake)

foreach(mutation IN ITEMS normal_builder removal_builder targeted_request inbound_registration outbound_registration)
    add_test(NAME mop_party_update_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_party_update_packets_source_test.cmake)
    set_tests_properties(mop_party_update_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_enter_world_gate_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_enter_world_gate_source_test.cmake)

foreach(mutation IN ITEMS converted_packet transfer_registration)
    add_test(NAME mop_enter_world_gate_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_enter_world_gate_source_test.cmake)
    set_tests_properties(mop_enter_world_gate_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_gm_ticket_packets_test mop_gm_ticket_packets_test.cpp)
target_include_directories(mop_gm_ticket_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_gm_ticket_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_gm_ticket_packets_test PRIVATE game)
add_test(NAME mop_gm_ticket_packets COMMAND mop_gm_ticket_packets_test)

add_test(NAME mop_gm_ticket_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gm_ticket_packets_source_test.cmake)

foreach(mutation IN ITEMS response_width callsite registration whitelist opcode_value)
    add_test(NAME mop_gm_ticket_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gm_ticket_packets_source_test.cmake)
    set_tests_properties(mop_gm_ticket_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_movement_packets_test
    mop_movement_packets_test.cpp
    ${CMAKE_SOURCE_DIR}/src/game/movement/MovementInfo.cpp)
target_include_directories(mop_movement_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_movement_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_movement_packets_test PRIVATE game)
add_test(NAME mop_movement_packets COMMAND mop_movement_packets_test)

add_test(NAME mop_movement_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_movement_source_test.cmake)

foreach(mutation IN ITEMS inbound_sequence player_move_sequence allocation_bound
        flags2_width inbound_registration output_metadata heartbeat_framing mover_guid_validation
        spline_state_builder spline_state_registration monster_move_writer monster_move_registration
        monster_move_integration monster_move_gate)
    add_test(NAME mop_movement_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_movement_source_test.cmake)
    set_tests_properties(mop_movement_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_death_packets_test mop_death_packets_test.cpp)
target_include_directories(mop_death_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_death_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_death_packets_test PRIVATE game)
add_test(NAME mop_death_packets COMMAND mop_death_packets_test)

add_test(NAME mop_death_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_death_packets_source_test.cmake)

foreach(mutation IN ITEMS wire_order player_call misc_call registration whitelist opcode_value)
    add_test(NAME mop_death_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_death_packets_source_test.cmake)
    set_tests_properties(mop_death_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_quest_share_packets_test mop_quest_share_packets_test.cpp)
target_include_directories(mop_quest_share_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_quest_share_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_quest_share_packets_test PRIVATE game)
add_test(NAME mop_quest_share_packets COMMAND mop_quest_share_packets_test)

add_test(NAME mop_quest_share_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_share_packets_source_test.cmake)

foreach(mutation IN ITEMS confirm_builder result_builder result_reader sharer_validation result_range
        push_registration confirm_registration result_registration outbound_registration
        confirm_whitelist result_whitelist legacy_sender opcode_values result_values)
    add_test(NAME mop_quest_share_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_share_packets_source_test.cmake)
    set_tests_properties(mop_quest_share_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_arena_team_legacy_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_arena_team_legacy_source_test.cmake)

foreach(mutation IN ITEMS legacy_sender legacy_handler backend)
    add_test(NAME mop_arena_team_legacy_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_arena_team_legacy_source_test.cmake)
    set_tests_properties(mop_arena_team_legacy_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_petition_legacy_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_petition_legacy_source_test.cmake)

foreach(mutation IN ITEMS legacy_sender legacy_handler)
    add_test(NAME mop_petition_legacy_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_petition_legacy_source_test.cmake)
    set_tests_properties(mop_petition_legacy_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()
