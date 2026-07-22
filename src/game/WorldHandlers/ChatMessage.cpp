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

/**
 * @file ChatMessage.cpp
 * @brief Cohesion split of Chat.cpp -- system-message output helpers and chat-packet builder.
 *        Same ChatHandler class; no behaviour change. CMake
 *        file(GLOB WorldHandlers/*.cpp) picks this file up automatically;
 *        Chat.h is unchanged.
 */

#include "Chat.h"
#include "Language.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "World.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Group.h"

/**
 * @brief Sends a system message to the current session, splitting multiline text.
 *
 * @param str The message text to send.
 */
void ChatHandler::SendSysMessage(const char* str)
{
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage original string
    char* buf = mangos_strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {
        // m_session == null when we're accessing these command from the console.
        ObjectGuid senderGuid;
        if (m_session)
        {
            senderGuid = m_session->GetPlayer()->GetObjectGuid();
        }

        ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, line, LANG_UNIVERSAL, CHAT_TAG_NONE, senderGuid);
        m_session->SendPacket(&data);
    }

    delete[] buf;
}

/**
 * @brief Sends a global system message to all sessions meeting the minimum security level.
 *
 * @param str The message text to send.
 * @param minSec The minimum security level required to receive the message.
 */
void ChatHandler::SendGlobalSysMessage(const char* str, AccountTypes minSec)
{
    // Chat output
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage original string
    char* buf = mangos_strdup(str);
    char* pos = buf;
    ObjectGuid senderGuid = m_session ? m_session->GetPlayer()->GetObjectGuid() : ObjectGuid();

    while (char* line = LineFromMessage(pos))
    {
        // m_session == null when we're accessing these command from the console.
        ObjectGuid senderGuid;
        if (m_session)
        {
            senderGuid = m_session->GetPlayer()->GetObjectGuid();
        }

        ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, line, LANG_UNIVERSAL, CHAT_TAG_NONE, senderGuid);
        sWorld.SendGlobalMessage(&data, minSec);
    }

    delete[] buf;
}

/**
 * @brief Sends a localized system message by string table entry.
 *
 * @param entry The string table entry identifier.
 */
void ChatHandler::SendSysMessage(int32 entry)
{
    SendSysMessage(GetMangosString(entry));
}

/**
 * @brief Formats and sends a localized system message.
 *
 * @param entry The string table entry identifier.
 */
void ChatHandler::PSendSysMessage(int32 entry, ...)
{
    const char* format = GetMangosString(entry);
    va_list ap;
    char str [2048];
    va_start(ap, entry);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

/**
 * @brief Formats and sends a localized multiline system message using @@ separators.
 *
 * @param entry The string table entry identifier.
 */
void  ChatHandler::PSendSysMessageMultiline(int32 entry, ...)
{
    uint32 linecount = 0;

    const char* format = GetMangosString(entry);
    va_list ap;
    char str[2048];
    va_start(ap, entry);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);

    std::string mangosString(str);

    /* Used for tracking our position within the string while iterating through it */
    std::string::size_type pos = 0, nextpos;

    /* Find the next occurance of @ in the string
     * This is how newlines are represented */
    while ((nextpos = mangosString.find("@@", pos)) != std::string::npos)
    {
        /* If these are not equal, it means a '@@' was found
         * These are used to represent newlines in the string
         * It is set by the code above here */
        if (nextpos != pos)
        {
            /* Send the player a system message containing the substring from pos to nextpos - pos */
            PSendSysMessage("%s", mangosString.substr(pos, nextpos - pos).c_str());
            ++linecount;
        }
        pos = nextpos + 2; // +2 because there are two @ as delimiter
    }

    /* There are no more newlines in our mangosString, so we send whatever is left */
    if (pos < mangosString.length())
    {
        PSendSysMessage("%s", mangosString.substr(pos).c_str());
    }
}

/**
 * @brief Formats and sends a raw system message.
 *
 * @param format The printf-style format string.
 */
void ChatHandler::PSendSysMessage(const char* format, ...)
{
    va_list ap;
    char str [2048];
    va_start(ap, format);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

void ChatHandler::BuildChatPacket(WorldPacket& data, ChatMsg msgtype, char const* message, Language language /*= LANG_UNIVERSAL*/, ChatTagFlags chatTag /*= CHAT_TAG_NONE*/,
                                  ObjectGuid const& senderGuid /*= ObjectGuid()*/, char const* senderName /*= NULL*/,
                                  ObjectGuid const& targetGuid /*= ObjectGuid()*/, char const* targetName /*= NULL*/,
                                  char const* channelName /*= NULL*/, uint32 achievementId /*= 0*/, const char* addonPrefix /*= NULL*/)
{
    MANGOS_ASSERT(message);

    MopChatPackets::Message packet;
    packet.chatType = msgtype;
    packet.language = language;
    packet.chatTag = chatTag;
    packet.senderGuid = senderGuid.GetRawValue();
    packet.receiverGuid = targetGuid.GetRawValue();
    packet.realmId1 = realmID;
    packet.realmId2 = realmID;
    packet.achievementId = achievementId;
    packet.text = message;

    bool const isGM = (chatTag & CHAT_TAG_GM) != 0;
    bool hasGroupGuid = false;
    bool hasGuildGuid = false;

    switch (msgtype)
    {
        case CHAT_MSG_PARTY:
        case CHAT_MSG_PARTY_LEADER:
        case CHAT_MSG_RAID:
        case CHAT_MSG_RAID_LEADER:
        case CHAT_MSG_RAID_WARNING:
            hasGroupGuid = true;
            break;
        case CHAT_MSG_GUILD:
        case CHAT_MSG_OFFICER:
        case CHAT_MSG_GUILD_ACHIEVEMENT:
            hasGuildGuid = true;
            break;
        case CHAT_MSG_MONSTER_WHISPER:
        case CHAT_MSG_RAID_BOSS_WHISPER:
            if (targetGuid && !targetGuid.IsPlayer() && !targetGuid.IsPet())
            {
                MANGOS_ASSERT(targetName);
                packet.receiverName = targetName;
            }
            // Fall through: monster whispers also carry the sender's name.
        case CHAT_MSG_MONSTER_SAY:
        case CHAT_MSG_MONSTER_PARTY:
        case CHAT_MSG_MONSTER_YELL:
        case CHAT_MSG_MONSTER_EMOTE:
        case CHAT_MSG_RAID_BOSS_EMOTE:
        case CHAT_MSG_BATTLENET:
        case CHAT_MSG_WHISPER_FOREIGN:
            MANGOS_ASSERT(senderName);
            packet.senderName = senderName;
            break;
        case CHAT_MSG_BG_SYSTEM_NEUTRAL:
        case CHAT_MSG_BG_SYSTEM_ALLIANCE:
        case CHAT_MSG_BG_SYSTEM_HORDE:
            if (targetGuid && !targetGuid.IsPlayer())
            {
                MANGOS_ASSERT(targetName);
                packet.receiverName = targetName;
            }
            break;
        case CHAT_MSG_CHANNEL:
            MANGOS_ASSERT(channelName);
            packet.channelName = channelName;
            MANGOS_ASSERT(senderName);
            packet.senderName = senderName;
            break;
        default:
            if (isGM)
            {
                MANGOS_ASSERT(senderName);
                packet.senderName = senderName;
            }
            break;
    }

    if (language == LANG_ADDON && addonPrefix)
        packet.addonPrefix = addonPrefix;

    if ((hasGroupGuid || hasGuildGuid) && senderGuid)
    {
        if (Player* sender = sObjectAccessor.FindPlayer(senderGuid))
        {
            if (hasGroupGuid && sender->GetGroup())
                packet.groupGuid = sender->GetGroup()->GetObjectGuid().GetRawValue();
            if (hasGuildGuid)
                packet.guildGuid = sender->GetGuildGuid().GetRawValue();
        }
    }

    MANGOS_ASSERT(MopChatPackets::BuildMessage(data, packet));
}
