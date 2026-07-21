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

#include "Player.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <cstdio>
#include <vector>

// RelWithDebInfo elides assert(); use a non-elidable check (mirrors mop_auth_test).
static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

namespace
{
    // Read n bits MSB-first from a WorldPacket's bit reader.
    uint32 ReadBits(WorldPacket& p, int n)
    {
        uint32 v = 0;
        for (int i = 0; i < n; ++i)
        {
            v = (v << 1) | (p.ReadBit() ? 1u : 0u);
        }
        return v;
    }

    // Decode a packet built by MopCharEnum::Build back into Entries and the header, mirroring
    // the documented order. Same-spec circularity is accepted (the live gate is the true oracle);
    // this catches order/width/packing regressions offline.
    struct DecodeHeader { uint32 ext; uint32 count; uint8 hasData; };

    DecodeHeader DecodePreambleAndEntries(WorldPacket& p, std::vector<MopCharEnum::Entry>& out)
    {
        p.rpos(0);
        DecodeHeader h;
        h.ext = ReadBits(p, 21);
        h.count = ReadBits(p, 16);

        // per-char bit block: capture presence masks + flags + name length
        std::vector<uint8> cMask(h.count * 8, 0), gMask(h.count * 8, 0), nameLen(h.count, 0);
        std::vector<uint8> flagA(h.count, 0), flagB(h.count, 0);
        for (uint32 i = 0; i < h.count; ++i)
        {
            uint8* cm = &cMask[i * 8];
            uint8* gm = &gMask[i * 8];
            gm[4] = p.ReadBit(); cm[0] = p.ReadBit(); gm[3] = p.ReadBit(); cm[3] = p.ReadBit();
            cm[7] = p.ReadBit(); flagA[i] = p.ReadBit(); flagB[i] = p.ReadBit(); cm[6] = p.ReadBit();
            gm[6] = p.ReadBit(); nameLen[i] = uint8(ReadBits(p, 6)); cm[1] = p.ReadBit();
            gm[1] = p.ReadBit(); gm[0] = p.ReadBit(); cm[4] = p.ReadBit(); gm[7] = p.ReadBit();
            cm[2] = p.ReadBit(); cm[5] = p.ReadBit(); gm[2] = p.ReadBit(); gm[5] = p.ReadBit();
        }
        h.hasData = p.ReadBit();
        p.ResetBitReader();   // byte-align the reader (mirror of FlushBits on the writer)

        out.assign(h.count, MopCharEnum::Entry());
        for (uint32 i = 0; i < h.count; ++i)
        {
            MopCharEnum::Entry& e = out[i];
            uint8* cm = &cMask[i * 8];
            uint8* gm = &gMask[i * 8];
            uint64 cg = 0, gg = 0;
            auto rdGuid = [&p](uint8* mask, uint64& g, int idx)
            {
                if (mask[idx]) { uint8 b; p >> b; g |= uint64(uint8(b ^ 1)) << (8 * idx); }
            };
            uint32 u; uint8 b8; float f;

            p >> u;                                   // op1  +132
            rdGuid(cm, cg, 1);                        // op2
            p >> e.slot;                              // op3  +57
            p >> e.hairStyle;                         // op4  +63
            rdGuid(gm, gg, 2);                        // op5
            rdGuid(gm, gg, 0);                        // op6
            rdGuid(gm, gg, 6);                        // op7
            e.name.resize(nameLen[i]);                // op8
            if (nameLen[i]) { p.read((uint8*)&e.name[0], nameLen[i]); }
            rdGuid(gm, gg, 3);                        // op9
            p >> e.posX;                              // op10
            p >> e.petFamily;                         // op11
            p >> e.face;                              // op12 +62
            p >> e.class_;                            // op13 +59
            rdGuid(gm, gg, 5);                        // op14
            for (int s = 0; s < 23; ++s)              // op15
            {
                p >> e.equipment[s].enchant;
                p >> e.equipment[s].invType;
                p >> e.equipment[s].displayId;
            }
            p >> e.customizeFlags;                    // op16 +100 -> customizeFlags (client reads customize here)
            rdGuid(cm, cg, 3);                        // op17
            rdGuid(cm, cg, 5);                        // op18
            p >> u;                                   // op19 +120
            rdGuid(gm, gg, 4);                        // op20
            p >> e.map;                               // op21 -> glue+60 = MAP (client's char-select preview reads mapId here)
            p >> e.race;                              // op22 +58
            p >> e.skin;                              // op23 +61
            rdGuid(gm, gg, 1);                        // op24
            p >> e.level;                             // op25 +66
            rdGuid(cm, cg, 0);                        // op26
            rdGuid(cm, cg, 2);                        // op27
            p >> e.hairColor;                         // op28 +64
            p >> e.gender;                            // op29 +60
            p >> e.facialHair;                        // op30 +65
            p >> e.petLevel;                          // op31 +116 -> petLevel
            rdGuid(cm, cg, 4);                        // op32
            rdGuid(cm, cg, 7);                        // op33
            p >> e.posY;                              // op34
            p >> e.petDisplayId;                      // op35 +112 -> petDisplayId
            p >> u;                                   // op36 +128
            rdGuid(cm, cg, 6);                        // op37
            p >> e.charFlags;                         // op38 +96 -> charFlags (client reads ghost/charFlags here)
            p >> e.zone;                              // op39 -> glue+68 = zone
            rdGuid(gm, gg, 7);                        // op40
            p >> e.posZ;                              // op41

            e.charGuid = cg;
            e.guildGuid = gg;
            e.flagFirstLogin = flagB[i] != 0;
            e.flagA = flagA[i] != 0;
        }
        return h;
    }

    MopCharEnum::Entry MakeEntry(uint64 cg, uint64 gg, const char* name, uint8 race, uint8 cls)
    {
        MopCharEnum::Entry e{};                   // aggregate value-init: empty string, zeroed PODs/array
        e.name = name;
        e.charGuid = cg; e.guildGuid = gg;
        e.race = race; e.class_ = cls; e.gender = 1;
        e.skin = 3; e.face = 4; e.hairStyle = 5; e.hairColor = 6; e.facialHair = 7;
        e.level = 85; e.slot = 2;
        e.zone = 1519; e.map = 0;
        e.posX = -8913.5f; e.posY = 554.6f; e.posZ = 93.7f;
        e.charFlags = 0x10; e.customizeFlags = 0; e.petDisplayId = 0; e.petLevel = 0; e.petFamily = 0;
        e.flagFirstLogin = false; e.flagA = false;
        for (int s = 0; s < 23; ++s) { e.equipment[s].displayId = s; e.equipment[s].invType = uint8(s); e.equipment[s].enchant = 0; }
        return e;
    }

    bool EntryEq(const MopCharEnum::Entry& a, const MopCharEnum::Entry& b)
    {
        return a.charGuid == b.charGuid && a.guildGuid == b.guildGuid && a.name == b.name &&
               a.race == b.race && a.class_ == b.class_ && a.gender == b.gender &&
               a.skin == b.skin && a.face == b.face && a.hairStyle == b.hairStyle &&
               a.hairColor == b.hairColor && a.facialHair == b.facialHair &&
               a.level == b.level && a.slot == b.slot && a.zone == b.zone && a.map == b.map &&
               a.charFlags == b.charFlags && a.customizeFlags == b.customizeFlags &&
               a.petDisplayId == b.petDisplayId && a.petLevel == b.petLevel && a.petFamily == b.petFamily &&
               a.flagFirstLogin == b.flagFirstLogin;
    }
}

// NOTE: linking 'game' drags in ACE (same as mop_auth_test.cpp), whose OS_main.h rewrites main() to
// ace_main_i() and requires the (int, char**) signature. A no-argument main() therefore fails to
// link (LNK2019).
int main(int /*argc*/, char** /*argv*/)
{
    // Test C: zero characters -> minimal 5-byte packet, count field 0.
    {
        WorldPacket p(SMSG_CHAR_ENUM, 8);
        MopCharEnum::Build(p, {});
        CHECK(p.size() == 5);                         // 21+16+1 = 38 bits -> 5 bytes
        std::vector<MopCharEnum::Entry> dec;
        DecodeHeader h = DecodePreambleAndEntries(p, dec);
        CHECK(h.ext == 0);
        CHECK(h.count == 0);
        CHECK(h.hasData == 1);   // empty list still reports success (valid response, count==0)
    }

    // Test A/D: round-trip two characters (one with a guild + zero GUID bytes, one without).
    {
        std::vector<MopCharEnum::Entry> in;
        in.push_back(MakeEntry(0x0000000100000005ULL, 0x0000000000000000ULL, "Arthas", 1, 6)); // no guild
        in.push_back(MakeEntry(0x00FF001200340078ULL, 0x0000000000009ABCULL, "Sy", 4, 8));      // zero-byte GUIDs + guild

        WorldPacket p(SMSG_CHAR_ENUM, 512);
        MopCharEnum::Build(p, in);

        std::vector<MopCharEnum::Entry> out;
        DecodeHeader h = DecodePreambleAndEntries(p, out);
        CHECK(h.ext == 0);
        CHECK(h.count == 2);
        CHECK(h.hasData == 1);
        CHECK(out.size() == 2);
        if (out.size() == 2)
        {
            CHECK(EntryEq(in[0], out[0]));
            CHECK(EntryEq(in[1], out[1]));
            CHECK(out[0].equipment[22].displayId == 22);
            CHECK(out[1].name == "Sy");
        }
        // Every byte consumed exactly (no over/under-read).
        CHECK(p.rpos() == p.size());
    }

    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURES\n", g_failures);
    return 1;
}
