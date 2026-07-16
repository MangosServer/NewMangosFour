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

add_executable(mop_charenum_test
    mop_charenum_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MopCharEnum.cpp)
target_include_directories(mop_charenum_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_charenum_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# Same rationale as mop_auth_test: WorldPacket/Opcodes pull the game header surface; link 'game'.
target_link_libraries(mop_charenum_test PRIVATE game)
add_test(NAME mop_charenum COMMAND mop_charenum_test)
