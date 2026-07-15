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
// test_framereader() -> Task 2.5
int main() { test_codec(); test_legality(); test_challenge(); /* test_framereader(); */
    std::printf(g_fail? "FAILED (%d)\n":"OK\n", g_fail); return g_fail? 1:0; }
