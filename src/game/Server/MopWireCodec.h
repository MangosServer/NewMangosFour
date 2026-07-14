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
