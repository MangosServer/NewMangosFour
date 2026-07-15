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

#ifndef MANGOS_MOP_WIRE_CODEC_H
#define MANGOS_MOP_WIRE_CODEC_H
#include <cstdint>
#include <cstddef>
namespace MopWire
{
    inline void WriteServerPreCryptHeader(uint8_t out[4], uint16_t size, uint16_t cmd)
    { out[0]=uint8_t(size); out[1]=uint8_t(size>>8); out[2]=uint8_t(cmd); out[3]=uint8_t(cmd>>8); }
    inline void WriteServerPostCryptHeader(uint8_t out[4], uint32_t size, uint16_t cmd)
    { uint32_t v=(size<<13)|(uint32_t(cmd)&0x1FFF); out[0]=uint8_t(v); out[1]=uint8_t(v>>8); out[2]=uint8_t(v>>16); out[3]=uint8_t(v>>24); }
    /// Validate wide (size_t/uint32) THEN narrow. false => reject (do not send).
    inline bool BuildServerHeader(bool postCrypt, size_t payloadSize, uint32_t cmd, uint8_t out[4])
    {
        if (postCrypt)
        {
            if (payloadSize > 0x7FFFF || cmd > 0x1FFF) { return false; }
            WriteServerPostCryptHeader(out, uint32_t(payloadSize), uint16_t(cmd));
        }
        else
        {
            if (payloadSize > 65533 || cmd > 0xFFFF) { return false; }
            WriteServerPreCryptHeader(out, uint16_t(payloadSize + 2), uint16_t(cmd));
        }
        return true;
    }
    /// Greeting reply header: 4 bytes {uint16 size LE; uint16 cmd LE}, size = payload + 2 -- i.e. the
    /// SAME shape the server emits for its own greeting (the exchange is symmetric). Confirmed on the
    /// wire by the live gate: the client sent `30 00 57 4F 52 4C ...` = size 48, cmd 0x4F57
    /// (MSG_WOW_CONNECTION -- "WO" in LE), payload "RLD OF WARCRAFT CONNECTION - CLIENT TO SERVER\0"
    /// (46 bytes; 46 + 2 == 48). Reading this as the 6-byte auth header swallows "RL" into cmd and
    /// yields 0x4C524F57 -> BAD_COMMAND. Only the greeting uses this width; CMSG_AUTH_SESSION and
    /// later pre-crypt packets use ReadClientPreCryptHeader's 6-byte form.
    inline bool ReadClientGreetingHeader(const uint8_t in[4], uint16_t& payloadSize, uint32_t& cmd)
    {
        uint32_t size=uint32_t(in[0])|(uint32_t(in[1])<<8);
        cmd=uint32_t(in[2])|(uint32_t(in[3])<<8);                // 16-bit cmd; caller range-checks
        if (size<2 || size>10240) { return false; }
        payloadSize=uint16_t(size-2); return true;
    }
    inline bool ReadClientPreCryptHeader(const uint8_t in[6], uint16_t& payloadSize, uint32_t& cmd)
    {
        uint32_t size=uint32_t(in[0])|(uint32_t(in[1])<<8);
        cmd=uint32_t(in[2])|(uint32_t(in[3])<<8)|(uint32_t(in[4])<<16)|(uint32_t(in[5])<<24);
        if (size<4 || size>10240) { return false; }
        payloadSize=uint16_t(size-4); return true;               // FULL 32-bit cmd out; caller range-checks
    }
    inline bool ReadClientPostCryptHeader(const uint8_t in[4], uint16_t& size, uint16_t& cmd)
    {
        uint32_t v=uint32_t(in[0])|(uint32_t(in[1])<<8)|(uint32_t(in[2])<<16)|(uint32_t(in[3])<<24);
        uint32_t s=v>>13; if (s>10236) { return false; }
        size=uint16_t(s); cmd=uint16_t(v&0x1FFF); return true;
    }
}
#endif
