/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * client 5.4.8 movement packet serialization.
 */

#include "Unit.h"
#include "Log.h"
#include "Opcodes.h"
#include "movement/MovementStructures.h"

void MovementInfo::Read(ByteBuffer& data, uint16 opcode)
{
    bool hasTransportData = false,
        hasMovementFlags = false,
        hasMovementFlags2 = false;

    MovementStatusElements* sequence = GetMovementStatusElementsSequence(opcode);
    if (!sequence)
    {
        sLog.outError("Unsupported MovementInfo::Read for 0x%X (%s)!", opcode, LookupOpcodeName(DIR_CLIENT, opcode));
        return;
    }

    for(uint32 i = 0; i < MSE_COUNT; ++i)
    {
        MovementStatusElements element = sequence[i];
        if (element == MSEEnd)
        {
            break;
        }

        if (element >= MSEGuidBit0 && element <= MSEGuidBit7)
        {
            guid[element - MSEGuidBit0] = data.ReadBit();
            continue;
        }

        if (element >= MSEGuid2Bit0 && element <= MSEGuid2Bit7)
        {
            guid2[element - MSEGuid2Bit0] = data.ReadBit();
            continue;
        }

        if (element >= MSETransportGuidBit0 && element <= MSETransportGuidBit7)
        {
            if (hasTransportData)
            {
                t_guid[element - MSETransportGuidBit0] = data.ReadBit();
            }
            continue;
        }

        if (element >= MSEGuidByte0 && element <= MSEGuidByte7)
        {
            if (guid[element - MSEGuidByte0])
            {
                guid[element - MSEGuidByte0] ^= data.ReadUInt8();
            }
            continue;
        }

        if (element >= MSEGuid2Byte0 && element <= MSEGuid2Byte7)
        {
            if (guid2[element - MSEGuid2Byte0])
            {
                guid2[element - MSEGuid2Byte0] ^= data.ReadUInt8();
            }
            continue;
        }

        if (element >= MSETransportGuidByte0 && element <= MSETransportGuidByte7)
        {
            if (hasTransportData && t_guid[element - MSETransportGuidByte0])
            {
                t_guid[element - MSETransportGuidByte0] ^= data.ReadUInt8();
            }
            continue;
        }

        switch (element)
        {
            case MSEFlags:
                if (hasMovementFlags)
                {
                    moveFlags = data.ReadBits(30);
                }
                break;
            case MSEFlags2:
                if (hasMovementFlags2)
                {
                    moveFlags2 = data.ReadBits(12);
                }
                break;
            case MSEFlags2_13:
                if (hasMovementFlags2)
                {
                    moveFlags2 = data.ReadBits(13);
                }
                break;
            case MSEHasUnknownBit:
                data.ReadBit();
                break;
            case MSEUnknownBit148:
                unknownBit148 = data.ReadBit();
                break;
            case MSEUnknownBit149:
                unknownBit149 = data.ReadBit();
                break;
            case MSEUnknownBit172:
                unknownBit172 = data.ReadBit();
                break;
            case MSEHasUnknownUInt32:
                hasUnknownUInt32 = !data.ReadBit();
                break;
            case MSETimestamp:
                if (si.hasTimeStamp)
                {
                    data >> time;
                }
                break;
            case MSEHasTimestamp:
                si.hasTimeStamp = !data.ReadBit();
                break;
            case MSEHasOrientation:
                si.hasOrientation = !data.ReadBit();
                break;
            case MSEHasMovementFlags:
                hasMovementFlags = !data.ReadBit();
                break;
            case MSEHasMovementFlags2:
                hasMovementFlags2 = !data.ReadBit();
                break;
            case MSEHasPitch:
                si.hasPitch = !data.ReadBit();
                break;
            case MSEHasFallData:
                si.hasFallData = data.ReadBit();
                break;
            case MSEHasFallDirection:
                if (si.hasFallData)
                {
                    si.hasFallDirection = data.ReadBit();
                }
                break;
            case MSEHasTransportData:
                hasTransportData = data.ReadBit();
                break;
            case MSEHasTransportTime2:
                if (hasTransportData)
                {
                    si.hasTransportTime2 = data.ReadBit();
                }
                break;
            case MSEHasTransportTime3:
                if (hasTransportData)
                {
                    si.hasTransportTime3 = data.ReadBit();
                }
                break;
            case MSEHasSpline:
                si.hasSpline = data.ReadBit();
                break;
            case MSEHasSplineElevation:
                si.hasSplineElevation = !data.ReadBit();
                break;
            case MSEPositionX:
                data >> pos.x;
                break;
            case MSEPositionY:
                data >> pos.y;
                break;
            case MSEPositionZ:
                data >> pos.z;
                break;
            case MSEPositionO:
                if (si.hasOrientation)
                {
                    data >> pos.o;
                }
                break;
            case MSEPitch:
                if (si.hasPitch)
                {
                    data >> s_pitch;
                }
                break;;
            case MSEFallTime:
                if (si.hasFallData)
                {
                    data >> fallTime;
                }
                break;
            case MSESplineElevation:
                if (si.hasSplineElevation)
                {
                    data >> splineElevation;
                }
                break;
            case MSEFallHorizontalSpeed:
                if (si.hasFallData && si.hasFallDirection)
                {
                    data >> jump.xyspeed;
                }
                break;
            case MSEFallVerticalSpeed:
                if (si.hasFallData)
                {
                    data >> jump.velocity;
                }
                break;
            case MSEFallCosAngle:
                if (si.hasFallData && si.hasFallDirection)
                {
                    data >> jump.cosAngle;
                }
                break;
            case MSEFallSinAngle:
                if (si.hasFallData && si.hasFallDirection)
                {
                    data >> jump.sinAngle;
                }
                break;
            case MSETransportSeat:
                if (hasTransportData)
                {
                    data >> t_seat;
                }
                break;
            case MSETransportPositionO:
                if (hasTransportData)
                {
                    data >> t_pos.o;
                }
                break;
            case MSETransportPositionX:
                if (hasTransportData)
                {
                    data >> t_pos.x;
                }
                break;
            case MSETransportPositionY:
                if (hasTransportData)
                {
                    data >> t_pos.y;
                }
                break;
            case MSETransportPositionZ:
                if (hasTransportData)
                {
                    data >> t_pos.z;
                }
                break;
            case MSETransportTime:
                if (hasTransportData)
                {
                    data >> t_time;
                }
                break;
            case MSETransportTime2:
                if (hasTransportData && si.hasTransportTime2)
                {
                    data >> t_time2;
                }
                break;
            case MSETransportTime3:
                if (hasTransportData && si.hasTransportTime3)
                {
                    data >> t_time3;
                }
                break;
            case MSEMovementCounter:
                data.read_skip<uint32>();
                break;
            case MSEMovementForceCount:
                movementForceCount = data.ReadBits(22);
                break;
            case MSEMovementForceIds:
                if (movementForceCount > (data.size() - data.rpos()) / sizeof(uint32))
                {
                    throw ByteBufferException(false, data.rpos(), movementForceCount * sizeof(uint32), data.size());
                }
                movementForceIds.resize(movementForceCount);
                for (uint32& movementForceId : movementForceIds)
                {
                    data >> movementForceId;
                }
                break;
            case MSEUnknownUInt32:
                if (hasUnknownUInt32)
                {
                    data >> unknownUInt32;
                }
                break;
            case MSEByteParam:
                 data >> byteParam;
                 break;
            default:
                MANGOS_ASSERT(false && "Wrong movement status element");
                break;
        }
    }
}

/**
 * @brief Serializes movement information into a packet buffer.
 *
 * @param data The packet buffer to populate.
 */
void MovementInfo::Write(ByteBuffer& data, uint16 opcode) const
{
    bool hasTransportData = !t_guid.IsEmpty();

    MovementStatusElements* sequence = GetMovementStatusElementsSequence(opcode);
    if (!sequence)
    {
        sLog.outError("Unsupported MovementInfo::Write for 0x%X (%s)!", opcode, LookupOpcodeName(DIR_SERVER, opcode));
        return;
    }

    for(uint32 i = 0; i < MSE_COUNT; ++i)
    {
        MovementStatusElements element = sequence[i];

        if (element == MSEEnd)
        {
            break;
        }

        if (element >= MSEGuidBit0 && element <= MSEGuidBit7)
        {
            data.WriteBit(guid[element - MSEGuidBit0]);
            continue;
        }

        if (element >= MSETransportGuidBit0 && element <= MSETransportGuidBit7)
        {
            if (hasTransportData)
            {
                data.WriteBit(t_guid[element - MSETransportGuidBit0]);
            }
            continue;
        }

        if (element >= MSEGuidByte0 && element <= MSEGuidByte7)
        {
            if (guid[element - MSEGuidByte0])
            {
                data << uint8((guid[element - MSEGuidByte0] ^ 1));
            }
            continue;
        }

        if (element >= MSETransportGuidByte0 && element <= MSETransportGuidByte7)
        {
            if (hasTransportData && t_guid[element - MSETransportGuidByte0])
            {
                data << uint8((t_guid[element - MSETransportGuidByte0] ^ 1));
            }
            continue;
        }

        switch (element)
        {
            case MSEHasMovementFlags:
                data.WriteBit(!moveFlags);
                break;
            case MSEHasMovementFlags2:
                data.WriteBit(!moveFlags2);
                break;
            case MSEFlags:
                if (moveFlags)
                {
                    data.WriteBits(moveFlags, 30);
                }
                break;
            case MSEFlags2:
                if (moveFlags2)
                {
                    data.WriteBits(moveFlags2, 12);
                }
                break;
            case MSEFlags2_13:
                if (moveFlags2)
                {
                    data.WriteBits(moveFlags2, 13);
                }
                break;
            case MSETimestamp:
                if (si.hasTimeStamp)
                {
                    data << uint32(time);
                }
                break;
            case MSEHasPitch:
                data.WriteBit(!si.hasPitch);
                break;
            case MSEHasTimestamp:
                data.WriteBit(!si.hasTimeStamp);
                break;
            case MSEHasUnknownBit:
                data.WriteBit(false);
                break;
            case MSEUnknownBit148:
                data.WriteBit(unknownBit148);
                break;
            case MSEUnknownBit149:
                data.WriteBit(unknownBit149);
                break;
            case MSEUnknownBit172:
                data.WriteBit(unknownBit172);
                break;
            case MSEHasUnknownUInt32:
                data.WriteBit(!hasUnknownUInt32);
                break;
            case MSEHasFallData:
                data.WriteBit(si.hasFallData);
                break;
            case MSEHasFallDirection:
                if (si.hasFallData)
                {
                    data.WriteBit(si.hasFallDirection);
                }
                break;
            case MSEHasTransportData:
                data.WriteBit(hasTransportData);
                break;
            case MSEHasTransportTime2:
                if (hasTransportData)
                {
                    data.WriteBit(si.hasTransportTime2);
                }
                break;
            case MSEHasTransportTime3:
                if (hasTransportData)
                {
                    data.WriteBit(si.hasTransportTime3);
                }
                break;
            case MSEHasSpline:
                data.WriteBit(si.hasSpline);
                break;
            case MSEHasSplineElevation:
                data.WriteBit(!si.hasSplineElevation);
                break;
            case MSEPositionX:
                data << float(pos.x);
                break;
            case MSEPositionY:
                data << float(pos.y);
                break;
            case MSEPositionZ:
                data << float(pos.z);
                break;
            case MSEPositionO:
                if (si.hasOrientation)
                {
                    data << float(NormalizeOrientation(pos.o));
                }
                break;
            case MSEPitch:
                if (si.hasPitch)
                {
                    data << float(s_pitch);
                }
                break;
            case MSEHasOrientation:
                data.WriteBit(!si.hasOrientation);
                break;
            case MSEFallTime:
                if (si.hasFallData)
                {
                    data << uint32(fallTime);
                }
                break;
            case MSESplineElevation:
                if (si.hasSplineElevation)
                {
                    data << float(splineElevation);
                }
                break;
            case MSEFallHorizontalSpeed:
                if (si.hasFallData && si.hasFallDirection)
                {
                    data << float(jump.xyspeed);
                }
                break;
            case MSEFallVerticalSpeed:
                if (si.hasFallData)
                {
                    data << float(jump.velocity);
                }
                break;
            case MSEFallCosAngle:
                if (si.hasFallData && si.hasFallDirection)
                {
                    data << float(jump.cosAngle);
                }
                break;
            case MSEFallSinAngle:
                if (si.hasFallData && si.hasFallDirection)
                {
                    data << float(jump.sinAngle);
                }
                break;
            case MSETransportSeat:
                if (hasTransportData)
                {
                    data << int8(t_seat);
                }
                break;
            case MSETransportPositionO:
                if (hasTransportData)
                {
                    data << float(NormalizeOrientation(t_pos.o));
                }
                break;
            case MSETransportPositionX:
                if (hasTransportData)
                {
                    data << float(t_pos.x);
                }
                break;
            case MSETransportPositionY:
                if (hasTransportData)
                {
                    data << float(t_pos.y);
                }
                break;
            case MSETransportPositionZ:
                if (hasTransportData)
                {
                    data << float(t_pos.z);
                }
                break;
            case MSETransportTime:
                if (hasTransportData)
                {
                    data << uint32(t_time);
                }
                break;
            case MSETransportTime2:
                if (hasTransportData && si.hasTransportTime2)
                {
                    data << uint32(t_time2);
                }
                break;
            case MSETransportTime3:
                if (hasTransportData && si.hasTransportTime3)
                {
                    data << uint32(t_time3);
                }
                break;
            case MSEMovementCounter:
                data << uint32(0);
                break;
            case MSEMovementForceCount:
                MANGOS_ASSERT(movementForceIds.size() < (1u << 22));
                data.WriteBits(movementForceIds.size(), 22);
                break;
            case MSEMovementForceIds:
                for (uint32 movementForceId : movementForceIds)
                {
                    data << movementForceId;
                }
                break;
            case MSEUnknownUInt32:
                if (hasUnknownUInt32)
                {
                    data << unknownUInt32;
                }
                break;
            default:
                MANGOS_ASSERT(false && "Wrong movement status element");
                break;
        }
    }
}
