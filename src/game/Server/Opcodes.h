/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/// \addtogroup u2w
/// @{
/// \file

#ifndef MANGOS_H_OPCODES
#define MANGOS_H_OPCODES

#include "Common.h"
#include "Policies/Singleton.h"

// Note: this include need for be sure have full definition of class WorldSession
//       if this class definition not complite then VS for x64 release use different size for
//       struct OpcodeHandler in this header and Opcode.cpp and get totally wrong data from
//       table opcodeTable in source when Opcode.h included but WorldSession.h not included
#include "WorldSession.h"

/**
 * This is a list of Opcodes that are known for the client/server communication, it is used
 * to tell the server to do something or the client to do something. Every opcode is handled
 * in some way, and you can find what functions handle what opcode in the implementation of
 * \ref Opcodes::BuildOpcodeList
 *
 * To send messages the following functions can be used: \ref WorldObject::SendMessageToSet,
 * \ref WorldObject::SendMessageToSetExcept, \ref WorldObject::SendMessageToSetInRange
 *
 * \see WorldPacket
 * \todo Replace the Pack GUID part with a packed GUID, ie: it's shorter than usual?
 */
enum OpcodesList
{
    MSG_WOW_CONNECTION                           = 0x4F57, // 5.4.8 18414 (SkyFire)
    SMSG_AUTH_CHALLENGE                          = 0x0949, // 5.4.8 18414 (SkyFire)
    CMSG_AUTH_SESSION                            = 0x00B2, // 5.4.8 18414 (Wow.exe binary)
    SMSG_AUTH_RESPONSE                           = 0x0ABA, // 5.4.8 18414 (SkyFire)
    MSG_NULL_ACTION                              = 0x1001,
    SMSG_DBLOOKUP                                = 0x1004,
    SMSG_QUERY_OBJECT_POSITION                   = 0x1006,
    SMSG_QUERY_OBJECT_ROTATION                   = 0x1008,
    CMSG_WORLD_TELEPORT                          = 0x24B2, // 4.3.4 15595
    CMSG_TELEPORT_TO_UNIT                        = 0x4206, // 4.3.4 15595
    SMSG_ZONE_MAP                                = 0x100C,
    SMSG_MOVE_CHARACTER_CHEAT                    = 0x100F,
    SMSG_CHECK_FOR_BOTS                          = 0x1016,
    SMSG_FORCEACTIONSHOW                         = 0x101C,
    SMSG_PETGODMODE                              = 0x101E,
    SMSG_REFER_A_FRIEND_EXPIRED                  = 0x101F,
    SMSG_GODMODE                                 = 0x1024,
    SMSG_DEBUG_AISTATE                           = 0x1030,
    CMSG_DESTROY_ITEM                            = 0x0026, // 5.4.8 18414 (Wow.exe binary)
    SMSG_DESTRUCTIBLE_BUILDING_DAMAGE            = 0x4825, // 4.3.4 15595
    CMSG_CHAR_CREATE                             = 0x0F1D, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHAR_ENUM                               = 0x00E0, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHAR_DELETE                             = 0x04E2, // 5.4.8 18414 (Wow.exe binary)
    SMSG_AUTH_SRP6_RESPONSE                      = 0x103A,
    SMSG_CHAR_CREATE                             = 0x1CAA, // 5.4.8 18414 (SkyFire) (was 0x11C3)
    SMSG_CHAR_ENUM                               = 0x11C3, // 5.4.8 18414 (SkyFire)
    SMSG_CHAR_DELETE                             = 0x0C9F, // 5.4.8 18414
    CMSG_PLAYER_LOGIN                            = 0x158F, // 5.4.8 18414 (Wow.exe binary)
    SMSG_NEW_WORLD                               = 0x04D9, // 5.3.0
    SMSG_TRANSFER_PENDING                        = 0x18A6, // 4.3.4 15595
    SMSG_TRANSFER_ABORTED                        = 0x0537, // 4.3.4 15595
    SMSG_CHARACTER_LOGIN_FAILED                  = 0x4417, // 4.3.4 15595
    SMSG_LOGIN_SETTIMESPEED                      = 0x082B, // 5.4.8 18414 (SkyFire) (was 0x4D15)
    SMSG_GAMETIME_UPDATE                         = 0x4127, // 4.3.4 15595
    SMSG_GAMETIME_SET                            = 0x0014, // 4.3.4 15595
    SMSG_GAMESPEED_SET                           = 0x1048,
    SMSG_SERVERTIME                              = 0x6327, // 4.3.4 15595
    CMSG_PLAYER_LOGOUT                           = 0x104B, // not in 5.4.8 (legacy; handler retained)
    CMSG_LOGOUT_REQUEST                          = 0x1349, // 5.4.8 18414 (Wow.exe binary)
    SMSG_LOGOUT_RESPONSE                         = 0x0524, // 4.3.4 15595
    SMSG_LOGOUT_COMPLETE                         = 0x2137, // 4.3.4 15595
    CMSG_LOGOUT_CANCEL                           = 0x06C1, // 5.4.8 18414 (Wow.exe binary)
    SMSG_LOGOUT_CANCEL_ACK                       = 0x6514, // 4.3.4 15595
    CMSG_NAME_QUERY                              = 0x0328, // 5.4.8 18414 (Wow.exe binary)
    SMSG_NAME_QUERY_RESPONSE                     = 0x6E04, // 4.3.4 15595
    CMSG_PET_NAME_QUERY                          = 0x1C62, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PET_NAME_QUERY_RESPONSE                 = 0x4C37, // 4.3.4 15595
    CMSG_GUILD_QUERY                             = 0x1AB6, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GUILD_QUERY_RESPONSE                    = 0x0E06, // 4.3.4 15595
    CMSG_PAGE_TEXT_QUERY                         = 0x1022, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PAGE_TEXT_QUERY_RESPONSE                = 0x2B14, // 4.3.4 15595
    CMSG_QUEST_QUERY                             = 0x02D5, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUEST_QUERY_RESPONSE                    = 0x6936, // 4.3.4 15595
    CMSG_GAMEOBJECT_QUERY                        = 0x1461, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GAMEOBJECT_QUERY_RESPONSE               = 0x0915, // 4.3.4 15595
    CMSG_CREATURE_QUERY                          = 0x0842, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CREATURE_QUERY_RESPONSE                 = 0x6024, // 4.3.4 15595
    CMSG_WHO                                     = 0x18A3, // 5.4.8 18414 (Wow.exe binary)
    SMSG_WHO                                     = 0x6907, // 4.3.4 15595
    CMSG_WHOIS                                   = 0x0308, // 5.3.0 17128
    SMSG_WHOIS                                   = 0x6917, // 4.3.4 15595
    CMSG_CONTACT_LIST                            = 0x0BB4, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CONTACT_LIST                            = 0x6017, // 4.3.4 15595
    SMSG_FRIEND_STATUS                           = 0x0717, // 4.3.4 15595
    CMSG_ADD_FRIEND                              = 0x09A6, // 5.4.8 18414 (Wow.exe binary)
    CMSG_DEL_FRIEND                              = 0x1103, // 5.4.8 18414 (Wow.exe binary)
    CMSG_SET_CONTACT_NOTES                       = 0x0937, // 5.4.8 18414 (Wow.exe binary)
    CMSG_ADD_IGNORE                              = 0x0D20, // 5.4.8 18414 (Wow.exe binary)
    CMSG_DEL_IGNORE                              = 0x0737, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GROUP_INVITE                            = 0x072D, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GROUP_INVITE                            = 0x31B2, // 4.3.4 15595
    SMSG_GROUP_CANCEL                            = 0x4D25, // 4.3.4 15595
    CMSG_GROUP_INVITE_RESPONSE                   = 0x0D61, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GROUP_DECLINE                           = 0x6835, // 4.3.4 15595
    CMSG_GROUP_UNINVITE                          = 0x1076,
    CMSG_GROUP_UNINVITE_GUID                     = 0x0CE1, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GROUP_UNINVITE                          = 0x0A07, // 4.3.4 15595
    CMSG_GROUP_SET_LEADER                        = 0x15BB, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GROUP_SET_LEADER                        = 0x0526, // 4.3.4 15595
    CMSG_LOOT_METHOD                             = 0x0DE1, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GROUP_DISBAND                           = 0x1798, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GROUP_DESTROYED                         = 0x2207, // 4.3.4 15595
    SMSG_GROUP_LIST                              = 0x4C24, // 4.3.4 15595
    SMSG_PARTY_MEMBER_STATS                      = 0x2104, // 4.3.4 15595
    SMSG_PARTY_COMMAND_RESULT                    = 0x6E07, // 4.3.4 15595
    CMSG_GUILD_CREATE                            = 0x1082, // not in 5.4.8 (legacy; handler retained)
    CMSG_GUILD_INVITE                            = 0x0869, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GUILD_INVITE                            = 0x14A2, // 4.3.4 15595
    CMSG_GUILD_ACCEPT                            = 0x18A2, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_DECLINE                           = 0x147B, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GUILD_DECLINE                           = 0x2C07, // 4.3.4 15595
    CMSG_GUILD_INFO                              = 0x1088,
    SMSG_GUILD_INFO                              = 0x1089,
    CMSG_GUILD_ROSTER                            = 0x1459, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GUILD_ROSTER                            = 0x3DA3, // 4.3.4 15595
    CMSG_GUILD_PROMOTE                           = 0x0571, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_SET_RANK                          = 0x0C7A, // 5.4.8 18414 (Wow.exe binary, via CMSG_GUILD_SET_RANK_PERMISSIONS)
    CMSG_GUILD_SWITCH_RANK                       = 0x0CD1, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_DEMOTE                            = 0x1553, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_LEAVE                             = 0x04D8, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_REMOVE                            = 0x0CD8, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_DISBAND                           = 0x0D73, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_LEADER                            = 0x3034, // not in 5.4.8 (legacy; handler retained)
    CMSG_GUILD_MOTD                              = 0x1473, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GUILD_EVENT                             = 0x0705, // 4.3.4 15595
    SMSG_GUILD_COMMAND_RESULT                    = 0x7DB3, // 4.3.4 15595
    CMSG_GUILD_AUTO_DECLINE_TOGGLE               = 0x2034, // 4.3.4 15595
    CMSG_GUILD_AUTO_DECLINE                      = 0x06CB, // 5.4.8 18414 (Wow.exe binary, via CMSG_AUTO_DECLINE_GUILD_INVITES)
    CMSG_GUILD_QUERY_RANKS                       = 0x0D50, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GUILD_QUERY_RANKS_RESULT                = 0x30B4, // 4.3.4 15595
    CMSG_MESSAGECHAT_ADDON_INSTANCE              = 0x08AF, // 5.4.8 18414 (Wow.exe binary, via CMSG_MESSAGECHAT_ADDON_INSTANCE_CHAT)
    CMSG_MESSAGECHAT_ADDON_GUILD                 = 0x0E3B, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_ADDON_OFFICER               = 0x180B, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_ADDON_PARTY                 = 0x028E, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_ADDON_RAID                  = 0x009A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_ADDON_WHISPER               = 0x0EBB, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_AFK                         = 0x0EAB, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_BATTLEGROUND                = 0x2156, // 4.3.4 15595
    CMSG_MESSAGECHAT_CHANNEL                     = 0x00BB, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_DND                         = 0x002E, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_EMOTE                       = 0x103E, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_GUILD                       = 0x0CAE, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_INSTANCE                    = 0x162A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_OFFICER                     = 0x0ABF, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_PARTY                       = 0x109A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_RAID                        = 0x083E, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_RAID_WARNING                = 0x16AB, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_SAY                         = 0x0A9A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_WHISPER                     = 0x123E, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MESSAGECHAT_YELL                        = 0x04AA, // 5.4.8 18414 (Wow.exe binary)
    SMSG_MESSAGECHAT                             = 0x2026, // 4.3.4 15595
    CMSG_JOIN_CHANNEL                            = 0x148E, // 5.4.8 18414 (Wow.exe binary)
    CMSG_LEAVE_CHANNEL                           = 0x042A, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CHANNEL_NOTIFY                          = 0x0825, // 5.3.0 17128
    CMSG_CHANNEL_LIST                            = 0x0C1B, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CHANNEL_LIST                            = 0x2214, // 4.3.4 15595
    CMSG_CHANNEL_PASSWORD                        = 0x0A1E, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_SET_OWNER                       = 0x141A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_OWNER                           = 0x00AF, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_MODERATOR                       = 0x00AE, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_UNMODERATOR                     = 0x041E, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_MUTE                            = 0x000A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_UNMUTE                          = 0x022A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_INVITE                          = 0x10AB, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_KICK                            = 0x0E0B, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_BAN                             = 0x08BF, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_UNBAN                           = 0x081F, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_ANNOUNCEMENTS                   = 0x06AF, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CHANNEL_MODERATE                        = 0x2944, // 4.3.4 15595
    SMSG_UPDATE_OBJECT                           = 0x1792, // 5.4.8 18414
    SMSG_DESTROY_OBJECT                          = 0x4724, // 4.3.4 15595
    SMSG_MOVE_SET_ACTIVE_MOVER                  = 0x0C6D, // 5.4.8 18414
    CMSG_USE_ITEM                                = 0x1CC1, // 5.4.8 18414 (Wow.exe binary)
    CMSG_OPEN_ITEM                               = 0x1D10, // 5.4.8 18414 (Wow.exe binary)
    CMSG_READ_ITEM                               = 0x0D00, // 5.4.8 18414 (Wow.exe binary)
    SMSG_READ_ITEM_OK                            = 0x2605, // 4.3.4 15595
    SMSG_READ_ITEM_FAILED                        = 0x0F16, // 4.3.4 15595
    SMSG_ITEM_COOLDOWN                           = 0x4D14, // 4.3.4 15595
    CMSG_GAMEOBJ_USE                             = 0x06D8, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GAMEOBJECT_CUSTOM_ANIM                  = 0x4936, // 4.3.4 15595
    CMSG_AREATRIGGER                             = 0x1C44, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MOVE_START_FORWARD                      = 0x095A, // 5.4.8 18414
    CMSG_MOVE_START_BACKWARD                     = 0x09D8, // 5.4.8 18414
    CMSG_MOVE_STOP                               = 0x08F1, // 5.4.8 18414
    CMSG_MOVE_START_STRAFE_LEFT                  = 0x01F8, // 5.4.8 18414
    CMSG_MOVE_START_STRAFE_RIGHT                 = 0x1058, // 5.4.8 18414
    CMSG_MOVE_STOP_STRAFE                        = 0x0171, // 5.4.8 18414
    CMSG_MOVE_JUMP                               = 0x1153, // 5.4.8 18414
    CMSG_MOVE_START_TURN_LEFT                    = 0x01D0, // 5.4.8 18414
    CMSG_MOVE_START_TURN_RIGHT                   = 0x107B, // 5.4.8 18414
    CMSG_MOVE_STOP_TURN                          = 0x1170, // 5.4.8 18414
    CMSG_MOVE_START_PITCH_UP                     = 0x00D8, // 5.4.8 18414
    CMSG_MOVE_START_PITCH_DOWN                   = 0x08D8, // 5.4.8 18414
    CMSG_MOVE_STOP_PITCH                         = 0x007A, // 5.4.8 18414
    CMSG_MOVE_SET_RUN_MODE                       = 0x0748, // not in 5.4.8 (legacy; handler retained)
    CMSG_MOVE_SET_WALK_MODE                      = 0x0BE1, // not in 5.4.8 (legacy; handler retained)
    MSG_MOVE_TOGGLE_LOGGING                      = 0x10C5,
    SMSG_MOVE_TELEPORT                           = 0x0C54, // 5.3.0
    MSG_MOVE_TELEPORT_CHEAT                      = 0x10C7,
    CMSG_MOVE_TELEPORT_ACK                       = 0x0078, // 5.4.8 18414 (Wow.exe binary)
    MSG_MOVE_TOGGLE_FALL_LOGGING                 = 0x10C9,
    CMSG_MOVE_FALL_LAND                          = 0x08FA, // 5.4.8 18414
    CMSG_MOVE_START_SWIM                         = 0x1858, // 5.4.8 18414
    CMSG_MOVE_STOP_SWIM                          = 0x0950, // 5.4.8 18414
    MSG_MOVE_SET_RUN_SPEED_CHEAT                 = 0x10CD,
    SMSG_MOVE_SET_RUN_SPEED                      = 0x0758, // 5.3.0 17128
    MSG_MOVE_SET_RUN_BACK_SPEED_CHEAT            = 0x10CF,
    SMSG_MOVE_SET_RUN_BACK_SPEED                 = 0x71B1, // 4.3.4 15595
    MSG_MOVE_SET_WALK_SPEED_CHEAT                = 0x10D1,
    SMSG_MOVE_SET_WALK_SPEED                     = 0x051D, // 5.3.0
    MSG_MOVE_SET_SWIM_SPEED_CHEAT                = 0x10D3,
    SMSG_MOVE_SET_SWIM_SPEED                     = 0x0905, // 5.3.0 17128
    MSG_MOVE_SET_SWIM_BACK_SPEED_CHEAT           = 0x10D5,
    SMSG_MOVE_SET_SWIM_BACK_SPEED                = 0x5CA6, // 4.3.4 15595
    MSG_MOVE_SET_ALL_SPEED_CHEAT                 = 0x10D7,
    MSG_MOVE_SET_TURN_RATE_CHEAT                 = 0x10D8,
    SMSG_MOVE_SET_TURN_RATE                      = 0x30A5, // 4.3.4 15595
    MSG_MOVE_TOGGLE_COLLISION_CHEAT              = 0x0BC8, // 5.4.1 17538
    CMSG_MOVE_SET_FACING                         = 0x1050, // 5.4.8 18414
    CMSG_MOVE_SET_PITCH                          = 0x0261, // not in 5.4.8 (legacy; handler retained)
    MSG_MOVE_WORLDPORT_ACK                       = 0x00E0, // 5.4.1 17538
    SMSG_MONSTER_MOVE                            = 0x0216, // 5.4.1 17538
    SMSG_MOVE_WATER_WALK                         = 0x75B1, // 4.3.4 15595
    SMSG_MOVE_LAND_WALK                          = 0x34B7, // 4.3.4 15595
    SMSG_FORCE_RUN_SPEED_CHANGE                  = 0x10E3,
    CMSG_FORCE_RUN_SPEED_CHANGE_ACK              = 0x10F3, // 5.4.8 18414 (Wow.exe binary, via CMSG_MOVE_FORCE_RUN_SPEED_CHANGE_ACK)
    SMSG_FORCE_RUN_BACK_SPEED_CHANGE             = 0x10E5,
    CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK         = 0x0158, // 5.4.8 18414 (Wow.exe binary, via CMSG_MOVE_FORCE_RUN_BACK_SPEED_CHANGE_ACK)
    SMSG_FORCE_SWIM_SPEED_CHANGE                 = 0x10E7,
    CMSG_FORCE_SWIM_SPEED_CHANGE_ACK             = 0x1853, // 5.4.8 18414 (Wow.exe binary, via CMSG_MOVE_FORCE_SWIM_SPEED_CHANGE_ACK)
    SMSG_FORCE_MOVE_ROOT                         = 0x7DA0, // 4.3.4 15595
    CMSG_FORCE_MOVE_ROOT_ACK                     = 0x107A, // 5.4.8 18414 (Wow.exe binary)
    SMSG_FORCE_MOVE_UNROOT                       = 0x7DB4, // 4.3.4 15595
    CMSG_FORCE_MOVE_UNROOT_ACK                   = 0x1051, // 5.4.8 18414 (Wow.exe binary)
    MSG_MOVE_ROOT                                = 0x10ED,
    MSG_MOVE_UNROOT                              = 0x10EE,
    MSG_MOVE_HEARTBEAT                           = 0x01F2, // 5.4.8 18414
    SMSG_MOVE_KNOCK_BACK                         = 0x5CB4, // 4.3.4 15595
    CMSG_MOVE_KNOCK_BACK_ACK                     = 0x00F2, // 5.4.8 18414 (Wow.exe binary)
    SMSG_MOVE_UPDATE_KNOCK_BACK                  = 0x3DB2, // 4.3.4 15595
    SMSG_MOVE_FEATHER_FALL                       = 0x79B0, // 4.3.4 15595
    SMSG_MOVE_NORMAL_FALL                        = 0x51B6, // 4.3.4 15595
    SMSG_MOVE_SET_HOVER                          = 0x5CB3, // 4.3.4 15595
    SMSG_MOVE_UNSET_HOVER                        = 0x51B3, // 4.3.4 15595
    CMSG_MOVE_HOVER_ACK                          = 0x0858, // 5.4.8 18414 (Wow.exe binary)
    MSG_MOVE_HOVER                               = 0x10F8,
    CMSG_OPENING_CINEMATIC                       = 0x0130, // 5.4.8 18414 (Wow.exe binary)
    SMSG_TRIGGER_CINEMATIC                       = 0x6C27, // 4.3.4 15595
    CMSG_NEXT_CINEMATIC_CAMERA                   = 0x1124, // 5.4.8 18414 (Wow.exe binary)
    CMSG_COMPLETE_CINEMATIC                      = 0x1F34, // 5.4.8 18414 (Wow.exe binary)
    SMSG_TUTORIAL_FLAGS                          = 0x1B90, // 5.4.8 18414 (SkyFire)
    CMSG_TUTORIAL_FLAG                           = 0x6C26, // 4.3.4 15595
    CMSG_TUTORIAL_CLEAR                          = 0x0F23, // 5.4.8 18414 (Wow.exe binary)
    CMSG_TUTORIAL_RESET                          = 0x0307, // 5.4.8 18414 (Wow.exe binary)
    CMSG_STANDSTATECHANGE                        = 0x03E6, // 5.4.8 18414 (Wow.exe binary)
    CMSG_EMOTE                                   = 0x1924, // 5.4.8 18414 (Wow.exe binary)
    SMSG_EMOTE                                   = 0x0A34, // 4.3.4 15595
    CMSG_TEXT_EMOTE                              = 0x07E9, // 5.4.8 18414 (Wow.exe binary)
    SMSG_TEXT_EMOTE                              = 0x0B05, // 4.3.4 15595
    CMSG_AUTOEQUIP_GROUND_ITEM                   = 0x1107,
    CMSG_AUTOSTORE_LOOT_ITEM                     = 0x0354, // 5.4.8 18414 (Wow.exe binary)
    CMSG_AUTOEQUIP_ITEM                          = 0x025F, // 5.4.8 18414 (Wow.exe binary)
    CMSG_AUTOSTORE_BAG_ITEM                      = 0x067C, // 5.4.8 18414 (Wow.exe binary)
    CMSG_SWAP_ITEM                               = 0x035D, // 5.4.8 18414 (Wow.exe binary)
    CMSG_SWAP_INV_ITEM                           = 0x03DF, // 5.4.8 18414 (Wow.exe binary)
    CMSG_SPLIT_ITEM                              = 0x02EC, // 5.4.8 18414 (Wow.exe binary)
    CMSG_AUTOEQUIP_ITEM_SLOT                     = 0x036F, // 5.4.8 18414 (Wow.exe binary)
    SMSG_INVENTORY_CHANGE_FAILURE                = 0x2236, // 4.3.4 15595
    SMSG_OPEN_CONTAINER                          = 0x4714, // 4.3.4 15595
    CMSG_INSPECT                                 = 0x1259, // 5.4.8 18414 (Wow.exe binary)
    SMSG_INSPECT_RESULTS_UPDATE                  = 0x0B98, // 5.3.0 17128
    CMSG_INITIATE_TRADE                          = 0x0267, // 5.4.8 18414 (Wow.exe binary)
    CMSG_BEGIN_TRADE                             = 0x1CE3, // 5.4.8 18414 (Wow.exe binary)
    CMSG_BUSY_TRADE                              = 0x14E0, // 5.4.8 18414 (Wow.exe binary)
    CMSG_IGNORE_TRADE                            = 0x0276, // 5.4.8 18414 (Wow.exe binary)
    CMSG_ACCEPT_TRADE                            = 0x144D, // 5.4.8 18414 (Wow.exe binary)
    CMSG_UNACCEPT_TRADE                          = 0x0023, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CANCEL_TRADE                            = 0x1941, // 5.4.8 18414 (Wow.exe binary)
    CMSG_SET_TRADE_ITEM                          = 0x03D5, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CLEAR_TRADE_ITEM                        = 0x00A7, // 5.4.8 18414 (Wow.exe binary)
    CMSG_SET_TRADE_GOLD                          = 0x14E3, // 5.4.8 18414 (Wow.exe binary)
    SMSG_TRADE_STATUS                            = 0x0DC0, // 5.3.0 17128
    SMSG_TRADE_STATUS_EXTENDED                   = 0x138D, // 5.3.0 17128
    SMSG_INITIALIZE_FACTIONS                     = 0x4634, // 4.3.4 15595
    SMSG_SET_FACTION_VISIBLE                     = 0x2525, // 4.3.4 15595
    SMSG_SET_FACTION_STANDING                    = 0x0126, // 4.3.4 15595
    CMSG_SET_FACTION_ATWAR                       = 0x027B, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SET_PROFICIENCY                         = 0x6207, // 4.3.4 15595
    CMSG_SET_ACTION_BUTTON                       = 0x1F8C, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ACTION_BUTTONS                          = 0x38B5, // 4.3.4 15595
    SMSG_INITIAL_SPELLS                          = 0x0104, // 4.3.4 15595
    SMSG_LEARNED_SPELL                           = 0x058C, // 5.3.0 17128
    SMSG_SUPERCEDED_SPELL                        = 0x35B0, // 4.3.4 15595
    CMSG_CAST_SPELL                              = 0x0206, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CANCEL_CAST                             = 0x18C0, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CAST_FAILED                             = 0x1598, // 5.3.0 17128
    SMSG_SPELL_START                             = 0x6415, // 4.3.4 15595
    SMSG_SPELL_GO                                = 0x6E16, // 4.3.4 15595
    SMSG_SPELL_FAILURE                           = 0x0C34, // 4.3.4 15595
    SMSG_SPELL_COOLDOWN                          = 0x4B16, // 4.3.4 15595
    SMSG_COOLDOWN_EVENT                          = 0x4F26, // 4.3.4 15595
    CMSG_CANCEL_AURA                             = 0x1861, // 5.4.8 18414 (Wow.exe binary)
    SMSG_EQUIPMENT_SET_ID                        = 0x2216, // 4.3.4 15595
    SMSG_PET_CAST_FAILED                         = 0x2B15, // 4.3.4 15595
    SMSG_CHANNEL_START                           = 0x0A15, // 4.3.4 15595
    SMSG_CHANNEL_UPDATE                          = 0x2417, // 4.3.4 15595
    CMSG_CANCEL_CHANNELLING                      = 0x08C0, // 5.4.8 18414 (Wow.exe binary)
    SMSG_AI_REACTION                             = 0x06AF, // 5.4.8 18414
    CMSG_SET_SELECTION                           = 0x0740, // 5.4.8 18414 (Wow.exe binary)
    CMSG_EQUIPMENT_SET_DELETE                    = 0x02E8, // 5.4.8 18414 (Wow.exe binary)
    CMSG_INSTANCE_LOCK_RESPONSE                  = 0x12C0, // 5.4.8 18414 (Wow.exe binary, via CMSG_INSTANCE_LOCK_WARNING_RESPONSE)
    CMSG_ATTACKSWING                             = 0x02E7, // 5.4.8 18414 (Wow.exe binary)
    CMSG_ATTACKSTOP                              = 0x0345, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ATTACKSTART                             = 0x11D5, // 5.3.0 17128
    SMSG_ATTACKSTOP                              = 0x0690, // 5.3.0 17128
    SMSG_ATTACKSWING_NOTINRANGE                  = 0x0B36, // 4.3.4 15595
    SMSG_ATTACKSWING_BADFACING                   = 0x6C07, // 4.3.4 15595
    SMSG_PENDING_RAID_LOCK                       = 0x4F17, // 4.3.4 15595
    SMSG_ATTACKSWING_DEADTARGET                  = 0x2B26, // 4.3.4 15595
    SMSG_ATTACKSWING_CANT_ATTACK                 = 0x0016, // 4.3.4 15595
    SMSG_ATTACKERSTATEUPDATE                     = 0x05A5, // 5.3.0 17128
    SMSG_BATTLEFIELD_PORT_DENIED                 = 0x0F90, // 5.3.0 17128
    SMSG_RESUME_CAST_BAR                         = 0x114E,
    SMSG_CANCEL_COMBAT                           = 0x4F04, // 4.3.4 15595
    SMSG_SPELLBREAKLOG                           = 0x6B17, // 4.3.4 15595
    SMSG_SPELLHEALLOG                            = 0x2816, // 4.3.4 15595
    SMSG_SPELLENERGIZELOG                        = 0x0414, // 4.3.4 15595
    SMSG_BREAK_TARGET                            = 0x0105, // 4.3.4 15595
    SMSG_BINDPOINTUPDATE                         = 0x0E3B, // 5.4.8 18414
    SMSG_BINDZONEREPLY                           = 0x1158,
    SMSG_PLAYERBOUND                             = 0x12DD, // 5.3.0 17128
    SMSG_CLIENT_CONTROL_UPDATE                   = 0x1043, // 5.4.8 18414 (Wow.exe leaf; name reference-consensus)
    CMSG_REPOP_REQUEST                           = 0x134A, // 5.4.8 18414 (Wow.exe binary)
    SMSG_RESURRECT_REQUEST                       = 0x0FD0, // 5.3.0 17128
    CMSG_RESURRECT_RESPONSE                      = 0x0B0C, // 5.4.8 18414 (Wow.exe binary)
    CMSG_RETURN_TO_GRAVEYARD                     = 0x12EA, // 5.4.8 18414 (Wow.exe binary)
    CMSG_LOOT                                    = 0x1CE2, // 5.4.8 18414 (Wow.exe binary)
    CMSG_LOOT_CURRENCY                           = 0x781C, // 4.3.4 15595
    CMSG_LOOT_MONEY                              = 0x02F6, // 5.4.8 18414 (Wow.exe binary)
    CMSG_LOOT_RELEASE                            = 0x0840, // 5.4.8 18414 (Wow.exe binary)
    SMSG_LOOT_RESPONSE                           = 0x4C16, // 4.3.4 15595
    SMSG_LOOT_RELEASE_RESPONSE                   = 0x12C1, // 5.3.0 17128
    SMSG_LOOT_REMOVED                            = 0x0E8C, // 5.3.0 17128
    SMSG_LOOT_CURRENCY_REMOVED                   = 0x1DB4, // 4.3.4 15595
    SMSG_LOOT_MONEY_NOTIFY                       = 0x2836, // 4.3.4 15595
    SMSG_LOOT_ITEM_NOTIFY                        = 0x6D15, // 4.3.4 15595
    SMSG_LOOT_CLEAR_MONEY                        = 0x2B37, // 4.3.4 15595
    SMSG_ITEM_PUSH_RESULT                        = 0x0E15, // 4.3.4 15595
    SMSG_DUEL_REQUESTED                          = 0x4504, // 4.3.4 15595
    SMSG_DUEL_OUTOFBOUNDS                        = 0x0C26, // 4.3.4 15595
    SMSG_DUEL_INBOUNDS                           = 0x0A27, // 4.3.4 15595
    SMSG_DUEL_COMPLETE                           = 0x2527, // 4.3.4 15595
    SMSG_DUEL_WINNER                             = 0x2D36, // 4.3.4 15595
    CMSG_DUEL_ACCEPTED                           = 0x2136, // not in 5.4.8 (legacy; handler retained)
    CMSG_DUEL_CANCELLED                          = 0x6624, // not in 5.4.8 (legacy; handler retained)
    SMSG_MOUNTRESULT                             = 0x15D0, // 5.3.0 17128
    SMSG_DISMOUNTRESULT                          = 0x0CC5, // 5.3.0 17128
    SMSG_REMOVED_FROM_PVP_QUEUE                  = 0x1171,
    CMSG_MOUNTSPECIAL_ANIM                       = 0x0082, // 5.4.8 18414 (Wow.exe binary)
    SMSG_MOUNTSPECIAL_ANIM                       = 0x0217, // 4.3.4 15595
    SMSG_PET_TAME_FAILURE                        = 0x6B24, // 4.3.4 15595
    CMSG_PET_SET_ACTION                          = 0x12E9, // 5.4.8 18414 (Wow.exe binary)
    CMSG_PET_ACTION                              = 0x025B, // 5.4.8 18414 (Wow.exe binary)
    CMSG_PET_ABANDON                             = 0x07D0, // 5.4.8 18414 (Wow.exe binary)
    CMSG_PET_RENAME                              = 0x0A32, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PET_NAME_INVALID                        = 0x6007, // 4.3.4 15595
    SMSG_PET_SPELLS                              = 0x4114, // 4.3.4 15595
    SMSG_PET_MODE                                = 0x0085, // 5.3.0 17128
    CMSG_GOSSIP_HELLO                            = 0x12F3, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GOSSIP_SELECT_OPTION                    = 0x0748, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GOSSIP_MESSAGE                          = 0x13AD, // 5.3.0 17128
    SMSG_GOSSIP_COMPLETE                         = 0x0806, // 4.3.4 15595
    CMSG_NPC_TEXT_QUERY                          = 0x0287, // 5.4.8 18414 (Wow.exe binary)
    SMSG_NPC_TEXT_UPDATE                         = 0x4436, // 4.3.4 15595
    SMSG_NPC_WONT_TALK                           = 0x1182,
    CMSG_QUESTGIVER_STATUS_QUERY                 = 0x036A, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUESTGIVER_STATUS                       = 0x2115, // 4.3.4 15595
    CMSG_QUESTGIVER_HELLO                        = 0x02DB, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUESTGIVER_QUEST_LIST                   = 0x0134, // 4.3.4 15595
    CMSG_QUESTGIVER_QUERY_QUEST                  = 0x12F0, // 5.4.8 18414 (Wow.exe binary)
    CMSG_QUESTGIVER_QUEST_AUTOLAUNCH             = 0x1188, // not in 5.4.8 (legacy; handler retained)
    SMSG_QUESTGIVER_QUEST_DETAILS                = 0x2425, // 4.3.4 15595
    CMSG_QUESTGIVER_ACCEPT_QUEST                 = 0x06D1, // 5.4.8 18414 (Wow.exe binary)
    CMSG_QUESTGIVER_COMPLETE_QUEST               = 0x0659, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUESTGIVER_REQUEST_ITEMS                = 0x07F4, // 5.3.0 17128
    CMSG_QUESTGIVER_REQUEST_REWARD               = 0x0378, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUESTGIVER_OFFER_REWARD                 = 0x2427, // 4.3.4 15595
    CMSG_QUESTGIVER_CHOOSE_REWARD                = 0x07CB, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUESTGIVER_QUEST_INVALID                = 0x4016, // 4.3.4 15595
    CMSG_QUESTGIVER_CANCEL                       = 0x1191, // not in 5.4.8 (legacy; handler retained)
    SMSG_QUESTGIVER_QUEST_COMPLETE               = 0x55A4, // 4.3.4 15595
    SMSG_QUESTGIVER_QUEST_FAILED                 = 0x4236, // 4.3.4 15595
    CMSG_QUESTLOG_SWAP_QUEST                     = 0x1194, // not in 5.4.8 (legacy; handler retained)
    CMSG_QUESTLOG_REMOVE_QUEST                   = 0x0779, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUESTLOG_FULL                           = 0x0E36, // 4.3.4 15595
    SMSG_QUESTUPDATE_FAILED                      = 0x6324, // 4.3.4 15595
    SMSG_QUESTUPDATE_FAILEDTIMER                 = 0x6427, // 4.3.4 15595
    SMSG_QUESTUPDATE_COMPLETE                    = 0x2937, // 4.3.4 15595
    SMSG_QUESTUPDATE_ADD_KILL                    = 0x0D27, // 4.3.4 15595
    SMSG_QUESTUPDATE_ADD_ITEM_OBSOLETE           = 0x119B,
    CMSG_QUEST_CONFIRM_ACCEPT                    = 0x124B, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUEST_CONFIRM_ACCEPT                    = 0x6F07, // 4.3.4 15595
    CMSG_PUSHQUESTTOPARTY                        = 0x03D2, // 5.4.8 18414 (Wow.exe binary)
    CMSG_LIST_INVENTORY                          = 0x02D8, // 5.4.8 18414 (Wow.exe binary)
    SMSG_LIST_INVENTORY                          = 0x7CB0, // 4.3.4 15595
    CMSG_SELL_ITEM                               = 0x1358, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SELL_ITEM                               = 0x6105, // 4.3.4 15595
    CMSG_BUY_ITEM                                = 0x02E2, // 5.4.8 18414 (Wow.exe binary)
    SMSG_BUY_ITEM                                = 0x0F26, // 4.3.4 15595
    SMSG_BUY_FAILED                              = 0x6435, // 4.3.4 15595
    SMSG_SHOWTAXINODES                           = 0x2A36, // 4.3.4 15595
    CMSG_TAXINODE_STATUS_QUERY                   = 0x02E1, // 5.4.8 18414 (Wow.exe binary, via CMSG_TAXI_NODE_STATUS_QUERY)
    SMSG_TAXINODE_STATUS                         = 0x2936, // 4.3.4 15595
    CMSG_TAXIQUERYAVAILABLENODES                 = 0x02E3, // 5.4.8 18414 (Wow.exe binary, via CMSG_TAXI_QUERY_AVAILABLE_NODES)
    CMSG_ACTIVATETAXI                            = 0x03C9, // 5.4.8 18414 (Wow.exe binary, via CMSG_ACTIVATE_TAXI)
    SMSG_ACTIVATETAXIREPLY                       = 0x6A37, // 4.3.4 15595
    SMSG_NEW_TAXI_PATH                           = 0x4B35, // 4.3.4 15595
    CMSG_TRAINER_LIST                            = 0x034B, // 5.4.8 18414 (Wow.exe binary)
    SMSG_TRAINER_LIST                            = 0x4414, // 4.3.4 15595
    CMSG_TRAINER_BUY_SPELL                       = 0x0352, // 5.4.8 18414 (Wow.exe binary)
    SMSG_TRAINER_SERVICE                         = 0x6A05, // 4.3.4 15595
    SMSG_TRAINER_BUY_FAILED                      = 0x0004, // 4.3.4 15595
    CMSG_BINDER_ACTIVATE                         = 0x1248, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PLAYERBINDERROR                         = 0x6A24, // 4.3.4 15595
    CMSG_BANKER_ACTIVATE                         = 0x02E9, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SHOW_BANK                               = 0x2627, // 4.3.4 15595
    CMSG_BUY_BANK_SLOT                           = 0x12F2, // 5.4.8 18414 (Wow.exe binary)
    CMSG_PETITION_SHOWLIST                       = 0x037B, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PETITION_SHOWLIST                       = 0x6405, // 4.3.4 15595
    CMSG_PETITION_BUY                            = 0x12D9, // 5.4.8 18414 (Wow.exe binary)
    CMSG_PETITION_SHOW_SIGNATURES                = 0x136B, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PETITION_SHOW_SIGNATURES                = 0x0716, // 4.3.4 15595
    CMSG_PETITION_SIGN                           = 0x06DA, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PETITION_SIGN_RESULTS                   = 0x6217, // 4.3.4 15595
    MSG_PETITION_DECLINE                         = 0x4905, // 4.3.4 15595
    CMSG_OFFER_PETITION                          = 0x15BE, // 5.4.8 18414 (Wow.exe binary)
    CMSG_TURN_IN_PETITION                        = 0x0673, // 5.4.8 18414 (Wow.exe binary)
    SMSG_TURN_IN_PETITION_RESULTS                = 0x0F07, // 4.3.4 15595
    CMSG_PETITION_QUERY                          = 0x0255, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PETITION_QUERY_RESPONSE                 = 0x4B37, // 4.3.4 15595
    SMSG_FISH_NOT_HOOKED                         = 0x0A17, // 4.3.4 15595
    SMSG_FISH_ESCAPED                            = 0x2205, // 4.3.4 15595
    CMSG_BUG                                     = 0x09E1, // 5.4.8 18414 (Wow.exe binary)
    SMSG_NOTIFICATION                            = 0x14A0, // 4.3.4 15595
    CMSG_PLAYED_TIME                             = 0x03F6, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PLAYED_TIME                             = 0x11E2, // 5.4.8 18414
    CMSG_QUERY_TIME                              = 0x0640, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUERY_TIME_RESPONSE                     = 0x2124, // 4.3.4 15595
    SMSG_LOG_XPGAIN                              = 0x4514, // 4.3.4 15595
    SMSG_AURACASTLOG                             = 0x11D2,
    CMSG_RECLAIM_CORPSE                          = 0x03D3, // 5.4.8 18414 (Wow.exe binary)
    CMSG_WRAP_ITEM                               = 0x02DF, // 5.4.8 18414 (Wow.exe binary)
    SMSG_LEVELUP_INFO                            = 0x00CD, // 5.3.0 17128
    MSG_MINIMAP_PING                             = 0x6635, // 4.3.4 15595
    SMSG_RESISTLOG                               = 0x11D7,
    SMSG_ENCHANTMENTLOG                          = 0x6035, // 4.3.4 15595
    SMSG_START_MIRROR_TIMER                      = 0x6824, // 4.3.4 15595
    SMSG_PAUSE_MIRROR_TIMER                      = 0x4015, // 4.3.4 15595
    SMSG_STOP_MIRROR_TIMER                       = 0x0B06, // 4.3.4 15595
    CMSG_PING                                    = 0x0012, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PONG                                    = 0x1969, // 5.4.8 18414 (SkyFire)
    SMSG_CLEAR_COOLDOWNS                         = 0x59B4, // 4.3.4 15595
    SMSG_GAMEOBJECT_PAGETEXT                     = 0x2925, // 4.3.4 15595
    CMSG_SETSHEATHED                             = 0x0249, // 5.4.8 18414 (Wow.exe binary)
    SMSG_COOLDOWN_CHEAT                          = 0x4537, // 4.3.4 15595
    SMSG_SPELL_DELAYED                           = 0x0715, // 4.3.4 15595
    CMSG_QUEST_POI_QUERY                         = 0x10C2, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUEST_POI_QUERY_RESPONSE                = 0x6304, // 4.3.4 15595
    SMSG_INVALID_PROMOTION_CODE                  = 0x6F25, // 4.3.4 15595
    MSG_GM_BIND_OTHER                            = 0x11E9,
    MSG_GM_SUMMON                                = 0x11EA,
    SMSG_ITEM_TIME_UPDATE                        = 0x0CC0, // 5.3.0 17128
    SMSG_ITEM_ENCHANT_TIME_UPDATE                = 0x0799, // 5.3.0 17128
    MSG_GM_SHOWLABEL                             = 0x11F0,
    CMSG_PET_CAST_SPELL                          = 0x044D, // 5.4.8 18414 (Wow.exe binary)
    MSG_SAVE_GUILD_EMBLEM                        = 0x2404, // 4.3.4 15595
    MSG_TABARDVENDOR_ACTIVATE                    = 0x6926, // 4.3.4 15595
    SMSG_PLAY_SPELL_VISUAL                       = 0x07D0, // 5.3.0 17128
    CMSG_ZONEUPDATE                              = 0x0265, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PARTYKILLLOG                            = 0x4937, // 4.3.4 15595
    SMSG_COMPRESSED_UPDATE_OBJECT                = 0x11F7,
    SMSG_EXPLORATION_EXPERIENCE                  = 0x0D84, // 5.3.0 17128
    MSG_RANDOM_ROLL                              = 0x0905, // 4.3.4 15595
    SMSG_ENVIRONMENTALDAMAGELOG                  = 0x6C05, // 4.3.4 15595
    CMSG_CHANGEPLAYER_DIFFICULTY                 = 0x1F8E, // 5.4.8 18414
    SMSG_RWHOIS                                  = 0x11FF,
    SMSG_LFG_PLAYER_REWARD                       = 0x0E90, // 5.3.0 17128
    SMSG_LFG_TELEPORT_DENIED                     = 0x049D, // 5.3.0 17128
    CMSG_UNLEARN_SKILL                           = 0x0268, // 5.4.8 18414 (Wow.exe binary)
    SMSG_REMOVED_SPELL                           = 0x4804, // 4.3.4 15595
    CMSG_GMTICKET_CREATE                         = 0x1A86, // 5.4.8 18414 (Wow.exe binary, via CMSG_GM_TICKET_CREATE)
    SMSG_GMTICKET_CREATE                         = 0x2107, // 4.3.4 15595
    CMSG_GMTICKET_UPDATETEXT                     = 0x0A26, // 5.4.8 18414 (Wow.exe binary, via CMSG_GM_TICKET_UPDATE_TEXT)
    SMSG_GMTICKET_UPDATETEXT                     = 0x6535, // 4.3.4 15595
    SMSG_ACCOUNT_DATA_TIMES                      = 0x162B, // 5.4.8 18414
    CMSG_REQUEST_ACCOUNT_DATA                    = 0x1D8A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_UPDATE_ACCOUNT_DATA                     = 0x0068, // 5.4.8 18414 (Wow.exe binary)
    SMSG_UPDATE_ACCOUNT_DATA                     = 0x0AAE, // 5.4.8 18414
    SMSG_CLEAR_FAR_SIGHT_IMMEDIATE               = 0x2A04, // 4.3.4 15595
    SMSG_CHANGEPLAYER_DIFFICULTY_RESULT          = 0x2217, // 4.3.4 15595
    CMSG_GMTICKET_GETTICKET                      = 0x1F89, // 5.4.8 18414 (Wow.exe binary, via CMSG_GM_TICKET_GET_TICKET)
    SMSG_GMTICKET_GETTICKET                      = 0x2C15, // 4.3.4 15595
    SMSG_INSTANCE_ENCOUNTER                      = 0x1215,
    SMSG_GAMEOBJECT_DESPAWN_ANIM                 = 0x0981, // 5.3.0 17128
    MSG_CORPSE_QUERY                             = 0x4336, // 4.3.4 15595
    CMSG_GMTICKET_DELETETICKET                   = 0x1A23, // 5.4.8 18414 (Wow.exe binary, via CMSG_GM_TICKET_DELETE_TICKET)
    SMSG_GMTICKET_DELETETICKET                   = 0x6D17, // 4.3.4 15595
    SMSG_CHAT_WRONG_FACTION                      = 0x6724, // 4.3.4 15595
    CMSG_GMTICKET_SYSTEMSTATUS                   = 0x0A82, // 5.4.8 18414 (Wow.exe binary, via CMSG_GM_TICKET_SYSTEM_STATUS)
    SMSG_GMTICKET_SYSTEMSTATUS                   = 0x0D35, // 4.3.4 15595
    CMSG_SPIRIT_HEALER_ACTIVATE                  = 0x0340, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUEST_FORCE_REMOVED                     = 0x121F,
    SMSG_SPIRIT_HEALER_CONFIRM                   = 0x4917, // 4.3.4 15595
    SMSG_GOSSIP_POI                              = 0x15A7, // 5.3.0 17128
    CMSG_CHAT_IGNORED                            = 0x048A, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GM_PLAYER_INFO                          = 0x1231,
    CMSG_GUILD_RANK                              = 0x1024, // 4.3.4 15595
    CMSG_GUILD_ADD_RANK                          = 0x047A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_DEL_RANK                          = 0x0D79, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_SET_NOTE                          = 0x05DA, // 5.4.8 18414 (Wow.exe binary)
    SMSG_LOGIN_VERIFY_WORLD                      = 0x1C0F, // 5.4.8 18414
    CMSG_SEND_MAIL                               = 0x1DBA, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SEND_MAIL_RESULT                        = 0x4927, // 4.3.4 15595
    CMSG_GET_MAIL_LIST                           = 0x077A, // 5.4.8 18414 (Wow.exe binary)
    SMSG_MAIL_LIST_RESULT                        = 0x4217, // 4.3.4 15595
    CMSG_BATTLEFIELD_LIST                        = 0x1C41, // 5.4.8 18414 (Wow.exe binary)
    SMSG_BATTLEFIELD_LIST                        = 0x0ACC, // 5.3.0 17128
    SMSG_FORCE_SET_VEHICLE_REC_ID                = 0x1240,
    CMSG_SET_VEHICLE_REC_ID_ACK                  = 0x185B, // 5.4.8 18414 (Wow.exe binary)
    CMSG_ITEM_TEXT_QUERY                         = 0x0123, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ITEM_TEXT_QUERY_RESPONSE                = 0x1134, // 5.4.8 18414
    CMSG_MAIL_TAKE_MONEY                         = 0x06FA, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MAIL_TAKE_ITEM                          = 0x1371, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MAIL_MARK_AS_READ                       = 0x0241, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MAIL_RETURN_TO_SENDER                   = 0x1FA8, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MAIL_DELETE                             = 0x14E2, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MAIL_CREATE_TEXT_ITEM                   = 0x1270, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SPELLLOGMISS                            = 0x0625, // 4.3.4 15595
    SMSG_SPELLLOGEXECUTE                         = 0x0626, // 4.3.4 15595
    SMSG_DEBUGAURAPROC                           = 0x124E,
    SMSG_PERIODICAURALOG                         = 0x0416, // 4.3.4 15595
    SMSG_SPELLDAMAGESHIELD                       = 0x2927, // 4.3.4 15595
    SMSG_SPELLNONMELEEDAMAGELOG                  = 0x4315, // 4.3.4 15595
    CMSG_LEARN_TALENT                            = 0x02A7, // 5.4.8 18414 (Wow.exe binary)
    SMSG_RESURRECT_FAILED                        = 0x1253,
    CMSG_TOGGLE_PVP                              = 0x0644, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ZONE_UNDER_ATTACK                       = 0x053F, // 5.3.0 17128
    MSG_AUCTION_HELLO                            = 0x2307, // 4.3.4 15595
    CMSG_AUCTION_SELL_ITEM                       = 0x02EB, // 5.4.8 18414 (Wow.exe binary)
    CMSG_AUCTION_REMOVE_ITEM                     = 0x0259, // 5.4.8 18414 (Wow.exe binary)
    CMSG_AUCTION_LIST_ITEMS                      = 0x02EA, // 5.4.8 18414 (Wow.exe binary)
    CMSG_AUCTION_LIST_OWNER_ITEMS                = 0x0361, // 5.4.8 18414 (Wow.exe binary)
    CMSG_AUCTION_PLACE_BID                       = 0x03C8, // 5.4.8 18414 (Wow.exe binary)
    SMSG_AUCTION_COMMAND_RESULT                  = 0x4C25, // 4.3.4 15595
    SMSG_AUCTION_LIST_RESULT                     = 0x6637, // 4.3.4 15595
    SMSG_AUCTION_OWNER_LIST_RESULT               = 0x6C34, // 4.3.4 15595
    SMSG_AUCTION_BIDDER_NOTIFICATION             = 0x4E27, // 4.3.4 15595
    SMSG_AUCTION_OWNER_NOTIFICATION              = 0x4116, // 4.3.4 15595
    SMSG_PROCRESIST                              = 0x1261,
    SMSG_COMBAT_EVENT_FAILED                     = 0x1262,
    SMSG_DISPEL_FAILED                           = 0x0307, // 4.3.4 15595
    SMSG_SPELLOGDAMAGE_IMMUNE                    = 0x4507, // 4.3.4 15595
    CMSG_AUCTION_LIST_BIDDER_ITEMS               = 0x12D0, // 5.4.8 18414 (Wow.exe binary)
    SMSG_AUCTION_BIDDER_LIST_RESULT              = 0x0027, // 4.3.4 15595
    SMSG_SET_FLAT_SPELL_MODIFIER                 = 0x2834, // 4.3.4 15595
    SMSG_SET_PCT_SPELL_MODIFIER                  = 0x0224, // 4.3.4 15595
    CMSG_SET_AMMO                                = 0x1269, // not in 5.4.8 (legacy; handler retained)
    SMSG_CORPSE_RECLAIM_DELAY                    = 0x13C9, // 5.3.0 17128
    CMSG_SET_ACTIVE_MOVER                        = 0x09F0, // 5.4.8 18414 (Wow.exe binary)
    CMSG_PET_CANCEL_AURA                         = 0x12DA, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CANCEL_AUTO_REPEAT_SPELL                = 0x1272, // 5.4.8 18414 (Wow.exe binary)
    MSG_GM_ACCOUNT_ONLINE                        = 0x0889, // 5.3.0 17128
    MSG_LIST_STABLED_PETS                        = 0x0834, // 4.3.4 15595
    CMSG_STABLE_PET                              = 0x1271, // not in 5.4.8 (legacy; handler retained)
    CMSG_UNSTABLE_PET                            = 0x1272, // not in 5.4.8 (legacy; handler retained)
    CMSG_BUY_STABLE_SLOT                         = 0x1273, // not in 5.4.8 (legacy; handler retained)
    SMSG_STABLE_RESULT                           = 0x118D, // 5.3.0 17128
    CMSG_STABLE_REVIVE_PET                       = 0x1275, // not in 5.4.8 (legacy; handler retained)
    CMSG_STABLE_SWAP_PET                         = 0x1276, // not in 5.4.8 (legacy; handler retained)
    MSG_QUEST_PUSH_RESULT                        = 0x4515, // 4.3.4 15595
    SMSG_PLAY_MUSIC                              = 0x4B06, // 4.3.4 15595
    SMSG_PLAY_OBJECT_SOUND                       = 0x2635, // 4.3.4 15595
    SMSG_PLAY_ONE_SHOT_ANIM_KIT                  = 0x4A35, // 4.3.4 15595
    CMSG_REQUEST_PET_INFO                        = 0x135B, // 5.4.8 18414 (Wow.exe binary)
    CMSG_FAR_SIGHT                               = 0x1341, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SPELLDISPELLOG                          = 0x4516, // 4.3.4 15595
    SMSG_DAMAGE_CALC_LOG                         = 0x2436, // 4.3.4 15595
    CMSG_GROUP_CHANGE_SUB_GROUP                  = 0x1799, // 5.4.8 18414 (Wow.exe binary)
    CMSG_REQUEST_PARTY_MEMBER_STATS              = 0x0806, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GROUP_SWAP_SUB_GROUP                    = 0x1281,
    CMSG_RESET_FACTION_CHEAT                     = 0x10B6, // 5.4.8 18414 (Wow.exe binary)
    CMSG_AUTOSTORE_BANK_ITEM                     = 0x02CF, // 5.4.8 18414 (Wow.exe binary)
    CMSG_AUTOBANK_ITEM                           = 0x066D, // 5.4.8 18414 (Wow.exe binary)
    MSG_QUERY_NEXT_MAIL_TIME                     = 0x0F04, // 4.3.4 15595
    SMSG_RECEIVED_MAIL                           = 0x2924, // 4.3.4 15595
    SMSG_RAID_GROUP_ONLY                         = 0x0D82, // 5.4.8 18414
    CMSG_SET_PVP_RANK_CHEAT                      = 0x1289,
    CMSG_SET_PVP_TITLE                           = 0x128C,
    SMSG_PVP_CREDIT                              = 0x6015, // 4.3.4 15595
    SMSG_AUCTION_REMOVED_NOTIFICATION            = 0x2334, // 4.3.4 15595
    CMSG_GROUP_RAID_CONVERT                      = 0x032C, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GROUP_REQUEST_JOIN_UPDATES              = 0x178A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GROUP_ASSISTANT_LEADER                  = 0x1897, // 5.4.8 18414 (Wow.exe binary)
    CMSG_BUYBACK_ITEM                            = 0x0661, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SERVER_MESSAGE                          = 0x6C04, // 4.3.4 15595
    CMSG_SET_SAVED_INSTANCE_EXTEND               = 0x0C68, // 5.4.8 18414 (Wow.exe binary)
    SMSG_LFG_OFFER_CONTINUE                      = 0x12C4, // 5.3.0 17128
    SMSG_TEST_DROP_RATE_RESULT                   = 0x1296,
    CMSG_LFG_GET_STATUS                          = 0x032D, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SHOW_MAILBOX                            = 0x2524, // 4.3.4 15595
    SMSG_RESET_RANGED_COMBAT_TIMER               = 0x1299,
    SMSG_CHAT_NOT_IN_PARTY                       = 0x6A14, // 4.3.4 15595
    CMSG_CANCEL_GROWTH_AURA                      = 0x0237, // 4.3.4 15595
    SMSG_CANCEL_AUTO_REPEAT                      = 0x6436, // 4.3.4 15595
    SMSG_STANDSTATE_UPDATE                       = 0x6F04, // 4.3.4 15595
    SMSG_LOOT_ALL_PASSED                         = 0x0DC1, // 5.3.0 17128
    SMSG_LOOT_ROLL_WON                           = 0x0985, // 5.3.0 17128
    CMSG_LOOT_ROLL                               = 0x15C2, // 5.4.8 18414 (Wow.exe binary)
    SMSG_LOOT_START_ROLL                         = 0x10C8, // 5.3.0 17128
    SMSG_LOOT_ROLL                               = 0x158C, // 5.3.0 17128
    CMSG_LOOT_MASTER_GIVE                        = 0x1DE1, // 5.4.8 18414 (Wow.exe binary)
    SMSG_LOOT_MASTER_LIST                        = 0x0325, // 4.3.4 15595
    SMSG_SET_FORCED_REACTIONS                    = 0x069D, // 5.3.0 17128
    SMSG_SPELL_FAILED_OTHER                      = 0x4535, // 4.3.4 15595
    SMSG_GAMEOBJECT_RESET_STATE                  = 0x2A16, // 4.3.4 15595
    CMSG_REPAIR_ITEM                             = 0x02C1, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CHAT_PLAYER_NOT_FOUND                   = 0x2526, // 4.3.4 15595
    MSG_TALENT_WIPE_CONFIRM                      = 0x0107, // 4.3.4 15595
    SMSG_SUMMON_REQUEST                          = 0x079C, // 5.3.0 17128
    CMSG_SUMMON_RESPONSE                         = 0x0A33, // 5.4.8 18414 (Wow.exe binary)
    MSG_DEV_SHOWLABEL                            = 0x12AE,
    SMSG_MONSTER_MOVE_TRANSPORT                  = 0x2004, // 4.3.4 15595
    SMSG_PET_BROKEN                              = 0x12B0,
    MSG_MOVE_FEATHER_FALL                        = 0x12B1,
    MSG_MOVE_WATER_WALK                          = 0x12B2,
    CMSG_SELF_RES                                = 0x0360, // 5.4.8 18414 (Wow.exe binary)
    SMSG_FEIGN_DEATH_RESISTED                    = 0x12B5,
    SMSG_SCRIPT_MESSAGE                          = 0x12B7,
    SMSG_DUEL_COUNTDOWN                          = 0x4836, // 4.3.4 15595
    SMSG_AREA_TRIGGER_MESSAGE                    = 0x4505, // 4.3.4 15595
    CMSG_SHOWING_HELM                            = 0x126B, // 5.4.8 18414 (Wow.exe binary)
    CMSG_SHOWING_CLOAK                           = 0x02F2, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ROLE_CHOSEN                             = 0x12BC,
    SMSG_PLAYER_SKINNED                          = 0x0116, // 4.3.4 15595
    SMSG_DURABILITY_DAMAGE_DEATH                 = 0x4C27, // 4.3.4 15595
    CMSG_SET_ACTIONBAR_TOGGLES                   = 0x0672, // 5.4.8 18414 (Wow.exe binary)
    MSG_PETITION_RENAME                          = 0x4005, // 4.3.4 15595
    SMSG_INIT_WORLD_STATES                       = 0x1560, // 5.4.8 18414
    SMSG_UPDATE_WORLD_STATE                      = 0x4816, // 4.3.4 15595
    SMSG_PET_ACTION_FEEDBACK                     = 0x08C4, // 5.3.0 17128
    CMSG_CHAR_RENAME                             = 0x0963, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CHAR_RENAME                             = 0x2024, // 4.3.4 15595
    CMSG_MOVE_SPLINE_DONE                        = 0x11D9, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MOVE_FALL_RESET                         = 0x00D9, // 5.4.8 18414 (Wow.exe binary)
    SMSG_INSTANCE_SAVE_CREATED                   = 0x0124, // 4.3.4 15595
    SMSG_RAID_INSTANCE_INFO                      = 0x6626, // 4.3.4 15595
    CMSG_REQUEST_RAID_INFO                       = 0x0A87, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MOVE_TIME_SKIPPED                       = 0x0150, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MOVE_FEATHER_FALL_ACK                   = 0x08D0, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MOVE_WATER_WALK_ACK                     = 0x10F2, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MOVE_NOT_ACTIVE_MOVER                   = 0x7A1A, // 4.3.4 15595
    SMSG_PLAY_SOUND                              = 0x2134, // 4.3.4 15595
    CMSG_BATTLEFIELD_STATUS                      = 0x1F9E, // 5.4.8 18414 (Wow.exe binary)
    SMSG_BATTLEFIELD_STATUS                      = 0x7DA1, // 4.3.4 15595
    SMSG_BATTLEFIELD_STATUS_ACTIVE               = 0x0CD8, // 5.3.0 17128
    SMSG_BATTLEFIELD_STATUS_FAILED               = 0x0798, // 5.3.0 17128
    SMSG_BATTLEFIELD_STATUS_QUEUED               = 0x11D1, // 5.3.0 17128
    SMSG_BATTLEFIELD_STATUS_NEEDCONFIRMATION     = 0x09D5, // 5.3.0 17128
    SMSG_BATTLEFIELD_STATUS_WAITFORGROUPS        = 0x0BDC, // 5.3.0 17128
    CMSG_BATTLEFIELD_PORT                        = 0x1379, // 5.4.8 18414 (Wow.exe binary)
    CMSG_INSPECT_HONOR_STATS                     = 0x19C3, // 5.4.8 18414 (Wow.exe binary)
    SMSG_INSPECT_HONOR_STATS                     = 0x109C, // 5.3.0 17128
    CMSG_BATTLEMASTER_HELLO                      = 0x0234, // not in 5.4.8 (legacy; handler retained)
    SMSG_FORCE_WALK_SPEED_CHANGE                 = 0x12DB,
    CMSG_FORCE_WALK_SPEED_CHANGE_ACK             = 0x00DB, // 5.4.8 18414 (Wow.exe binary, via CMSG_MOVE_FORCE_WALK_SPEED_CHANGE_ACK)
    SMSG_FORCE_SWIM_BACK_SPEED_CHANGE            = 0x12DD,
    CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK        = 0x10D1, // 5.4.8 18414 (Wow.exe binary, via CMSG_MOVE_FORCE_SWIM_BACK_SPEED_CHANGE_ACK)
    SMSG_FORCE_TURN_RATE_CHANGE                  = 0x12DF,
    CMSG_FORCE_TURN_RATE_CHANGE_ACK              = 0x185A, // 5.4.8 18414 (Wow.exe binary, via CMSG_MOVE_FORCE_TURN_RATE_CHANGE_ACK)
    CMSG_PVP_LOG_DATA                            = 0x14C2, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PVP_LOG_DATA                            = 0x0795, // 5.3.0 17128
    CMSG_LEAVE_BATTLEFIELD                       = 0x0257, // 5.4.8 18414 (Wow.exe binary, via CMSG_BATTLEFIELD_LEAVE)
    CMSG_AREA_SPIRIT_HEALER_QUERY                = 0x03F1, // 5.4.8 18414 (Wow.exe binary)
    CMSG_AREA_SPIRIT_HEALER_QUEUE                = 0x12D8, // 5.4.8 18414 (Wow.exe binary)
    SMSG_AREA_SPIRIT_HEALER_TIME                 = 0x0734, // 4.3.4 15595
    SMSG_WARDEN_DATA                             = 0x12D9, // 5.3.0 17128
    CMSG_WARDEN_DATA                             = 0x1816, // 5.4.8 18414 (Wow.exe binary)
    CMSG_BATTLEGROUND_PLAYER_POSITIONS           = 0x3902, // 4.3.4 15595
    SMSG_BATTLEGROUND_PLAYER_POSITIONS           = 0x03CC, // 5.3.0
    CMSG_PET_STOP_ATTACK                         = 0x065B, // 5.4.8 18414 (Wow.exe binary)
    SMSG_BINDER_CONFIRM                          = 0x2835, // 4.3.4 15595
    SMSG_BATTLEGROUND_PLAYER_JOINED              = 0x1281, // 5.3.0 17128
    SMSG_BATTLEGROUND_PLAYER_LEFT                = 0x1581, // 5.3.0 17128
    CMSG_BATTLEMASTER_JOIN                       = 0x0769, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ADDON_INFO                              = 0x160A, // 5.4.8 18414 (SkyFire)
    CMSG_PET_UNLEARN                             = 0x12F1, // not in 5.4.8 (legacy; handler retained)
    SMSG_PET_UNLEARN_CONFIRM                     = 0x12F2,
    SMSG_PARTY_MEMBER_STATS_FULL                 = 0x0215, // 4.3.4 15595
    CMSG_PET_SPELL_AUTOCAST                      = 0x06F0, // 5.4.8 18414 (Wow.exe binary) [was 0x12E9 alias of PET_SET_ACTION]
    SMSG_WEATHER                                 = 0x1391, // 5.3.0 17128
    SMSG_PLAY_TIME_WARNING                       = 0x12F6,
    SMSG_MINIGAME_SETUP                          = 0x12F7,
    SMSG_MINIGAME_STATE                          = 0x12F8,
    CMSG_MINIGAME_MOVE                           = 0x12F9,
    SMSG_MINIGAME_MOVE_FAILED                    = 0x12FA,
    SMSG_RAID_INSTANCE_MESSAGE                   = 0x6E15, // 4.3.4 15595
    SMSG_COMPRESSED_MOVES                        = 0x0517, // 4.3.4 15595
    CMSG_GUILD_INFO_TEXT                         = 0x0C70, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CHAT_RESTRICTED                         = 0x6536, // 4.3.4 15595
    SMSG_SPLINE_SET_RUN_SPEED                    = 0x12FF,
    SMSG_SPLINE_SET_RUN_BACK_SPEED               = 0x1300,
    SMSG_SPLINE_SET_SWIM_SPEED                   = 0x1301,
    SMSG_SPLINE_SET_WALK_SPEED                   = 0x1302,
    SMSG_SPLINE_SET_SWIM_BACK_SPEED              = 0x1303,
    SMSG_SPLINE_SET_TURN_RATE                    = 0x1304,
    SMSG_SPLINE_MOVE_UNROOT                      = 0x75B6, // 4.3.4 15595
    SMSG_SPLINE_MOVE_FEATHER_FALL                = 0x3DA5, // 4.3.4 15595
    SMSG_SPLINE_MOVE_NORMAL_FALL                 = 0x38B2, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_HOVER                   = 0x14B6, // 4.3.4 15595
    SMSG_SPLINE_MOVE_UNSET_HOVER                 = 0x7DA5, // 4.3.4 15595
    SMSG_SPLINE_MOVE_WATER_WALK                  = 0x50A2, // 4.3.4 15595
    SMSG_SPLINE_MOVE_LAND_WALK                   = 0x3DA7, // 4.3.4 15595
    SMSG_SPLINE_MOVE_START_SWIM                  = 0x31A5, // 4.3.4 15595
    SMSG_SPLINE_MOVE_STOP_SWIM                   = 0x1DA2, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_RUN_MODE                = 0x75A7, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_WALK_MODE               = 0x54B6, // 4.3.4 15595
    MSG_GM_DESTROY_CORPSE                        = 0x1311,
    CMSG_ACTIVATETAXIEXPRESS                     = 0x06FB, // 5.4.8 18414 (Wow.exe binary, via CMSG_ACTIVATE_TAXI_EXPRESS)
    SMSG_SET_FACTION_ATWAR                       = 0x1314,
    SMSG_SET_FACTION_NOT_VISIBLE                 = 0x0000, //TODO: Needs fixing up
    SMSG_GAMETIMEBIAS_SET                        = 0x1315,
    CMSG_SET_FACTION_INACTIVE                    = 0x0778, // 5.4.8 18414 (Wow.exe binary)
    CMSG_SET_WATCHED_FACTION                     = 0x06C9, // 5.4.8 18414 (Wow.exe binary)
    MSG_MOVE_TIME_SKIPPED                        = 0x7A0A, // 4.3.4 15595
    SMSG_SPLINE_MOVE_ROOT                        = 0x51B4, // 4.3.4 15595
    SMSG_INVALIDATE_PLAYER                       = 0x0ED5, // 5.3.0 17128
    CMSG_RESET_INSTANCES                         = 0x0C69, // 5.4.8 18414 (Wow.exe binary)
    SMSG_INSTANCE_RESET                          = 0x6F05, // 4.3.4 15595
    SMSG_INSTANCE_RESET_FAILED                   = 0x4725, // 4.3.4 15595
    SMSG_UPDATE_LAST_INSTANCE                    = 0x0437, // 4.3.4 15595
    MSG_RAID_TARGET_UPDATE                       = 0x2C36, // 4.3.4 15595
    MSG_RAID_READY_CHECK                         = 0x0DDC, // 5.3.0 17128
    SMSG_PET_ACTION_SOUND                        = 0x13D1, // 5.3.0 17128
    SMSG_PET_DISMISS_SOUND                       = 0x0ED1, // 5.3.0 17128
    SMSG_GHOSTEE_GONE                            = 0x1327,
    CMSG_GM_UPDATE_TICKET_STATUS                 = 0x15A8, // 5.4.8 18414 (Wow.exe binary, via CMSG_GM_TICKET_CASE_STATUS)
    SMSG_GM_TICKET_STATUS_UPDATE                 = 0x2C25, // 4.3.4 15595
    MSG_SET_DUNGEON_DIFFICULTY                   = 0x4925, // 4.3.4 15595
    CMSG_GMSURVEY_SUBMIT                         = 0x073C, // 5.4.8 18414 (Wow.exe binary, via CMSG_GM_SURVEY_SUBMIT)
    SMSG_UPDATE_INSTANCE_OWNERSHIP               = 0x4915, // 4.3.4 15595
    SMSG_CHAT_PLAYER_AMBIGUOUS                   = 0x2F34, // 4.3.4 15595
    MSG_DELAY_GHOST_TELEPORT                     = 0x132F,
    SMSG_SPELLINSTAKILLLOG                       = 0x6216, // 4.3.4 15595
    SMSG_SPELL_UPDATE_CHAIN_TARGETS              = 0x6006, // 4.3.4 15595
    CMSG_CHAT_FILTERED                           = 0x0946, // 4.3.4 15595
    SMSG_EXPECTED_SPAM_RECORDS                   = 0x4D36, // 4.3.4 15595
    SMSG_SPELLSTEALLOG                           = 0x4E26, // 4.3.4 15595
    SMSG_LOTTERY_QUERY_RESULT_OBSOLETE           = 0x1336,
    SMSG_LOTTERY_RESULT_OBSOLETE                 = 0x1338,
    SMSG_CHARACTER_PROFILE                       = 0x1339,
    SMSG_CHARACTER_PROFILE_REALM_CONNECTED       = 0x133A,
    SMSG_DEFENSE_MESSAGE                         = 0x0314, // 4.3.4 15595
    SMSG_WORLD_SERVER_INFO                       = 0x0427, // 5.4.1 17538
    MSG_GM_RESETINSTANCELIMIT                    = 0x133D,
    SMSG_MOTD                                    = 0x183B, // 5.4.8 18414
    SMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY = 0x133F,
    SMSG_MOVE_UNSET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY = 0x1340,
    CMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY_ACK = 0x11DB, // 5.4.8 18414 (Wow.exe binary)
    MSG_MOVE_START_SWIM_CHEAT                    = 0x1342,
    MSG_MOVE_STOP_SWIM_CHEAT                     = 0x1343,
    SMSG_MOVE_SET_CAN_FLY                        = 0x0F48, // 5.3.0 17128
    SMSG_MOVE_UNSET_CAN_FLY                      = 0x034D, // 5.3.0 17128
    CMSG_MOVE_SET_CAN_FLY                        = 0x720E, // 4.3.4 15595
    CMSG_MOVE_SET_CAN_FLY_ACK                    = 0x1052, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MOVE_SET_FLY                            = 0x01F1, // 5.4.8 18414 (Wow.exe binary)
    CMSG_SOCKET_GEMS                             = 0x02CB, // 5.4.8 18414 (Wow.exe binary)
    CMSG_ARENA_TEAM_CREATE                       = 0x04A1, // not in 5.4.8 (legacy; handler retained)
    SMSG_ARENA_TEAM_COMMAND_RESULT               = 0x06C1, // 5.3.0 17128
    MSG_MOVE_UPDATE_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY = 0x134B,
    CMSG_ARENA_TEAM_QUERY                        = 0x0514, // not in 5.4.8 (legacy; handler retained)
    SMSG_ARENA_TEAM_QUERY_RESPONSE               = 0x6336, // 4.3.4 15595
    CMSG_ARENA_TEAM_ROSTER                       = 0x6F37, // not in 5.4.8 (legacy; handler retained)
    SMSG_ARENA_TEAM_ROSTER                       = 0x14C0, // 5.3.0 17128
    CMSG_ARENA_TEAM_INVITE                       = 0x2F27, // not in 5.4.8 (legacy; handler retained)
    SMSG_ARENA_TEAM_INVITE                       = 0x0F36, // 4.3.4 15595
    CMSG_ARENA_TEAM_ACCEPT                       = 0x2A25, // not in 5.4.8 (legacy; handler retained)
    CMSG_ARENA_TEAM_DECLINE                      = 0x6925, // not in 5.4.8 (legacy; handler retained)
    CMSG_ARENA_TEAM_LEAVE                        = 0x0E16, // not in 5.4.8 (legacy; handler retained)
    CMSG_ARENA_TEAM_REMOVE                       = 0x2F05, // not in 5.4.8 (legacy; handler retained)
    CMSG_ARENA_TEAM_DISBAND                      = 0x6504, // not in 5.4.8 (legacy; handler retained)
    CMSG_ARENA_TEAM_LEADER                       = 0x4204, // not in 5.4.8 (legacy; handler retained)
    SMSG_ARENA_TEAM_EVENT                        = 0x0617, // 4.3.4 15595
    CMSG_BATTLEMASTER_JOIN_ARENA                 = 0x02D2, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MOVE_START_ASCEND                       = 0x11FA, // 5.4.8 18414
    CMSG_MOVE_STOP_ASCEND                        = 0x115A, // 5.4.8 18414
    SMSG_ARENA_TEAM_STATS                        = 0x4425, // 4.3.4 15595
    CMSG_LFG_JOIN                                = 0x046B, // 5.4.8 18414 (Wow.exe binary)
    CMSG_LFG_LEAVE                               = 0x01E0, // 5.4.8 18414 (Wow.exe binary)
    CMSG_LFG_SEARCH_JOIN                         = 0x135F,
    CMSG_LFG_SEARCH_LEAVE                        = 0x1360,
    SMSG_LFG_SEARCH_RESULTS                      = 0x1361,
    SMSG_LFG_PROPOSAL_UPDATE                     = 0x1362,
    CMSG_LFG_PROPOSAL_RESPONSE                   = 0x1D9D, // 5.4.8 18414 (Wow.exe binary, via CMSG_LFG_PROPOSAL_RESULT)
    SMSG_LFG_ROLE_CHECK_UPDATE                   = 0x1364,
    SMSG_LFG_JOIN_RESULT                         = 0x0291, // 5.3.0 17128
    SMSG_LFG_QUEUE_STATUS                        = 0x1366,
    CMSG_SET_LFG_COMMENT                         = 0x0530, // 4.3.4 15595
    SMSG_LFG_UPDATE_PLAYER                       = 0x1368,
    SMSG_LFG_UPDATE_PARTY                        = 0x1369,
    SMSG_LFG_UPDATE_SEARCH                       = 0x136A,
    CMSG_LFG_SET_ROLES                           = 0x08A2, // 5.4.8 18414 (Wow.exe binary)
    CMSG_LFG_BOOT_PLAYER_VOTE                    = 0x17BE, // 5.4.8 18414 (Wow.exe binary, via CMSG_LFG_SET_BOOT_VOTE)
    SMSG_LFG_BOOT_PLAYER                         = 0x136E,
    CMSG_LFG_GET_PLAYER_INFO                     = 0x136F,
    SMSG_LFG_PLAYER_INFO                         = 0x1370,
    CMSG_LFG_TELEPORT                            = 0x1AA6, // 5.4.8 18414 (Wow.exe binary)
    CMSG_LFG_GET_PARTY_INFO                      = 0x1372,
    SMSG_LFG_PARTY_INFO                          = 0x1373,
    SMSG_TITLE_EARNED                            = 0x02D0, // 5.3.0 17128
    CMSG_SET_TITLE                               = 0x03C7, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CANCEL_MOUNT_AURA                       = 0x1552, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ARENA_ERROR                             = 0x04BA, // 5.4.8 18414
    MSG_INSPECT_ARENA_TEAMS                      = 0x2704, // 4.3.4 15595
    SMSG_DEATH_RELEASE_LOC                       = 0x2F07, // 4.3.4 15595
    CMSG_CANCEL_TEMP_ENCHANTMENT                 = 0x024B, // 5.4.8 18414 (Wow.exe binary)
    SMSG_FORCED_DEATH_UPDATE                     = 0x2606, // 4.3.4 15595
    MSG_MOVE_SET_FLIGHT_SPEED_CHEAT              = 0x137E,
    SMSG_MOVE_SET_FLIGHT_SPEED                   = 0x0E54, // 5.3.0 17128
    MSG_MOVE_SET_FLIGHT_BACK_SPEED_CHEAT         = 0x1380,
    SMSG_MOVE_SET_FLIGHT_BACK_SPEED              = 0x30A2, // 4.3.4 15595
    SMSG_FORCE_FLIGHT_SPEED_CHANGE               = 0x1382,
    CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK           = 0x09DA, // 5.4.8 18414 (Wow.exe binary, via CMSG_MOVE_FORCE_FLIGHT_SPEED_CHANGE_ACK)
    SMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE          = 0x1384,
    CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK      = 0x105B, // 5.4.8 18414 (Wow.exe binary, via CMSG_MOVE_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK)
    SMSG_SPLINE_SET_FLIGHT_SPEED                 = 0x1386,
    SMSG_SPLINE_SET_FLIGHT_BACK_SPEED            = 0x1387,
    SMSG_FLIGHT_SPLINE_SYNC                      = 0x1389,
    CMSG_SET_TAXI_BENCHMARK_MODE                 = 0x0762, // 5.4.8 18414 (Wow.exe binary)
    SMSG_JOINED_BATTLEGROUND_QUEUE               = 0x138B,
    SMSG_REALM_SPLIT                             = 0x1A2E, // 5.4.8 18414 (SkyFire)
    CMSG_REALM_SPLIT                             = 0x0062, // 5.4.8 18414 (Wow.exe binary)
    CMSG_MOVE_CHNG_TRANSPORT                     = 0x09DB, // 5.4.8 18414 (Wow.exe binary)
    MSG_PARTY_ASSIGNMENT                         = 0x0424, // 4.3.4 15595
    SMSG_OFFER_PETITION_ERROR                    = 0x2716, // 4.3.4 15595
    SMSG_TIME_SYNC_REQ                           = 0x1A8F, // 5.4.8 18414
    CMSG_TIME_SYNC_RESP                          = 0x01DB, // 5.4.8 18414 (Wow.exe binary)
    SMSG_RESET_FAILED_NOTIFY                     = 0x4616, // 4.3.4 15595
    SMSG_REAL_GROUP_UPDATE                       = 0x0F34, // 4.3.4 15595
    SMSG_LFG_DISABLED                            = 0x0815, // 4.3.4 15595
    SMSG_CHEAT_DUMP_ITEMS_DEBUG_ONLY_RESPONSE    = 0x139C,
    SMSG_CHEAT_DUMP_ITEMS_DEBUG_ONLY_RESPONSE_WRITE_FILE = 0x139D,
    SMSG_UPDATE_COMBO_POINTS                     = 0x6B34, // 4.3.4 15595
    SMSG_VOICE_SESSION_ROSTER_UPDATE             = 0x139F,
    SMSG_VOICE_SESSION_LEAVE                     = 0x13A0,
    SMSG_VOICE_SESSION_ADJUST_PRIORITY           = 0x13A1,
    SMSG_VOICE_SET_TALKER_MUTED                  = 0x13A3,
    SMSG_INIT_EXTRA_AURA_INFO_OBSOLETE           = 0x13A4,
    SMSG_SET_EXTRA_AURA_INFO_OBSOLETE            = 0x13A5,
    SMSG_SET_EXTRA_AURA_INFO_NEED_UPDATE_OBSOLETE = 0x13A6,
    SMSG_CLEAR_EXTRA_AURA_INFO_OBSOLETE          = 0x13A7,
    CMSG_MOVE_START_DESCEND                      = 0x01D1, // 5.4.8 18414
    SMSG_IGNORE_REQUIREMENTS_CHEAT               = 0x13AA,
    SMSG_SPELL_CHANCE_PROC_LOG                   = 0x13AB,
    SMSG_DISMOUNT                                = 0x2135, // 4.3.4 15595
    SMSG_MOVE_UPDATE_CAN_FLY                     = 0x3DA1, // 4.3.4 15595
    MSG_RAID_READY_CHECK_CONFIRM                 = 0x03C9, // 5.3.0 17128
    CMSG_VOICE_SESSION_ENABLE                    = 0x15A9, // 5.4.8 18414 (Wow.exe binary)
    SMSG_VOICE_SESSION_ENABLE                    = 0x13B1,
    SMSG_VOICE_PARENTAL_CONTROLS                 = 0x0CC9, // 5.3.0 17128
    SMSG_GM_MESSAGECHAT                          = 0x6434, // 4.3.4 15595
    MSG_GM_GEARRATING                            = 0x13B5,
    CMSG_COMMENTATOR_ENABLE                      = 0x0B07, // 4.3.4 15595
    SMSG_COMMENTATOR_STATE_CHANGED               = 0x0737, // 4.3.4 15595
    CMSG_COMMENTATOR_GET_MAP_INFO                = 0x0026, // 4.3.4 15595
    SMSG_COMMENTATOR_MAP_INFO                    = 0x0327, // 4.3.4 15595
    CMSG_COMMENTATOR_GET_PLAYER_INFO             = 0x0D14, // 4.3.4 15595
    SMSG_COMMENTATOR_GET_PLAYER_INFO             = 0x13BB,
    SMSG_COMMENTATOR_PLAYER_INFO                 = 0x2F36, // 4.3.4 15595
    CMSG_COMMENTATOR_ENTER_INSTANCE              = 0x4105, // 4.3.4 15595
    CMSG_COMMENTATOR_EXIT_INSTANCE               = 0x6136, // 4.3.4 15595
    CMSG_COMMENTATOR_INSTANCE_COMMAND            = 0x0917, // 4.3.4 15595
    SMSG_CLEAR_TARGET                            = 0x4B26, // 4.3.4 15595
    SMSG_CROSSED_INEBRIATION_THRESHOLD           = 0x2036, // 4.3.4 15595
    CMSG_CHEAT_PLAYER_LOGIN                      = 0x13C3,
    SMSG_CHEAT_PLAYER_LOOKUP                     = 0x13C5,
    SMSG_KICK_REASON                             = 0x13C6,
    MSG_RAID_READY_CHECK_FINISHED                = 0x1591, // 5.3.0 17128
    CMSG_COMPLAIN                                = 0x0319, // 5.4.8 18414 (Wow.exe binary)
    SMSG_COMPLAIN_RESULT                         = 0x6D24, // 4.3.4 15595
    SMSG_FEATURE_SYSTEM_STATUS                   = 0x121E, // 5.4.8 18414 (SkyFire) (was 0x3DB7)
    CMSG_CHANNEL_SILENCE_VOICE                   = 0x2D54, // 4.3.4 15595
    CMSG_CHANNEL_SILENCE_ALL                     = 0x2154, // 4.3.4 15595
    CMSG_CHANNEL_UNSILENCE_VOICE                 = 0x3146, // 4.3.4 15595
    CMSG_CHANNEL_UNSILENCE_ALL                   = 0x2546, // 4.3.4 15595
    CMSG_CHANNEL_DISPLAY_LIST                    = 0x2144, // 4.3.4 15595
    CMSG_SET_ACTIVE_VOICE_CHANNEL                = 0x4305, // 4.3.4 15595
    SMSG_CHANNEL_MEMBER_COUNT                    = 0x6414, // 4.3.4 15595
    CMSG_CHANNEL_VOICE_ON                        = 0x1144, // 4.3.4 15595
    CMSG_CHANNEL_VOICE_OFF                       = 0x13D8,
    SMSG_DEBUG_LIST_TARGETS                      = 0x13DA,
    SMSG_AVAILABLE_VOICE_CHANNEL                 = 0x13DB,
    CMSG_ADD_VOICE_IGNORE                        = 0x13DC,
    CMSG_DEL_VOICE_IGNORE                        = 0x13DD,
    CMSG_PARTY_SILENCE                           = 0x6B26, // 4.3.4 15595
    CMSG_PARTY_UNSILENCE                         = 0x4D24, // 4.3.4 15595
    MSG_NOTIFY_PARTY_SQUELCH                     = 0x4D06, // 4.3.4 15595
    SMSG_COMSAT_RECONNECT_TRY                    = 0x4D35, // 4.3.4 15595
    SMSG_COMSAT_DISCONNECT                       = 0x0316, // 4.3.4 15595
    SMSG_COMSAT_CONNECT_FAIL                     = 0x6317, // 4.3.4 15595
    SMSG_VOICE_CHAT_STATUS                       = 0x0F15, // 4.3.4 15595
    CMSG_REPORT_PVP_AFK                          = 0x06F9, // 5.4.8 18414 (Wow.exe binary)
    SMSG_REPORT_PVP_AFK_RESULT                   = 0x2D06, // 4.3.4 15595
    CMSG_GUILD_BANKER_ACTIVATE                   = 0x0372, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_BANK_QUERY_TAB                    = 0x1372, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GUILD_BANK_LIST                         = 0x78A5, // 4.3.4 15595
    CMSG_GUILD_BANK_SWAP_ITEMS                   = 0x136A, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_BANK_BUY_TAB                      = 0x0251, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_BANK_UPDATE_TAB                   = 0x07C2, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_BANK_DEPOSIT_MONEY                = 0x0770, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_BANK_WITHDRAW_MONEY               = 0x07EA, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_BANK_LOG_QUERY                    = 0x0CD3, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GUILD_BANK_LOG_QUERY_RESULT             = 0x30B2, // 4.3.4 15595
    CMSG_SET_CHANNEL_WATCH                       = 0x4517, // 4.3.4 15595
    SMSG_USERLIST_ADD                            = 0x13C5, // 5.3.0 17128
    SMSG_USERLIST_REMOVE                         = 0x0899, // 5.3.0 17128
    SMSG_USERLIST_UPDATE                         = 0x0AC5, // 5.3.0 17128
    CMSG_CLEAR_CHANNEL_WATCH                     = 0x2604, // 4.3.4 15595
    SMSG_INSPECT_RESULTS                         = 0x4014, // 4.3.4 15595
    SMSG_GOGOGO_OBSOLETE                         = 0x13F6,
    SMSG_ECHO_PARTY_SQUELCH                      = 0x0814, // 4.3.4 15595
    CMSG_SET_TITLE_SUFFIX                        = 0x13F8,
    CMSG_SPELLCLICK                              = 0x067A, // 5.4.8 18414 (Wow.exe binary)
    SMSG_LOOT_LIST                               = 0x1199, // 5.3.0 17128
    SMSG_VOICESESSION_FULL                       = 0x6225, // 4.3.4 15595
    CMSG_GUILD_PERMISSIONS                       = 0x145A, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GUILD_PERMISSIONS                       = 0x34A3, // 4.3.4 15595
    CMSG_GUILD_BANK_MONEY_WITHDRAWN              = 0x14DB, // 5.4.8 18414 (Wow.exe binary, via CMSG_GUILD_BANK_MONEY_WITHDRAWN_QUERY)
    SMSG_GUILD_BANK_MONEY_WITHDRAWN              = 0x5DB4, // 4.3.4 15595
    CMSG_GUILD_EVENT_LOG_QUERY                   = 0x15D9, // 5.4.8 18414 (Wow.exe binary)
    SMSG_GUILD_EVENT_LOG                         = 0x10B2, // 4.3.4 15595
    CMSG_GET_MIRRORIMAGE_DATA                    = 0x02A3, // 5.4.8 18414 (Wow.exe binary, via CMSG_GET_MIRROR_IMAGE_DATA)
    SMSG_MIRRORIMAGE_DATA                        = 0x2634, // 4.3.4 15595
    SMSG_FORCE_DISPLAY_UPDATE                    = 0x1404,
    SMSG_SPELL_CHANCE_RESIST_PUSHBACK            = 0x1405,
    SMSG_IGNORE_DIMINISHING_RETURNS_CHEAT        = 0x0125, // 4.3.4 15595
    CMSG_KEEP_ALIVE                              = 0x1A87, // 5.4.8 18414 (Wow.exe binary)
    SMSG_RAID_READY_CHECK_ERROR                  = 0x1409,
    CMSG_OPT_OUT_OF_LOOT                         = 0x06E0, // 5.4.8 18414 (Wow.exe binary)
    CMSG_QUERY_GUILD_BANK_TEXT                   = 0x0550, // 5.4.8 18414 (Wow.exe binary, via CMSG_GUILD_BANK_QUERY_TEXT)
    SMSG_GUILD_BANK_TEXT                         = 0x75A3, // 4.3.4 15595
    CMSG_SET_GUILD_BANK_TEXT                     = 0x3023, // 4.3.4 15595
    CMSG_GRANT_LEVEL                             = 0x0662, // 5.4.8 18414 (Wow.exe binary)
    MSG_GM_CHANGE_ARENA_RATING                   = 0x1410,
    CMSG_DECLINE_CHANNEL_INVITE                  = 0x1411,
    SMSG_GROUPACTION_THROTTLED                   = 0x1394, // 5.3.0 17128
    SMSG_OVERRIDE_LIGHT                          = 0x4225, // 4.3.4 15595
    SMSG_TOTEM_CREATED                           = 0x2414, // 4.3.4 15595
    CMSG_TOTEM_DESTROYED                         = 0x1263, // 5.4.8 18414 (Wow.exe binary)
    CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY        = 0x02F1, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUESTGIVER_STATUS_MULTIPLE              = 0x4F25, // 4.3.4 15595
    CMSG_SET_PLAYER_DECLINED_NAMES               = 0x09E2, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SET_PLAYER_DECLINED_NAMES_RESULT        = 0x08CC, // 5.3.0 17128
    SMSG_SERVER_BUCK_DATA                        = 0x141E,
    SMSG_SEND_UNLEARN_SPELLS                     = 0x02D9, // 5.3.0 17128
    SMSG_PROPOSE_LEVEL_GRANT                     = 0x6114, // 4.3.4 15595
    CMSG_ACCEPT_LEVEL_GRANT                      = 0x02FB, // 5.4.8 18414 (Wow.exe binary)
    SMSG_REFER_A_FRIEND_FAILURE                  = 0x2037, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_FLYING                  = 0x31B5, // 4.3.4 15595
    SMSG_SPLINE_MOVE_UNSET_FLYING                = 0x58A6, // 4.3.4 15595
    SMSG_SUMMON_CANCEL                           = 0x0B34, // 4.3.4 15595
    CMSG_ALTER_APPEARANCE                        = 0x07F0, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ENABLE_BARBER_SHOP                      = 0x2D16, // 4.3.4 15595
    SMSG_BARBER_SHOP_RESULT                      = 0x6125, // 4.3.4 15595
    CMSG_CALENDAR_GET_CALENDAR                   = 0x1F9F, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_GET_EVENT                      = 0x030C, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_GUILD_FILTER                   = 0x04E3, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_ARENA_TEAM                     = 0x142D, // not in 5.4.8 (legacy; handler retained)
    CMSG_CALENDAR_ADD_EVENT                      = 0x0A37, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_UPDATE_EVENT                   = 0x1F8D, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_REMOVE_EVENT                   = 0x0C61, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_COPY_EVENT                     = 0x1A97, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_EVENT_INVITE                   = 0x1D8E, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_EVENT_RSVP                     = 0x1FB8, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_EVENT_REMOVE_INVITE            = 0x0962, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_EVENT_STATUS                   = 0x1AB3, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_EVENT_MODERATOR_STATUS         = 0x0708, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CALENDAR_SEND_CALENDAR                  = 0x6805, // 4.3.4 15595
    SMSG_CALENDAR_SEND_EVENT                     = 0x1438, // 4.3.4 15595
    SMSG_CALENDAR_FILTER_GUILD                   = 0x1439,
    SMSG_CALENDAR_ARENA_TEAM                     = 0x143A,
    SMSG_CALENDAR_EVENT_INVITE                   = 0x143B,
    SMSG_CALENDAR_EVENT_INVITE_REMOVED           = 0x143C,
    SMSG_CALENDAR_EVENT_STATUS                   = 0x143D,
    SMSG_CALENDAR_COMMAND_RESULT                 = 0x143E,
    SMSG_CALENDAR_RAID_LOCKOUT_ADDED             = 0x143F,
    SMSG_CALENDAR_RAID_LOCKOUT_REMOVED           = 0x1440,
    SMSG_CALENDAR_EVENT_INVITE_ALERT             = 0x1441, // 4.3.4 15595
    SMSG_CALENDAR_EVENT_INVITE_REMOVED_ALERT     = 0x1442,
    SMSG_CALENDAR_EVENT_INVITE_STATUS_ALERT      = 0x1443,
    SMSG_CALENDAR_EVENT_REMOVED_ALERT            = 0x1444,
    SMSG_CALENDAR_EVENT_UPDATED_ALERT            = 0x1445,
    SMSG_CALENDAR_EVENT_MODERATOR_STATUS_ALERT   = 0x1446,
    CMSG_CALENDAR_COMPLAIN                       = 0x1F8F, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CALENDAR_GET_NUM_PENDING                = 0x0813, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CALENDAR_SEND_NUM_PENDING               = 0x0C17, // 4.3.4 15595
    SMSG_NOTIFY_DANCE                            = 0x4904, // 4.3.4 15595
    SMSG_PLAY_DANCE                              = 0x4704, // 4.3.4 15595
    CMSG_STOP_DANCE                              = 0x2907, // 4.3.4 15595
    SMSG_STOP_DANCE                              = 0x4637, // 4.3.4 15595
    CMSG_SYNC_DANCE                              = 0x0036, // 4.3.4 15595
    CMSG_DANCE_QUERY                             = 0x4E07, // 4.3.4 15595
    SMSG_DANCE_QUERY_RESPONSE                    = 0x2F06, // 4.3.4 15595
    SMSG_INVALIDATE_DANCE                        = 0x0E27, // 4.3.4 15595
    SMSG_LEARNED_DANCE_MOVES                     = 0x041F, // 5.4.1 17538
    MSG_MOVE_SET_PITCH_RATE_CHEAT                = 0x145B,
    SMSG_MOVE_SET_PITCH_RATE                     = 0x75B0, // 4.3.4 15595
    SMSG_FORCE_PITCH_RATE_CHANGE                 = 0x145D,
    CMSG_FORCE_PITCH_RATE_CHANGE_ACK             = 0x0172, // 5.4.8 18414 (Wow.exe binary, via CMSG_MOVE_FORCE_PITCH_RATE_CHANGE_ACK)
    SMSG_SPLINE_SET_PITCH_RATE                   = 0x145F,
    CMSG_CALENDAR_EVENT_INVITE_NOTES             = 0x1460,
    SMSG_CALENDAR_EVENT_INVITE_NOTES             = 0x1461,
    SMSG_CALENDAR_EVENT_INVITE_NOTES_ALERT       = 0x1462,
    CMSG_UPDATE_MISSILE_TRAJECTORY               = 0x781E, // 4.3.4 15595
    SMSG_UPDATE_ACCOUNT_DATA_COMPLETE            = 0x2015, // 4.3.4 15595
    SMSG_TRIGGER_MOVIE                           = 0x1198, // 5.3.0 17128
    CMSG_COMPLETE_MOVIE                          = 0x1362, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ACHIEVEMENT_EARNED                      = 0x4405, // 4.3.4 15595
    SMSG_DYNAMIC_DROP_ROLL_RESULT                = 0x146A,
    SMSG_CRITERIA_UPDATE                         = 0x6E37, // 4.3.4 15595
    CMSG_QUERY_INSPECT_ACHIEVEMENTS              = 0x0373, // 5.4.8 18414 (Wow.exe binary)
    SMSG_RESPOND_INSPECT_ACHIEVEMENTS            = 0x0DD1, // 5.3.0 17128
    CMSG_DISMISS_CONTROLLED_VEHICLE              = 0x09FA, // 5.4.8 18414 (Wow.exe binary)
    SMSG_QUESTUPDATE_ADD_PVP_KILL                = 0x4416, // 4.3.4 15595
    SMSG_CALENDAR_RAID_LOCKOUT_UPDATED           = 0x1472,
    CMSG_CHAR_CUSTOMIZE                          = 0x0A13, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CHAR_CUSTOMIZE                          = 0x1432, // 5.4.8 18414
    SMSG_PET_RENAMEABLE                          = 0x2B27, // 4.3.4 15595
    CMSG_REQUEST_VEHICLE_EXIT                    = 0x1DC3, // 5.4.8 18414 (Wow.exe binary)
    CMSG_REQUEST_VEHICLE_PREV_SEAT               = 0x03C4, // 5.4.8 18414 (Wow.exe binary)
    CMSG_REQUEST_VEHICLE_NEXT_SEAT               = 0x0141, // 5.4.8 18414 (Wow.exe binary)
    CMSG_REQUEST_VEHICLE_SWITCH_SEAT             = 0x1143, // 5.4.8 18414 (Wow.exe binary)
    CMSG_PET_LEARN_TALENT                        = 0x6725, // 4.3.4 15595
    SMSG_SET_PHASE_SHIFT                         = 0x0BD4, // 5.3.0
    SMSG_ALL_ACHIEVEMENT_DATA                    = 0x12D1, // 5.3.0 17128
    SMSG_HEALTH_UPDATE                           = 0x4734, // 4.3.4 15595
    SMSG_POWER_UPDATE                            = 0x4A07, // 4.3.4 15595
    CMSG_GAMEOBJ_REPORT_USE                      = 0x06D9, // 5.4.8 18414 (Wow.exe binary)
    SMSG_HIGHEST_THREAT_UPDATE                   = 0x4104, // 4.3.4 15595
    SMSG_THREAT_UPDATE                           = 0x1080, // 5.3.0 17128
    SMSG_THREAT_REMOVE                           = 0x0EC0, // 5.3.0 17128
    SMSG_THREAT_CLEAR                            = 0x0199, // 5.3.0 17128
    SMSG_CONVERT_RUNE                            = 0x4F14, // 4.3.4 15595
    SMSG_RESYNC_RUNES                            = 0x6224, // 4.3.4 15595
    SMSG_ADD_RUNE_POWER                          = 0x6915, // 4.3.4 15595
    CMSG_REMOVE_GLYPH                            = 0x148B, // not in 5.4.8 (legacy; handler retained)
    SMSG_DUMP_OBJECTS_DATA                       = 0x148D,
    CMSG_DISMISS_CRITTER                         = 0x12DB, // 5.4.8 18414 (Wow.exe binary)
    SMSG_NOTIFY_DEST_LOC_SPELL_CAST              = 0x148F,
    CMSG_AUCTION_LIST_PENDING_SALES              = 0x02DA, // 5.4.8 18414 (Wow.exe binary)
    SMSG_AUCTION_LIST_PENDING_SALES              = 0x6A27, // 4.3.4 15595
    SMSG_MODIFY_COOLDOWN                         = 0x6016, // 4.3.4 15595
    SMSG_PET_UPDATE_COMBO_POINTS                 = 0x4325, // 4.3.4 15595
    CMSG_ENABLETAXI                              = 0x0741, // 5.4.8 18414 (Wow.exe binary, via CMSG_ENABLE_TAXI)
    SMSG_PRE_RESURRECT                           = 0x6C36, // 4.3.4 15595
    SMSG_AURA_UPDATE_ALL                         = 0x6916, // 5.3.0 17128
    SMSG_AURA_UPDATE                             = 0x4707, // 4.3.4 15595
    SMSG_SERVER_FIRST_ACHIEVEMENT                = 0x6424, // 4.3.4 15595
    SMSG_PET_LEARNED_SPELL                       = 0x1594, // 5.3.0 17128
    SMSG_PET_REMOVED_SPELL                       = 0x6A04, // 4.3.4 15595
    CMSG_CHANGE_SEATS_ON_CONTROLLED_VEHICLE      = 0x08F8, // 5.4.8 18414 (Wow.exe binary)
    CMSG_HEARTH_AND_RESURRECT                    = 0x0360, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ON_CANCEL_EXPECTED_RIDE_VEHICLE_AURA    = 0x4D34, // 4.3.4 15595
    SMSG_CRITERIA_DELETED                        = 0x2915, // 4.3.4 15595
    SMSG_ACHIEVEMENT_DELETED                     = 0x6A16, // 4.3.4 15595
    SMSG_SERVER_INFO_RESPONSE                    = 0x14A2,
    SMSG_SERVER_BUCK_DATA_START                  = 0x14A4,
    SMSG_BATTLEGROUND_INFO_THROTTLED             = 0x0D9D, // 5.3.0 17128
    SMSG_SET_VEHICLE_REC_ID                      = 0x4115, // 4.3.4 15595
    CMSG_RIDE_VEHICLE_INTERACT                   = 0x0277, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CONTROLLER_EJECT_PASSENGER              = 0x06E7, // 5.4.8 18414 (Wow.exe binary, via CMSG_EJECT_PASSENGER)
    SMSG_PET_GUIDS                               = 0x15CD, // 5.3.0 17128
    SMSG_CLIENTCACHE_VERSION                     = 0x002A, // 5.4.8 18414 (SkyFire)
    SMSG_SET_ITEM_PURCHASE_DATA                  = 0x15A3, // 4.3.4 15595
    CMSG_GET_ITEM_PURCHASE_DATA                  = 0x1258, // 5.4.8 18414 (Wow.exe binary)
    CMSG_ITEM_PURCHASE_REFUND                    = 0x074B, // 5.4.8 18414 (Wow.exe binary, via CMSG_ITEM_REFUND)
    SMSG_ITEM_PURCHASE_REFUND_RESULT             = 0x5DB1, // 4.3.4 15595
    CMSG_CORPSE_TRANSPORT_QUERY                  = 0x1FBE, // 5.4.8 18414 (Wow.exe binary, via CMSG_CORPSE_QUERY)
    SMSG_CORPSE_TRANSPORT_QUERY                  = 0x0E35, // 4.3.4 15595
    CMSG_CALENDAR_EVENT_SIGNUP                   = 0x01E3, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CALENDAR_CLEAR_PENDING_ACTION           = 0x14BC,
    SMSG_LOAD_EQUIPMENT_SET                      = 0x2E04, // 4.3.4 15595
    CMSG_SAVE_EQUIPMENT_SET                      = 0x0669, // 5.4.8 18414 (Wow.exe binary, via CMSG_EQUIPMENT_SET_SAVE)
    CMSG_ON_MISSILE_TRAJECTORY_COLLISION         = 0x06D6, // 5.4.8 18414 (Wow.exe binary, via CMSG_MISSILE_TRAJECTORY_COLLISION)
    SMSG_NOTIFY_MISSILE_TRAJECTORY_COLLISION     = 0x14C0,
    SMSG_TALENT_UPDATE                           = 0x6F26, // 4.3.4 15595
    CMSG_LEARN_TALENT_GROUP                      = 0x2415, // 4.3.4 15595
    CMSG_PET_LEARN_TALENT_GROUP                  = 0x6E24, // 4.3.4 15595
    SMSG_DESTROY_ARENA_UNIT                      = 0x2637, // 4.3.4 15595
    SMSG_ARENA_TEAM_CHANGE_FAILED                = 0x6E34, // 4.3.4 15595
    SMSG_PROFILEDATA_RESPONSE                    = 0x14CB,
    SMSG_COMPOUND_MOVE                           = 0x14CE,
    SMSG_MOVE_GRAVITY_DISABLE                    = 0x75B2, // 4.3.4 15595
    CMSG_MOVE_GRAVITY_DISABLE_ACK                = 0x09D3, // 5.4.8 18414 (Wow.exe binary)
    SMSG_MOVE_GRAVITY_ENABLE                     = 0x30B3, // 4.3.4 15595
    CMSG_MOVE_GRAVITY_ENABLE_ACK                 = 0x11D8, // 5.4.8 18414 (Wow.exe binary)
    MSG_MOVE_GRAVITY_CHNG                        = 0x14D3,
    SMSG_SPLINE_MOVE_GRAVITY_DISABLE             = 0x5DB5, // 4.3.4 15595
    SMSG_SPLINE_MOVE_GRAVITY_ENABLE              = 0x3CA6, // 4.3.4 15595
    CMSG_USE_EQUIPMENT_SET                       = 0x036E, // 5.4.8 18414 (Wow.exe binary, via CMSG_EQUIPMENT_SET_USE)
    SMSG_USE_EQUIPMENT_SET_RESULT                = 0x2424, // 4.3.4 15595
    SMSG_FORCE_ANIM                              = 0x4C05, // 4.3.4 15595
    CMSG_CHAR_FACTION_CHANGE                     = 0x0329, // 5.4.8 18414 (Wow.exe binary)
    SMSG_CHAR_FACTION_CHANGE                     = 0x4C06, // 4.3.4 15595
    SMSG_PVP_QUEUE_STATS                         = 0x14DD,
    SMSG_BATTLEFIELD_MANAGER_ENTRY_INVITE        = 0x14DF,
    CMSG_BATTLEFIELD_MANAGER_ENTRY_INVITE_RESPONSE = 0x1806, // 5.4.8 18414 (Wow.exe binary, via CMSG_BATTLEFIELD_MGR_ENTRY_INVITE_RESPONSE)
    SMSG_BATTLEFIELD_MANAGER_ENTERING            = 0x14E1,
    SMSG_BATTLEFIELD_MANAGER_QUEUE_INVITE        = 0x14E2,
    CMSG_BATTLEFIELD_MANAGER_QUEUE_INVITE_RESPONSE = 0x0A97, // 5.4.8 18414 (Wow.exe binary, via CMSG_BATTLEFIELD_MGR_QUEUE_INVITE_RESPONSE)
    CMSG_BATTLEFIELD_MANAGER_QUEUE_REQUEST       = 0x710C, // 4.3.4 15595
    SMSG_BATTLEFIELD_MANAGER_QUEUE_REQUEST_RESPONSE = 0x14E5,
    SMSG_BATTLEFIELD_MANAGER_EJECT_PENDING       = 0x14E6,
    SMSG_BATTLEFIELD_MANAGER_EJECTED             = 0x14E7,
    CMSG_BATTLEFIELD_MANAGER_EXIT_REQUEST        = 0x08B3, // 5.4.8 18414 (Wow.exe binary, via CMSG_BATTLEFIELD_MGR_EXIT_REQUEST)
    SMSG_BATTLEFIELD_MANAGER_STATE_CHANGED       = 0x14E9,
    MSG_SET_RAID_DIFFICULTY                      = 0x0614, // 4.3.4 15595
    SMSG_XPGAIN                                  = 0x14EE,
    SMSG_GMTICKET_RESPONSE_ERROR                 = 0x14EF,
    SMSG_GMTICKET_GET_RESPONSE                   = 0x2E34, // 4.3.4 15595
    SMSG_GMTICKET_RESOLVE_RESPONSE               = 0x0A04, // 4.3.4 15595
    SMSG_GMTICKET_CREATE_RESPONSE_TICKET         = 0x14F3,
    CMSG_GM_CREATE_TICKET_RESPONSE               = 0x14F4,
    SMSG_SERVERINFO                              = 0x1091, // 5.3.0
    CMSG_UI_TIME_REQUEST                         = 0x15AB, // 5.4.8 18414 (Wow.exe binary)
    SMSG_UI_TIME                                 = 0x05AC, // 5.4.1 17538
    CMSG_CHAR_RACE_CHANGE                        = 0x0D24, // 4.3.4 15595
    MSG_VIEW_PHASE_SHIFT                         = 0x14FA,
    SMSG_TALENTS_INVOLUNTARILY_RESET             = 0x14FB,
    SMSG_DEBUG_SERVER_GEO                        = 0x14FD,
    SMSG_LOOT_UPDATE                             = 0x14FE,
    CMSG_READY_FOR_ACCOUNT_DATA_TIMES            = 0x031C, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ALL_QUESTS_COMPLETED                    = 0x6314, // 4.3.4 15595
    SMSG_AFK_MONITOR_INFO_RESPONSE               = 0x1505,
    SMSG_AREA_TRIGGER_NO_CORPSE                  = 0x2A14, // 4.3.4 15595
    SMSG_CAMERA_SHAKE                            = 0x4214, // 4.3.4 15595
    SMSG_SOCKET_GEMS                             = 0x6014, // 4.3.4 15595
    SMSG_CONNECT_TO                              = 0x0942, // 4.3.4 15595
    CMSG_CONNECT_TO_FAILED                       = 0x2533, // 4.3.4 15595
    SMSG_SUSPEND_COMMS                           = 0x4140, // 4.3.4 15595
    SMSG_RESUME_COMMS                            = 0x1512,
    CMSG_AUTH_CONTINUED_SESSION                  = 0x0F49, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SEND_ALL_COMBAT_LOG                     = 0x1515,
    SMSG_OPEN_LFG_DUNGEON_FINDER                 = 0x0412, // 4.3.4 15595
    SMSG_MOVE_SET_COLLISION_HGT                  = 0x11B0, // 4.3.4 15595
    CMSG_MOVE_SET_COLLISION_HGT_ACK              = 0x09FB, // 5.4.8 18414 (Wow.exe binary, via CMSG_MOVE_SET_COLLISION_HEIGHT_ACK)
    MSG_MOVE_SET_COLLISION_HGT                   = 0x1519,
    CMSG_COMMENTATOR_SKIRMISH_QUEUE_COMMAND      = 0x0025, // 4.3.4 15595
    SMSG_COMMENTATOR_SKIRMISH_QUEUE_RESULT1      = 0x2126, // 4.3.4 15595
    SMSG_COMMENTATOR_SKIRMISH_QUEUE_RESULT2      = 0x6814, // 4.3.4 15595
    SMSG_COMPRESSED_UNKNOWN_1310                 = 0x151F,
    SMSG_UNKNOWN_1311                            = 0x1520,
    SMSG_UNKNOWN_1312                            = 0x1521,
    SMSG_UNKNOWN_1314                            = 0x1523,
    SMSG_UNKNOWN_1315                            = 0x1524,
    SMSG_UNKNOWN_1316                            = 0x1525,
    SMSG_UNKNOWN_1317                            = 0x1526,
    SMSG_UNKNOWN_1329                            = 0x1532,
    SMSG_PLAYER_MOVE                             = 0x1A32, // 5.4.8 18414
    SMSG_SPLINE_MOVE_SET_FLIGHT_BACK_SPEED       = 0x38B3, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_FLIGHT_SPEED            = 0x39A0, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_PITCH_RATE              = 0x14B0, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_RUN_BACK_SPEED          = 0x3DB3, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_RUN_SPEED               = 0x51B7, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_SWIM_SPEED              = 0x39A4, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_SWIM_BACK_SPEED         = 0x59A1, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_TURN_RATE               = 0x78B5, // 4.3.4 15595
    SMSG_SPLINE_MOVE_SET_WALK_SPEED              = 0x34A5, // 4.3.4 15595
    CMSG_REORDER_CHARACTERS                      = 0x08A7, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SET_CURRENCY_WEEK_LIMIT                 = 0x70A7, // 4.3.4 15595
    SMSG_SET_CURRENCY                            = 0x1595, // 5.3.0
    SMSG_SEND_CURRENCIES                         = 0x15A5, // 4.3.4 15595
    CMSG_SET_CURRENCY_FLAGS                      = 0x03E4, // 5.4.8 18414 (Wow.exe binary)
    SMSG_WEEKLY_RESET_CURRENCIES                 = 0x3CA1, // 4.3.4 15595
    CMSG_INSPECT_RATED_BG_STATS                  = 0x0882, // 5.4.8 18414 (Wow.exe binary, via CMSG_REQUEST_INSPECT_RATED_BG_STATS)
    CMSG_REQUEST_RATED_BG_INFO                   = 0x2423, // 4.3.4 15595
    CMSG_REQUEST_RATED_BG_STATS                  = 0x0826, // 5.4.8 18414 (Wow.exe binary)
    SMSG_RATED_BG_STATS                          = 0x0394, // 5.3.0
    CMSG_REQUEST_PVP_REWARDS                     = 0x0375, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PVP_REWARDS                             = 0x5DA4, // 4.3.4 15595
    CMSG_REQUEST_PVP_OPTIONS_ENABLED             = 0x0A22, // 5.4.8 18414 (Wow.exe binary)
    SMSG_PVP_OPTIONS_ENABLED                     = 0x0381, // 5.3.0
    CMSG_REQUEST_HOTFIX                          = 0x158D, // 5.4.8 18414 (Wow.exe binary)
    SMSG_DB_REPLY                                = 0x38A4, // 4.3.4 15595
    CMSG_OBJECT_UPDATE_FAILED                    = 0x1061, // 5.4.8 18414 (Wow.exe binary)
    CMSG_REFORGE_ITEM                            = 0x0C4F, // 5.4.8 18414 (Wow.exe binary)
    SMSG_REFORGE_RESULT                          = 0x58A4, // 4.3.4 15595
    CMSG_LOAD_SCREEN                             = 0x1DBD, // 5.4.8 18414 (Wow.exe binary)
    SMSG_START_TIMER                             = 0x0988, // 5.3.0 17128
    CMSG_ENABLE_NAGLE                            = 0x12B3, // 5.4.8 18414 (Wow.exe binary)
    CMSG_GUILD_ACHIEVEMENT_MEMBERS               = 0x1470, // 5.4.8 18414 (Wow.exe binary)
    CMSG_BATTLEMASTER_JOIN_RATED                 = 0x0674, // 5.4.8 18414 (Wow.exe binary)
    CMSG_CLEAR_RAID_MARKER                       = 0x1443, // 5.4.8 18414 (Wow.exe binary)
    SMSG_ARENA_UNIT_DESTROYED                    = 0x000F, // 5.4.8 18414
    CMSG_LF_GUILD_POST_REQUEST                   = 0x1450, // 5.4.8 18414 (Wow.exe binary)
    CMSG_QUERY_GUILD_MEMBERS_FOR_RECIPE          = 0x0CFA, // 5.4.8 18414 (Wow.exe binary)
    CMSG_QUERY_GUILD_MEMBER_RECIPES              = 0x04F0, // 5.4.8 18414 (Wow.exe binary)
    CMSG_QUERY_GUILD_RECIPES                     = 0x0478, // 5.4.8 18414 (Wow.exe binary)
    CMSG_SET_PET_SLOT                            = 0x10A7, // 5.4.8 18414 (Wow.exe binary)
    CMSG_UNLEARN_SPECIALIZATION                  = 0x1841, // 5.4.8 18414 (Wow.exe binary)
    SMSG_MULTIPLE_PACKETS                        = 0x1168, // 5.4.8 18414
    SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT          = 0x0332, // 5.4.8 18414
    CMSG_BATTLEFIELD_MGR_QUEUE_REQUEST           = 0x1283, // 5.4.8 18414 (Wow.exe binary)
    CMSG_LOG_DISCONNECT                          = 0x10B3, // 5.4.8 18414 (Wow.exe binary)
    SMSG_SET_TIME_ZONE_INFORMATION               = 0x19C1, // 5.4.8 18414 (SkyFire) (new symbol)
};

#define OPCODE_TABLE_SIZE 0x2000   // 13-bit wire space; greeting 0x4F57 is OUT of range
// Eluna submodule (shared across MaNGOS cores) references NUM_MSG_TYPES as the opcode upper bound.
// Keep the pre-migration value so Phase 1a leaves Eluna's accepted opcode range unchanged (non-closure
// opcodes still carry Four values up to 0xFFFF). Phase 1b tightens this to OPCODE_TABLE_SIZE once every
// opcode is < 0x2000.
#define NUM_MSG_TYPES 0xFFFF

/**
 * Initializes opcode handler metadata tables.
 */
extern void InitializeOpcodes();

/// Player state
enum SessionStatus
{
    STATUS_AUTHED = 0,                     ///< Player authenticated (_player==NULL, m_playerRecentlyLogout = false or will be reset before handler call)
    STATUS_LOGGEDIN,                       ///< Player in game (_player!=NULL, inWorld())
    STATUS_TRANSFER,                       ///< Player transferring to another map (_player!=NULL, !inWorld())
    STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT, ///< _player!= NULL or _player==NULL && m_playerRecentlyLogout)
    STATUS_NEVER,                          ///< Opcode not accepted from client (deprecated or server side only)
    STATUS_UNHANDLED                       ///< We don' handle this opcode yet
};

/**
 * This determines how a \ref WorldPacket is handled by MaNGOS. This can be either in the
 * same function as we received it in, this is unusual, or it can be in:
 * - \ref World::UpdateSessions if it's not thread safe
 * - \ref Map::Update if it is thread safe
 */
enum PacketProcessing
{
    PROCESS_INPLACE = 0,   ///< process packet whenever we receive it - mostly for non-handled or non-implemented packets
    PROCESS_THREADUNSAFE,  ///< packet is not thread-safe - process it in \ref World::UpdateSessions
    PROCESS_THREADSAFE     ///< packet is thread-safe - process it in \ref Map::Update
};

class WorldPacket;

/**
 * A structure containing some of the necessary info to handle a \ref WorldPacket when it comes in.
 * The most interesting thing in here is the \ref OpcodeHandler::handler that actually does
 * something with one of the opcodes (see \ref Opcodes) that came in.
 */
struct OpcodeHandler
{
    ///A string representation of the name of this opcode (see \ref Opcodes)
    char const* name;
    ///The status for this handler, it tells whether or not we will handle the packet at all and
    ///when we will handle it.
    SessionStatus status;
    ///This tells where the packet should be processed, ie: is it thread un/safe, which in turn
    ///determines where it will be processed
    PacketProcessing packetProcessing;
    ///The callback called for this opcode which will work some magic
    void (WorldSession::*handler)(WorldPacket& recvPacket);
};

enum PacketDirection { DIR_CLIENT, DIR_SERVER };
extern OpcodeHandler clientOpcodeTable[OPCODE_TABLE_SIZE];
extern OpcodeHandler serverOpcodeTable[OPCODE_TABLE_SIZE];
OpcodeHandler const* LookupClientOpcode(uint16 value);
char const* LookupClientOpcodeName(uint16 value);
char const* LookupServerOpcodeName(uint16 value);
char const* LookupOpcodeName(PacketDirection dir, uint16 value);
#endif

/**
 * \var OpcodesList::SMSG_PERIODICAURALOG
 * This opcode is used to send data for the combat log when you receive either periodic damage or
 * buffs from a \ref Aura in some way, ie  you gain 10 life every second, you increase your regen
 * of power or something along those lines. The data that needs to be sent is a little different
 * depending on the \ref Modifier for the \ref Aura, what should always be included though is:
 * - The victims Pack GUID (see \ref Object::GetPackGUID)
 * - The casting \ref Player s Pack GUID (see \ref Object::GetPackGUID)
 * - The spellid for the \ref Aura (see \ref Aura::GetId) as a \ref uint32
 * - A 1 as a \ref uint32 this is the count of something (what)
 * - The id of the aura see \ref Modifier::m_auraname as a \ref uint32
 *
 * Now comes different parts depending on what value the \ref Modifier::m_auraname has, if it
 * is \ref AuraType::SPELL_AURA_PERIODIC_DAMAGE or
 * \ref AuraType::SPELL_AURA_PERIODIC_DAMAGE_PERCENT then this is sent:
 * - Damage done as a \ref uint32 from \ref SpellPeriodicAuraLogInfo::damage
 * - The \ref SpellSchools of the \ref SpellEntry for the \ref Aura as a \ref uint32 (see
 * \ref SpellEntry::School)
 * - How much that was absorbed as a \ref uint32
 * - How mcuh that was resisted as a \ref uint32
 *
 * If the \ref Modifier::m_auraname has one of the values of:
 * \ref AuraType::SPELL_AURA_PERIODIC_HEAL or \ref AuraType::SPELL_AURA_OBS_MOD_HEALTH then
 * this should be sent:
 * - Damage/healing (in this case) done as a \ref uint32
 *
 * If the \ref Modifier::m_auraname has one of the values of:
 * \ref AuraType::SPELL_AURA_OBS_MOD_MANA or \ref AuraType::SPELL_AURA_PERIODIC_ENERGIZE then
 * this should be sent:
 * - The \ref Modifier::m_miscvalue as a \ref uint32, in this case it's a power type from the
 * \ref Powers
 * - The damage/mana earned (in this case) as a \ref uint32
 *
 * If the \ref Modifier::m_auraname has one of the values of:
 * \ref AuraType::SPELL_AURA_PERIODIC_MANA_LEECH then this should be sent:
 * - The \ref Modifier::m_miscvalue as a \ref uint32, in this case it's a power type from the
 * \ref Powers
 * - The damage/amount of mana drained (in this case) as a \ref uint32
 * - The gain multiplier as a \ref float from the which probably increases how much power was
 * drained
 *
 * To not create this packet and send it all the time you need it you can use
 * \ref Unit::SendPeriodicAuraLog
 *
 * Also, this should be sent with \ref Object::SendMessageToSet so that all nearby (in
 * the same \ref Cell) \ref Player s get the information. To do this with an \ref Aura
 * one could use \ref Aura::GetTarget and then use the \ref Unit::SendMessageToSet
 * \todo Is it actually for the combat log?
 * \todo Is it in the same \ref Cell?
 * \todo What is the count that is sent as a uint32?
 * \todo Document the multiplier in some way?
 */

/**
 * \var OpcodesList::SMSG_SPELLNONMELEEDAMAGELOG
 * This opcode is used to send data for the combat log when you damage someone with a non melee
 * spell, ie frostbolt.
 * The data that needs to be sent is the following in the same order:
 * - The victims Pack GUID (see \ref Object::GetPackGUID)
 * - The \ref Player s Pack GUID (see \ref Object::GetPackGUID)
 * - Id of the spell that was used as a \ref uint32
 * - The amount of damage that was done (not including resisted damage etc) as a \ref uint32
 * - The \ref SpellSchoolMask of the \ref Spell as a \ref uint8, should be from the representation
 * in \ref SpellSchools though, to do this one can use \ref GetFirstSchoolInMask
 * - The amount of absorbed damage as a \ref uint32
 * - The amount of resisted damage as a \ref uint32
 * - A \ref uint8 which if it is 1 shows the spell name for the client, ie: "%s's ranged shot
 * hit %s for %d damage" (taken from source) and if it's 0 no message is shown
 * - A \ref uint8 value that seems to be unused
 * - The amount of blocked damage as a \ref uint32
 * - The \ref HitInfo as a \ref uint32 which tells what happened it would seem
 * - A \ref uint8 that's usually 0 and is used as a flag to use extended data (taken from source)
 *
 * To not create this packet and send it all the time you need it you can use
 * \ref Unit::SendSpellNonMeleeDamageLog
 *
 * Also, this should be sent with \ref Object::SendMessageToSet so that all nearby (in
 * the same \ref Cell) \ref Player s get the information.
 * \todo Is it actually for the combat log?
 * \todo Is it in the same \ref Cell?
 */

/**
 * \var OpcodesList::SMSG_SPELLENERGIZELOG
 * This opcode is used to send data for the combat log when you gain energy in some way.
 * The data that needs to be sent is the following in the same order:
 * - The victims Pack GUID (see \ref Object::GetPackGUID)
 * - The \ref Player s Pack GUID (see \ref Object::GetPackGUID)
 * - the spellid as a \ref uint32
 * - the powertype as a \ref uint32, see \ref Powers for the available power types
 * - the damage or in this case gain as a \ref uint32
 *
 * To not create this packet and send it all the time you need it you can use
 * \ref Unit::SendEnergizeSpellLog
 * Also, this should be sent with \ref Object::SendMessageToSet so that all nearby (in
 * the same \ref Cell) \ref Player s get the information.
 * \todo Is it actually for the combat log?
 * \todo Is it in the same \ref Cell?
 */

/**
 * \var OpcodesList::SMSG_SPELLHEALLOG
 * This opcode is used to send data for the combat log when healing is done. The data
 * that needs to be sent is the following in the same order:
 * - The victims Pack GUID (see \ref Object::GetPackGUID)
 * - The \ref Player s Pack GUID (see \ref Object::GetPackGUID)
 * - The spellid as a \ref uint32
 * - The damage/healing done as a \ref uint32
 * - If it was critical or not as a \ref uint8 (1 meaning critical, 0 meaning normal)
 * - And a \ref uint8 with the value 0 which doesn't seem to be used in the client
 *
 * To not create this packet and send it all the time you need it you can use
 * \ref Unit::SendHealSpellLog
 * Also, this should be sent with \ref Object::SendMessageToSet so that all nearby (in
 * the same \ref Cell) \ref Player s get the information.
 * \todo Is it actually for the combat log?
 * \todo Is it in the same \ref Cell?
 */

/**
 * \var OpcodesList::SMSG_ATTACKERSTATEUPDATE
 * This opcode is used to send information about a recent hit, who it hit, how
 * much damage it did and so forth. See the \ref CalcDamageInfo structure for more
 * info on what will be sent. The data that needs to be sent is the following in
 * the same order:
 * - The \ref CalcDamageInfo::HitInfo as a \ref uint32
 * - The \ref Unit s Pack GUID (see \ref Object::GetPackGUID)
 * - The targets Pack GUID (see \ref Object::GetPackGUID)
 * - The full damage that was done as a \ref uint32
 * - A 1 as a \ref uint8, this acts as the subdamage count (could it be higher?)
 * - A \ref uint32 of \code{.cpp} GetFirstSchoolInMask(damageInfo->damageSchoolMask) \endcode
 * Need to find out what this does
 * - A float representation of the damage (seen as sub damage from comments)
 * - A \ref uint32 representation of the same damage
 * - A \ref uint32 representation of how much was absorbed (see \ref CalcDamageInfo::absorb)
 * - A \ref uint32 representation of how much was resisted (see \ref CalcDamageInfo::resist)
 * - The targets state as a \ref uint32 (see \ref CalcDamageInfo::TargetState)
 * - If the absorbed part is zero add a 0 as an \ref uint32 otherwise add a -1 as an \ref uint32
 * - The spell id as a \ref uint32 if a spell was used, although in
 * \ref Unit::SendAttackStateUpdate it is always 0.
 * - The blocked amount as a \ref uint32 (see \ref CalcDamageInfo::blocked_amount) this is
 * normally \ref HitInfo::HITINFO_NOACTION according to comments in \ref Unit::SendAttackStateUpdate
 *
 * It appears this should also be sent with \ref Object::SendMessageToSet to that all nearby (in
 * the same \ref Cell) \ref Player s can get take part of the info
 * \see VictimState
 * \todo Is this correct? Is it really about a recent hit?
 */


/// @}
