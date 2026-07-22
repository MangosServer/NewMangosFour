/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * @file packet_builder.cpp
 * @brief Movement spline network packet construction
 *
 * This file implements the PacketBuilder class which constructs network
 * packets for movement spline data. Handles:
 *
 * - Monster movement packets (MSG_MONSTER_MOVE)
 * - Linear and Catmull-Rom spline paths
 * - Facing/targeting information
 * - Path compression and packing
 *
 * The packet format includes:
 * - Source position
 * - Spline flags and duration
 * - Path points (absolute or relative)
 * - Final facing/target information
 *
 * @see PacketBuilder for the builder class
 * @see MoveSpline for the movement spline data
 * @see SMSG_MONSTER_MOVE for the opcode
 */

#include "packet_builder.h"
#include "MoveSpline.h"
#include "Util.h"
#include "WorldPacket.h"
#include "../Object/Creature.h"

namespace Movement
{
    /**
     * @namespace Movement
     * @brief Movement system namespace
     *
     * Contains all movement-related classes and functions for
     * spline-based movement and packet construction.
     */
    /**
     * @brief Overloads the << operator to write a Vector3 to a ByteBuffer.
     * @param b The ByteBuffer to write to.
     * @param v The Vector3 to write.
     */
    inline void operator << (ByteBuffer& b, const Vector3& v)
    {
        b << v.x << v.y << v.z;
    }

    /**
     * @brief Overloads the >> operator to read a Vector3 from a ByteBuffer.
     * @param b The ByteBuffer to read from.
     * @param v The Vector3 to read.
     */
    inline void operator >> (ByteBuffer& b, Vector3& v)
    {
        b >> v.x >> v.y >> v.z;
    }

    static MonsterMoveType GetMonsterMoveType(MoveSplineFlag const& splineFlags)
    {
        switch (splineFlags & MoveSplineFlag::Mask_Final_Facing)
        {
            case MoveSplineFlag::Final_Target:
                return MonsterMoveFacingTarget;
            case MoveSplineFlag::Final_Angle:
                return MonsterMoveFacingAngle;
            case MoveSplineFlag::Final_Point:
                return MonsterMoveFacingSpot;
            default:
                return MonsterMoveNormal;
        }
    }

    void PacketBuilder::WriteMonsterMove(MonsterMoveData const& move, WorldPacket& data)
    {
        uint8 const type = move.type;
        ObjectGuid const moverGuid = move.moverGuid;
        ObjectGuid const transportGuid = move.transportGuid;
        uint32 const uncompressedSplineCount = uint32(move.uncompressedPath.size());
        uint32 const compressedSplineCount = uint32(move.compressedPath.size());

        data << float(move.position.z) << float(move.position.x) << uint32(move.splineId) << float(move.position.y);
        data << float(0.0f) << float(0.0f) << float(0.0f);

        data.WriteBit(!move.hasVerticalAcceleration);
        data.WriteGuidMask<0>(moverGuid);
        data.WriteBits(type, 3);
        if (type == MonsterMoveFacingTarget)
            data.WriteGuidMask<6, 4, 3, 0, 5, 7, 1, 2>(move.facingTargetGuid);
        data.WriteBit(true);
        data.WriteBit(true);
        data.WriteBit(move.transportSeat == -1);
        data.WriteBits(uncompressedSplineCount, 20);
        data.WriteBit(!move.splineFlags);
        data.WriteGuidMask<3>(moverGuid);
        data.WriteBit(true);
        data.WriteBit(true);
        data.WriteBit(true);
        data.WriteBit(!move.duration);
        data.WriteGuidMask<7, 4>(moverGuid);
        data.WriteBit(true);
        data.WriteGuidMask<5>(moverGuid);
        data.WriteBits(compressedSplineCount, 22);
        data.WriteGuidMask<6>(moverGuid);
        data.WriteBit(false);
        data.WriteGuidMask<7, 1, 3, 0, 6, 4, 5, 2>(transportGuid);
        data.WriteBit(false);
        data.WriteBit(false);
        data.WriteGuidMask<2, 1>(moverGuid);
        data.FlushBits();

        for (Vector3 const& offset : move.compressedPath)
            data.appendPackXYZ(offset.x, offset.y, offset.z);
        data.WriteGuidBytes<1>(moverGuid);
        data.WriteGuidBytes<6, 4, 1, 7, 0, 3, 5, 2>(transportGuid);
        for (Vector3 const& point : move.uncompressedPath)
            data << point.y << point.x << point.z;
        if (type == MonsterMoveFacingTarget)
            data.WriteGuidBytes<5, 7, 0, 4, 3, 2, 6, 1>(move.facingTargetGuid);
        data.WriteGuidBytes<5>(moverGuid);
        if (move.hasVerticalAcceleration)
            data << move.verticalAcceleration;
        if (type == MonsterMoveFacingAngle)
            data << float(NormalizeOrientation(move.facingAngle));
        data.WriteGuidBytes<3>(moverGuid);
        if (move.splineFlags)
            data << move.splineFlags;
        data.WriteGuidBytes<6>(moverGuid);
        if (type == MonsterMoveFacingSpot)
            data << move.facingPoint.x << move.facingPoint.y << move.facingPoint.z;
        data.WriteGuidBytes<0>(moverGuid);
        if (move.transportSeat != -1)
            data << move.transportSeat;
        data.WriteGuidBytes<7, 2, 4>(moverGuid);
        if (move.duration)
            data << move.duration;
    }

    void PacketBuilder::WriteStopMovement(Vector3 const& position, uint32 splineId, WorldPacket& data,
        ObjectGuid moverGuid, ObjectGuid transportGuid, int8 transportSeat)
    {
        MonsterMoveData move;
        move.position = position;
        move.splineId = splineId;
        move.type = MonsterMoveStop;
        move.moverGuid = moverGuid;
        move.transportGuid = transportGuid;
        move.transportSeat = transportSeat;
        WriteMonsterMove(move, data);
    }

    void PacketBuilder::WriteMonsterMove(const MoveSpline& move_spline, WorldPacket& data,
        ObjectGuid moverGuid, ObjectGuid transportGuid, int8 transportSeat)
    {
        MonsterMoveType const type = GetMonsterMoveType(move_spline.splineflags);
        Vector3 const& firstPoint = move_spline.spline.getPoint(move_spline.spline.first());
        MonsterMoveData move;
        move.position = firstPoint;
        move.splineId = move_spline.GetId();
        move.type = uint8(type);
        move.moverGuid = moverGuid;
        move.transportGuid = transportGuid;
        move.transportSeat = transportSeat;
        move.splineFlags = move_spline.splineflags.raw();
        move.duration = uint32(move_spline.Duration());
        move.hasVerticalAcceleration = move_spline.splineflags.parabolic;
        move.verticalAcceleration = move_spline.vertical_acceleration;
        switch (type)
        {
            case MonsterMoveFacingTarget:
                move.facingTargetGuid = ObjectGuid(move_spline.facing.target);
                break;
            case MonsterMoveFacingAngle:
                move.facingAngle = move_spline.facing.angle;
                break;
            case MonsterMoveFacingSpot:
                move.facingPoint = Vector3(move_spline.facing.f.x, move_spline.facing.f.y, move_spline.facing.f.z);
                break;
            default:
                break;
        }

        if (move_spline.splineflags & MoveSplineFlag::UncompressedPath)
        {
            int32 const end = move_spline.spline.getPointCount() - (move_spline.splineflags.cyclic ? 2 : 1);
            for (int32 i = 2; i < end; ++i)
                move.uncompressedPath.push_back(move_spline.spline.getPoint(i));
        }
        else
        {
            uint32 const lastIndex = move_spline.spline.getPointCount() - 3;
            Vector3 const* realPath = &move_spline.spline.getPoint(1);
            if (lastIndex > 1)
            {
                Vector3 const middle = (realPath[0] + realPath[lastIndex]) / 2.0f;
                for (uint32 i = 1; i < lastIndex; ++i)
                    move.compressedPath.push_back(middle - realPath[i]);
            }
            Vector3 const& destination = move_spline.spline.getPoint(move_spline.spline.getPointCount() - 2);
            move.uncompressedPath.push_back(destination);
        }
        WriteMonsterMove(move, data);
    }

    void PacketBuilder::WriteCreateBits(const MoveSpline& move_spline, ByteBuffer& data)
    {
        if (!data.WriteBit(!move_spline.Finalized()))
        {
            return;
        }

        uint32 nodes = move_spline.getPath().size();
        bool hasSplineStartTime = move_spline.splineflags & (MoveSplineFlag::Trajectory | MoveSplineFlag::Animation);
        bool hasSplineVerticalAcceleration = (move_spline.splineflags & MoveSplineFlag::Trajectory) && move_spline.effect_start_time < move_spline.Duration();

        data.WriteBits(uint8(move_spline.spline.mode()), 2);
        data.WriteBit(hasSplineStartTime);
        data.WriteBits(nodes, 22);

        switch (move_spline.splineflags & MoveSplineFlag::Mask_Final_Facing)
        {
            case MoveSplineFlag::Final_Target:
            {
                data.WriteBits(2, 2);

                data.WriteGuidMask<4, 3, 7, 2, 6, 1, 0, 5>(ObjectGuid(move_spline.facing.target));
                break;
            }
            case MoveSplineFlag::Final_Angle:
                data.WriteBits(0, 2);
                break;
            case MoveSplineFlag::Final_Point:
                data.WriteBits(1, 2);
                break;
            default:
                data.WriteBits(3, 2);
                break;
        }

        data.WriteBit(hasSplineVerticalAcceleration);
        data.WriteBits(move_spline.splineflags.raw(), 25);
    }

    void PacketBuilder::WriteCreateBytes(const MoveSpline& move_spline, ByteBuffer& data)
    {
        if (!move_spline.Finalized())
        {
            uint32 nodes = move_spline.getPath().size();
            bool hasSplineStartTime = move_spline.splineflags & (MoveSplineFlag::Trajectory | MoveSplineFlag::Animation);
            bool hasSplineVerticalAcceleration = (move_spline.splineflags & MoveSplineFlag::Trajectory) && move_spline.effect_start_time < move_spline.Duration();

            if (hasSplineVerticalAcceleration)
            {
                data << float(move_spline.vertical_acceleration);   // added in 3.1
            }

            data << int32(move_spline.timePassed());

            if (move_spline.splineflags & MoveSplineFlag::Final_Angle)
            {
                data << float(NormalizeOrientation(move_spline.facing.angle));
            }
            else if (move_spline.splineflags & MoveSplineFlag::Final_Target)
            {
                data.WriteGuidBytes<5, 3, 7, 1, 6, 4, 2, 0>(ObjectGuid(move_spline.facing.target));
            }

            for (uint32 i = 0; i < nodes; ++i)
            {
                data << float(move_spline.getPath()[i].z);
                data << float(move_spline.getPath()[i].x);
                data << float(move_spline.getPath()[i].y);
            }

            if (move_spline.splineflags & MoveSplineFlag::Final_Point)
            {
                data << float(move_spline.facing.f.x) << float(move_spline.facing.f.z) << float(move_spline.facing.f.y);
            }

            data << float(1.f);
            data << int32(move_spline.Duration());
            if (hasSplineStartTime)
            {
                data << int32(move_spline.effect_start_time);   // added in 3.1
            }

            data << float(1.f);
        }

        if (!move_spline.isCyclic())
        {
            Vector3 dest = move_spline.FinalDestination();
            data << float(dest.z);
            data << float(dest.x);
            data << float(dest.y);
        }
        else
        {
            data << Vector3::zero();
        }

        data << uint32(move_spline.GetId());
    }
}
