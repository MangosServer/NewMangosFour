/**
 * Pins the four 5.4.8 server opcode names established by migration Wave 1.
 * Value and semantic-name provenance are recorded in
 * docs/opcode-migration/evidence/wave-1-foundation.md.
 */

#include "Opcodes.h"

#include <cstdint>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void CheckOpcode(std::uint32_t actual, std::uint32_t expected)
{
    CHECK(actual == expected);
    CHECK(actual <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    CheckOpcode(std::uint32_t(SMSG_SPELL_EXECUTE_LOG), 0x00D8u);
    CheckOpcode(std::uint32_t(SMSG_ATTACKSWING_ERROR), 0x11E1u);
    CheckOpcode(std::uint32_t(SMSG_RANDOM_ROLL), 0x141Au);
    CheckOpcode(std::uint32_t(SMSG_INSPECT_RATED_BG_STATS), 0x041Fu);

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_opcode_foundation: all checks passed\n");
    return 0;
}
