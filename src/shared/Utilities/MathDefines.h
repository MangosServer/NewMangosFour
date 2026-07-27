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

#ifndef MANGOS_MATHDEFINES_H
#define MANGOS_MATHDEFINES_H

#include <cmath>

/**
 * Split out of Common.h so components that only need the math constants do not
 * have to pull in the whole shared header. ScriptDev3 includes this directly
 * since it stopped including Common.h.
 *
 * finiteAlways() deliberately stays in Common.h: this core's version is built
 * on finite() rather than std::isfinite(), and the save path uses it from
 * there. Defining it here as well would be a redefinition.
 */

// M_PI is POSIX, not ISO C++, so MSVC does not define it without _USE_MATH_DEFINES.
#ifndef M_PI
#  define M_PI          3.14159265358979323846
#endif

#ifndef M_PI_F
#  define M_PI_F        float(M_PI)
#endif

#endif
