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
        typedef bool (*DecryptFn)(void* ctx, uint8_t* header, size_t len);   ///< in-place ARC4 on the HEADER only
        typedef bool (*CmdValidFn)(void* ctx, uint32_t cmd, bool preCrypt);  ///< game rule; false => reject
        struct Frame { uint32_t cmd; std::vector<uint8_t> payload; };

        MopFrameReader()
          : m_rpos(0), m_haveHeader(false), m_cmd(0), m_need(0),
            m_reason(MF_NONE), m_hdrLen(0), m_hdrPreCrypt(false) {}
        void Push(const uint8_t* data, size_t len) { m_buf.insert(m_buf.end(), data, data + len); compact(); }

        /// postCrypt is m_Crypt.IsInitialized() - the SOLE codec authority - passed each call; it may change
        /// between frames (never mid-frame: crypt inits only after a full frame is processed).
        Status TryFrame(Frame& out, bool postCrypt, void* ctx, DecryptFn decrypt, CmdValidFn cmdValid)
        {
            if (!m_haveHeader)
            {
                const size_t hlen = postCrypt ? 4u : 6u;
                if (avail() < hlen) { return NEED_MORE; }               // NO decrypt call before a complete header
                uint8_t hdr[6];
                for (size_t i = 0; i < hlen; ++i) { hdr[i] = m_buf[m_rpos + i]; }
                if (decrypt && !decrypt(ctx, hdr, hlen)) { return fail(MF_DECRYPT, hdr, hlen, !postCrypt); }
                uint16_t payloadSize = 0;
                if (postCrypt)
                {
                    uint16_t c16 = 0;
                    if (!MopWire::ReadClientPostCryptHeader(hdr, payloadSize, c16)) { return fail(MF_BAD_SIZE, hdr, hlen, false); }
                    m_cmd = c16;
                }
                else
                {
                    uint32_t c32 = 0;
                    if (!MopWire::ReadClientPreCryptHeader(hdr, payloadSize, c32)) { return fail(MF_BAD_SIZE, hdr, hlen, true); }
                    m_cmd = c32;
                }
                if (cmdValid && !cmdValid(ctx, m_cmd, !postCrypt)) { return fail(MF_BAD_COMMAND, hdr, hlen, !postCrypt); }
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
