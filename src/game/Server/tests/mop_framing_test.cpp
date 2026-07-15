#include "MopWireCodec.h"
#include "MopHandshake.h"
#include "MopFrameReader.h"
#include <cstdio>
#include <cstdint>
#include <vector>
static int g_fail = 0;
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); ++g_fail; } } while(0)

static void test_codec()
{
    uint8_t h[4];
    CHECK(MopWire::BuildServerHeader(false,46,0x4F57,h) && h[0]==0x30&&h[1]==0x00&&h[2]==0x57&&h[3]==0x4F);
    CHECK(MopWire::BuildServerHeader(false,0,0x0001,h) && h[0]==0x02&&h[1]==0x00);      // pre-crypt payload 0
    CHECK(MopWire::BuildServerHeader(false,65533,0x0001,h));                            // exact pre-crypt max
    CHECK(!MopWire::BuildServerHeader(false,65534,0x0001,h));                           // pre-crypt over-max
    CHECK(!MopWire::BuildServerHeader(false,(size_t)0xFFFFFFFFull+1,0x0001,h));         // huge payload
    CHECK(!MopWire::BuildServerHeader(false,46,0x10001,h));                             // cmd>0xFFFF (before narrow)
    CHECK(MopWire::BuildServerHeader(true,0,0x0001,h) && h[0]==0x01);                   // post-crypt zero
    CHECK(MopWire::BuildServerHeader(true,0x7FFFF,0x1FFF,h));                            // post-crypt max
    CHECK(!MopWire::BuildServerHeader(true,0x80000,0x0001,h));                          // post-crypt oversize
    CHECK(!MopWire::BuildServerHeader(true,0,0x2000,h));                                // opcode overflow (no alias)
    uint16_t sz,c16; uint32_t c32;
    uint8_t ok[6]={0x08,0,0xB2,0,0,0};    CHECK(MopWire::ReadClientPreCryptHeader(ok,sz,c32)&&sz==4&&c32==0x000000B2u);
    uint8_t hi[6]={0x08,0,0xB2,0,0x01,0}; CHECK(MopWire::ReadClientPreCryptHeader(hi,sz,c32)&&c32==0x000100B2u);
    uint8_t sm[6]={0x03,0,0,0,0,0};       CHECK(!MopWire::ReadClientPreCryptHeader(sm,sz,c32));
    uint8_t lg[6]={0x00,0x30,0,0,0,0};    CHECK(!MopWire::ReadClientPreCryptHeader(lg,sz,c32));
    uint8_t pc[4]={0xBA,0x2A,0x05,0x00};  CHECK(MopWire::ReadClientPostCryptHeader(pc,sz,c16)&&sz==41&&c16==0x0ABA);
    uint8_t pb[4]={0xFF,0xFF,0xFF,0xFF};  CHECK(!MopWire::ReadClientPostCryptHeader(pb,sz,c16));   // post-crypt size>10236
}
static void test_legality()
{
    using namespace MopHs;
    const ConnectionState S[4]={CONN_GREETING,CONN_CHALLENGED,CONN_AUTHENTICATING,CONN_AUTHED};
    for (int i=0;i<4;++i)                                  // full 3x4 matrix
    {
        CHECK(IsHandshakeOpcodeLegal(S[i],OPC_GREETING)     == (S[i]==CONN_GREETING));
        CHECK(IsHandshakeOpcodeLegal(S[i],OPC_AUTH_SESSION) == (S[i]==CONN_CHALLENGED));
        CHECK(IsHandshakeOpcodeLegal(S[i],OPC_NORMAL)       == (S[i]==CONN_AUTHED));
    }
    CHECK(RateLimitElapsed(10,11) && !RateLimitElapsed(10,10));
}
static bool constRng(uint8_t* out, size_t len)                  // deterministic: field=0xAB..., seed=01 02 03 04
{
    if (len == 32) { for (size_t i = 0; i < 32; ++i) { out[i] = 0xAB; } return true; }
    if (len == 4)  { out[0] = 1; out[1] = 2; out[2] = 3; out[3] = 4; return true; }
    return false;
}
static bool failFirstRng(uint8_t*, size_t) { return false; }
static int g_rngCall = 0;
static bool failSecondRng(uint8_t* out, size_t len)             // first draw (32B) ok, second (seed) fails
{
    if (++g_rngCall == 1) { for (size_t i = 0; i < len; ++i) { out[i] = 0xCD; } return true; }
    return false;
}
static void test_challenge()
{
    using namespace MopHs;
    std::vector<uint8_t> out; uint32_t seed = 0xDEADBEEFu;
    CHECK(BuildAuthChallengePayload(&constRng, out, seed));
    CHECK(out.size() == 39);
    CHECK(out[0] == 0 && out[1] == 0);                          // two leading zero bytes
    CHECK(out[2] == 0xAB && out[33] == 0xAB);                   // 32 entropy bytes
    CHECK(out[34] == 1);                                        // the 0x01 marker
    CHECK(out[35] == 1 && out[36] == 2 && out[37] == 3 && out[38] == 4);   // seed suffix = LE bytes (identity)
    CHECK(seed == 0x04030201u);                                 // uint32 LE of 01 02 03 04
    out.assign(5, 0xFF); seed = 7;                              // fail-closed: first draw fails
    CHECK(!BuildAuthChallengePayload(&failFirstRng, out, seed) && out.empty());
    g_rngCall = 0; out.assign(5, 0xFF);                         // fail-closed: SECOND (seed) draw fails
    CHECK(!BuildAuthChallengePayload(&failSecondRng, out, seed) && out.empty());
}
struct HookCtx { int decryptCalls, calls6, calls4; bool sawOther, decryptOk, cmdOk; };
static bool countingDecrypt(void* ctx, uint8_t*, size_t len)
{
    HookCtx* c = static_cast<HookCtx*>(ctx);
    ++c->decryptCalls;
    if (len == 6) { ++c->calls6; } else if (len == 4) { ++c->calls4; } else { c->sawOther = true; }
    return c->decryptOk;
}
static bool countingCmdValid(void* ctx, uint32_t cmd, bool)
{
    HookCtx* c = static_cast<HookCtx*>(ctx);
    return c->cmdOk && (cmd == 0x4F57u || cmd < 0x2000u);        // mirrors the production CmdValidHook rule
}
static void test_framereader()
{
    typedef MopFrameReader R;
    // 1) pre-crypt frame, fragmented header -> NO decrypt before a full header; then exactly one 6-byte call.
    {
        HookCtx cx = {0,0,0,false,true,true}; R rd; R::Frame f;
        uint8_t part[3] = {0x08,0,0xB2}; rd.Push(part, 3);      // size=8 => payload 4, cmd 0xB2
        CHECK(rd.TryFrame(f, false, &cx, countingDecrypt, countingCmdValid) == R::NEED_MORE);
        CHECK(cx.decryptCalls == 0);                            // hook NOT called on a partial header
        uint8_t rest[3] = {0,0,0}; rd.Push(rest, 3);
        uint8_t body[4] = {1,2,3,4};  rd.Push(body, 4);
        CHECK(rd.TryFrame(f, false, &cx, countingDecrypt, countingCmdValid) == R::FRAME_READY);
        CHECK(cx.decryptCalls == 1 && cx.calls6 == 1 && cx.calls4 == 0 && !cx.sawOther);
        CHECK(f.cmd == 0x000000B2u && f.payload.size() == 4);
    }
    // 2) post-crypt frame -> exactly one 4-byte decrypt call.
    {
        HookCtx cx = {0,0,0,false,true,true}; R rd; R::Frame f;
        uint8_t h[4] = {0x01,0,0,0};  rd.Push(h, 4);            // v=1 => size 0, cmd 1
        CHECK(rd.TryFrame(f, true, &cx, countingDecrypt, countingCmdValid) == R::FRAME_READY);
        CHECK(cx.decryptCalls == 1 && cx.calls4 == 1 && cx.calls6 == 0 && !cx.sawOther);
        CHECK(f.cmd == 1u && f.payload.empty());
    }
    // 3) malformed: size < 4 -> MF_BAD_SIZE; header captured (6 bytes, pre-crypt), never payload.
    {
        HookCtx cx = {0,0,0,false,true,true}; R rd; R::Frame f;
        uint8_t bad[6] = {0x03,0,0,0,0,0};  rd.Push(bad, 6);
        CHECK(rd.TryFrame(f, false, &cx, countingDecrypt, countingCmdValid) == R::MALFORMED);
        CHECK(rd.LastReason() == R::MF_BAD_SIZE);
        size_t hl = 0; bool pc = false; const uint8_t* hb = rd.LastHeader(hl, pc);
        CHECK(hl == 6 && pc == true && hb[0] == 0x03);
    }
    // 4) malformed: high-bit cmd rejected by the validator -> MF_BAD_COMMAND.
    {
        HookCtx cx = {0,0,0,false,true,true}; R rd; R::Frame f;
        uint8_t hi[6] = {0x08,0,0x00,0x00,0x01,0x00};  rd.Push(hi, 6);   // cmd = 0x00010000 (>= 0x2000)
        CHECK(rd.TryFrame(f, false, &cx, countingDecrypt, countingCmdValid) == R::MALFORMED);
        CHECK(rd.LastReason() == R::MF_BAD_COMMAND);
    }
    // 5) malformed: decrypt hook returns false -> MF_DECRYPT (exactly one call).
    {
        HookCtx cx = {0,0,0,false,false,true}; R rd; R::Frame f;   // decryptOk=false
        uint8_t any[4] = {0x01,0,0,0};  rd.Push(any, 4);
        CHECK(rd.TryFrame(f, true, &cx, countingDecrypt, countingCmdValid) == R::MALFORMED);
        CHECK(rd.LastReason() == R::MF_DECRYPT && cx.decryptCalls == 1);
    }
    // 6) 6->4 transition in ONE buffer, postCrypt passed per call (no stored flag).
    {
        HookCtx cx = {0,0,0,false,true,true}; R rd; R::Frame f;
        uint8_t pre[6]  = {0x04,0,0xB2,0,0,0};   // size 4 => payload 0, cmd 0xB2
        uint8_t post[4] = {0x01,0,0,0};          // size 0, cmd 1
        rd.Push(pre, 6); rd.Push(post, 4);
        CHECK(rd.TryFrame(f, false, &cx, countingDecrypt, countingCmdValid) == R::FRAME_READY && f.cmd == 0xB2u);
        CHECK(rd.TryFrame(f, true,  &cx, countingDecrypt, countingCmdValid) == R::FRAME_READY && f.cmd == 1u);
        CHECK(cx.calls6 == 1 && cx.calls4 == 1 && !cx.sawOther);   // one call each width, never payload
    }
    // 7) complete header + PARTIAL payload -> NEED_MORE; remaining payload -> FRAME_READY;
    //    decrypt count stays EXACTLY ONE across the whole sequence (header not re-decrypted).
    {
        HookCtx cx = {0,0,0,false,true,true}; R rd; R::Frame f;
        uint8_t hdr[6]  = {0x08,0,0xB2,0,0,0};           // size 8 => payload 4, cmd 0xB2
        uint8_t half[2] = {0xAA,0xBB};                   // only 2 of the 4 payload bytes
        rd.Push(hdr, 6); rd.Push(half, 2);
        CHECK(rd.TryFrame(f, false, &cx, countingDecrypt, countingCmdValid) == R::NEED_MORE);
        CHECK(cx.decryptCalls == 1);                     // header decrypted once; NOT re-decrypted while waiting
        uint8_t rest[2] = {0xCC,0xDD}; rd.Push(rest, 2);
        CHECK(rd.TryFrame(f, false, &cx, countingDecrypt, countingCmdValid) == R::FRAME_READY);
        CHECK(cx.decryptCalls == 1 && cx.calls6 == 1 && cx.calls4 == 0);   // still exactly one 6-byte decrypt
        CHECK(f.cmd == 0xB2u && f.payload.size() == 4 && f.payload[0] == 0xAA && f.payload[3] == 0xDD);
    }
    // 8) two ORDINARY pre-crypt frames coalesced in one Push -> READY, READY, then NEED_MORE (drained).
    {
        HookCtx cx = {0,0,0,false,true,true}; R rd; R::Frame f;
        uint8_t two[20] = {0x08,0,0xB2,0,0,0, 0xDE,0xAD,0xBE,0xEF,    // frame A: cmd 0xB2, payload 4
                           0x08,0,0xC3,0,0,0, 0x01,0x02,0x03,0x04};   // frame B: cmd 0xC3, payload 4
        rd.Push(two, 20);
        CHECK(rd.TryFrame(f, false, &cx, countingDecrypt, countingCmdValid) == R::FRAME_READY && f.cmd == 0xB2u);
        CHECK(rd.TryFrame(f, false, &cx, countingDecrypt, countingCmdValid) == R::FRAME_READY && f.cmd == 0xC3u);
        CHECK(rd.TryFrame(f, false, &cx, countingDecrypt, countingCmdValid) == R::NEED_MORE);
        CHECK(cx.decryptCalls == 2 && cx.calls6 == 2 && !cx.sawOther);    // one header decrypt per frame
    }
}
int main() { test_codec(); test_legality(); test_challenge(); test_framereader();
    std::printf(g_fail? "FAILED (%d)\n":"OK\n", g_fail); return g_fail? 1:0; }
