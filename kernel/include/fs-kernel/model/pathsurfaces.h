/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2010  Bohdan Stelmakh <chamel@users.sourceforge.net> 
 *   Copyright (C) 2025  Benoit Blancard <benblan@users.sourceforge.net>
 *
 *   This program is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>. 
 * 
 */

#ifndef PATHSURFACES_H
#define PATHSURFACES_H

#include "fs-utils/common.h"
#include "fs-kernel/model/position.h"

namespace fs_knl {
    enum class SurfaceType : uint8_t
    {
        Empty    = 0x00,
        Type01   = 0x01,
        Type02   = 0x02,
        Type03   = 0x03,
        Type04   = 0x04,
        Type05   = 0x05,
        Type06   = 0x06,
        Type07   = 0x07,
        Type08   = 0x08,
        Type09   = 0x09,
        Type0C   = 0x0C,
        Type10   = 0x10,
        Type11   = 0x11,
        Type12   = 0x12,
        Type10A   = 0x13,
        Type11A   = 0x14,
        Type12A   = 0x15,
        Type13   = 0x16,
        TypeSolidFlat = 0x17,   ///< Surface is flat and can be walked on
        Unknown
    };

    class floodPointDesc {
    public:
        floodPointDesc() {
            bfNodeDesc = 0;
            dirh = 0;
            dirm = 0;
            dirl = 0;
            lvl = 0;
            
        }
        /*! Description of the node.
         * If value is 0 - not defined,
         * Bitfield :
         * 0b - base point,
         * 1b - target point,
         * 2b - link (when base point reaches target point or vice versa),
         * 3b - walkable,
         * 4b - constant,
         * 5b - non walkable,
         * 6b - needs to be defined, after tiles are defined as walkable this flag
         *      will have meaning of safe walking ground (non-highway, non-railway
         *      tiles)
        * \see mapFloodDesc
        */
        uint8_t bfNodeDesc;
        // dirh(z + 1), dirm(z), dirl(z - 1) - directions
        // 0x01 = (x, y + 1, z); 0x02 = (x + 1, y + 1, z);
        // 0x04 = (x + 1, y, z); 0x08 = (x + 1, y - 1, z);
        // 0x10 = (x, y - 1, z); 0x20 = (x - 1, y - 1, z);
        // 0x40 = (x - 1, y, z); 0x80 = (x - 1, y + 1, z)
        // can be combined 0x01 | 0x02; 0x01 | 0x10 | 0x40 etc.
        uint8_t dirh;
        uint8_t dirm;
        uint8_t dirl;

        unsigned short lvl;

        bool isDirectionUpContains(uint8_t bmDirection) {
            return fs_utl::isBitsOnWithMask(dirh, bmDirection);
        }

        bool isDirectionGroundContains(uint8_t bmDirection) {
            return fs_utl::isBitsOnWithMask(dirm, bmDirection);
        }

        bool isDirectionDownContains(uint8_t bmDirection) {
            return fs_utl::isBitsOnWithMask(dirl, bmDirection);
        }

        //! In path finding, identify the direction to North
        static const uint8_t kBMaskDirNorth;
        //! In path finding, identify the direction to North-East
        static const uint8_t kBMaskDirNorthEast;
        //! In path finding, identify the direction to East
        static const uint8_t kBMaskDirEast;
        //! In path finding, identify the direction to South-East
        static const uint8_t kBMaskDirSouthEast;
        //! In path finding, identify the direction to South
        static const uint8_t kBMaskDirSouth;
        //! In path finding, identify the direction to South-West
        static const uint8_t kBMaskDirSouthWest;
        //! In path finding, identify the direction to West
        static const uint8_t kBMaskDirWest;
        //! In path finding, identify the direction to North-West
        static const uint8_t kBMaskDirNorthWest;
    };

    enum mapFloodDesc {
        m_fdNotDefined    = 0,
        m_fdBasePoint     = 1,
        m_fdTargetPoint   = 2,
        m_fdLink          = 4,
        m_fdWalkable      = 8,
        m_fdConstant      = 16,
        m_fdNonWalkable   = 32,
        m_fdDefReq        = 64,
        m_fdSafeWalk      = 64
    };

    struct toSetDesc {
        WorldPoint coords;
        floodPointDesc *pNode;
    };
    
    struct lvlNodesDesc {
        uint16_t indxs;
        uint16_t n;
    };

}

#endif

