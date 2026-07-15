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

#ifndef MANGOS_MOP_FRAME_READER_H
#define MANGOS_MOP_FRAME_READER_H
#include "MopWireCodec.h"
#include <cstdint>
#include <cstddef>
#include <vector>
class MopFrameReader
{
    public:
        enum Status { NEED_MORE, FRAME_READY, MALFORMED };
        /// Bounded reason for a MALFORMED result (for a diagnostic log - never reveals payload).
        enum MalformedReason { MF_NONE, MF_DECRYPT, MF_BAD_SIZE, MF_BAD_COMMAND };
        /// Inbound header width is STATE-driven, not a simple pre/post-crypt bool:
        ///   HDR_GREETING  - 4 bytes {uint16 size; uint16 cmd}, size = payload + 2. ONLY the client's
        ///                   MSG_WOW_CONNECTION reply. Symmetric with the server's own greeting header.
        ///   HDR_PRECRYPT  - 6 bytes {uint16 size; uint32 cmd}, size = payload + 4. CMSG_AUTH_SESSION onward.
        ///   HDR_POSTCRYPT - 4 bytes packed (size<<13)|(cmd&0x1FFF), after crypt init.
        /// Chosen by WorldSocket from (m_Crypt.IsInitialized(), m_connState) and passed PER CALL -- the
        /// reader stores no codec state of its own.
        enum HeaderKind { HDR_GREETING, HDR_PRECRYPT, HDR_POSTCRYPT };
        typedef bool (*DecryptFn)(void* ctx, uint8_t* header, size_t len);   ///< in-place ARC4 on the HEADER only
        typedef bool (*CmdValidFn)(void* ctx, uint32_t cmd, bool preCrypt);  ///< game rule; false => reject
        struct Frame { uint32_t cmd; std::vector<uint8_t> payload; };

        MopFrameReader()
          : m_rpos(0), m_haveHeader(false), m_cmd(0), m_need(0),
            m_reason(MF_NONE), m_hdrLen(0), m_hdrPreCrypt(false) {}
        void Push(const uint8_t* data, size_t len) { m_buf.insert(m_buf.end(), data, data + len); compact(); }

        /// kind is derived fresh from (m_Crypt.IsInitialized(), m_connState) - the SOLE codec authority -
        /// and passed each call; it may change between frames (never mid-frame: crypt inits and state
        /// advances only after a full frame is processed).
        Status TryFrame(Frame& out, HeaderKind kind, void* ctx, DecryptFn decrypt, CmdValidFn cmdValid)
        {
            const bool preCrypt = (kind != HDR_POSTCRYPT);
            if (!m_haveHeader)
            {
                const size_t hlen = (kind == HDR_PRECRYPT) ? 6u : 4u;
                if (avail() < hlen) { return NEED_MORE; }               // NO decrypt call before a complete header
                uint8_t hdr[6];
                for (size_t i = 0; i < hlen; ++i) { hdr[i] = m_buf[m_rpos + i]; }
                if (decrypt && !decrypt(ctx, hdr, hlen)) { return fail(MF_DECRYPT, hdr, hlen, preCrypt); }
                uint16_t payloadSize = 0;
                if (kind == HDR_POSTCRYPT)
                {
                    uint16_t c16 = 0;
                    if (!MopWire::ReadClientPostCryptHeader(hdr, payloadSize, c16)) { return fail(MF_BAD_SIZE, hdr, hlen, preCrypt); }
                    m_cmd = c16;
                }
                else if (kind == HDR_GREETING)
                {
                    uint32_t c32 = 0;
                    if (!MopWire::ReadClientGreetingHeader(hdr, payloadSize, c32)) { return fail(MF_BAD_SIZE, hdr, hlen, preCrypt); }
                    m_cmd = c32;
                }
                else
                {
                    uint32_t c32 = 0;
                    if (!MopWire::ReadClientPreCryptHeader(hdr, payloadSize, c32)) { return fail(MF_BAD_SIZE, hdr, hlen, preCrypt); }
                    m_cmd = c32;
                }
                if (cmdValid && !cmdValid(ctx, m_cmd, preCrypt)) { return fail(MF_BAD_COMMAND, hdr, hlen, preCrypt); }
                m_rpos += hlen; m_need = payloadSize; m_haveHeader = true;
            }
            if (avail() < m_need) { return NEED_MORE; }
            out.cmd = m_cmd;
            out.payload.assign(m_buf.begin() + m_rpos, m_buf.begin() + m_rpos + m_need);
            m_rpos += m_need; m_haveHeader = false; compact();
            return FRAME_READY;
        }

        MalformedReason LastReason() const { return m_reason; }
        /// Copy of the offending header (<=6 bytes) captured at the MALFORMED point; never contains payload.
        const uint8_t* LastHeader(size_t& len, bool& preCrypt) const { len = m_hdrLen; preCrypt = m_hdrPreCrypt; return m_lastHeader; }
    private:
        Status fail(MalformedReason r, const uint8_t* hdr, size_t len, bool preCrypt)
        {
            m_reason = r; m_hdrLen = (len < 6u ? len : 6u); m_hdrPreCrypt = preCrypt;
            for (size_t i = 0; i < m_hdrLen; ++i) { m_lastHeader[i] = hdr[i]; }
            return MALFORMED;
        }
        size_t avail() const { return m_buf.size() - m_rpos; }
        void compact()
        {
            if (m_rpos && m_rpos == m_buf.size()) { m_buf.clear(); m_rpos = 0; }
            else if (m_rpos > 4096) { m_buf.erase(m_buf.begin(), m_buf.begin() + m_rpos); m_rpos = 0; }
        }
        std::vector<uint8_t> m_buf; size_t m_rpos; bool m_haveHeader; uint32_t m_cmd; size_t m_need;
        MalformedReason m_reason; uint8_t m_lastHeader[6]; size_t m_hdrLen; bool m_hdrPreCrypt;
};
/// Bounded, payload-free name for a malformed reason (for diagnostic logs).
inline const char* MalformedReasonName(MopFrameReader::MalformedReason r)
{
    switch (r)
    {
        case MopFrameReader::MF_DECRYPT:     return "DECRYPT_FAILURE";
        case MopFrameReader::MF_BAD_SIZE:    return "BAD_SIZE";
        case MopFrameReader::MF_BAD_COMMAND: return "BAD_COMMAND";
        default:                             return "NONE";
    }
}
#endif
