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
    ///
    /// PRE-CRYPT writes {uint16 size LE; uint16 cmd LE}, size = payload + 2, UNENCRYPTED.
    /// The size field is LITTLE-endian. Confirmed by the live 5.4.8.18414 gate: the client decoded
    /// our `29 00 49 09` (size 0x0029 = 41 = 39 + 2, cmd 0x0949 SMSG_AUTH_CHALLENGE) and replied
    /// with CMSG_AUTH_SESSION. The client's OWN greeting reply uses the same LE shape (`30 00` = 48
    /// for a 50-byte frame; big-endian that would be 12288, absurd). DO NOT byte-swap this to match
    /// the deleted Cata-era ServerPktHeader -- that baseline wrote the size big-endian and is the
    /// direct cause of the "Connected." stall this replaced; no MoP client ever framed against it.
    ///
    /// SEQUENCING CONSTRAINT -- PHASE 1b MUST LAND BEFORE PHASE 3 INITIALIZES CRYPT.
    /// The post-crypt header packs (size << 13) | (cmd & 0x1FFF), so the opcode field is physically
    /// 13 bits: a cmd > 0x1FFF CANNOT be represented and is rejected here (SendPacket logs
    /// "frame out of range" and returns -1 -- never silent). 536 values in Opcodes.h still exceed
    /// 0x1FFF (279 of them SMSG, e.g. SMSG_MESSAGECHAT = 0x2026) because they retain their 4.3.4
    /// values. This is UNREACHABLE for all of Phase 2 (crypt is never initialized, so postCrypt is
    /// always false), but once Phase 3 calls m_Crypt.Init() those sends begin failing unless the
    /// Phase 1b opcode-table migration has already remapped them into the 13-bit space.
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
    /// 6-byte auth-era header {uint16 size LE; uint32 cmd LE}, size = payload + 4. Used for
    /// CMSG_AUTH_SESSION onward; the greeting uses ReadClientGreetingHeader's 4-byte form instead.
    ///
    /// The size field is LITTLE-endian, and the arithmetic is decisive. The live 5.4.8.18414 gate
    /// captured the client emitting `b7 01 b2 00 00 00` for a 441-byte frame:
    ///     LE: size = 0x01B7 = 439 = 435 + 4, and the 435-byte body parsed cleanly.  <- correct
    ///     BE: size = 0xB701 = 46849, which trips the `size > 10240` reject below and would kill
    ///         the connection before authentication could start.
    /// Those bytes are the CLIENT's own encoding, not our interpretation of them.
    ///
    /// DO NOT "restore" a byte-swap here. The deleted Cata-era parser applied
    /// EndianConvertReverse(header.size); that path is precisely what never got a MoP client past
    /// "Connected.", so it cannot serve as evidence of the correct byte order.
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
