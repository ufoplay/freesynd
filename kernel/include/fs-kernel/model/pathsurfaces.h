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
        kSlopeSN   = 0x01,
        kSlopeNS   = 0x02,
        kSlopeEW   = 0x03,
        kSlopeWE   = 0x04,
        kGround    = 0x05,
        Type06   = 0x06,
        Type07   = 0x07,
        Type08   = 0x08,
        Type09   = 0x09,
        kWall    = 0x0A,
        kRoadCurve = 0x0B,
        Type0C   = 0x0C,
        kRoof    = 0x0D,
        kRoadPedCross = 0x0E,
        kRoadMark = 0x0F,
        kTrainStop = 0x10,
        Type11   = 0x11,
        Type12   = 0x12,
        Unknown
    };

    /*!
     * @brief Node in the 3-D tile grid used by the bidirectional flood-fill pathfinding algorithm.
     *
     * The mission allocates one FloodNode per map tile (index: x + y*mmax_x + z*mmax_m_xy).
     * During a path search, the algorithm expands outward simultaneously from the source tile
     * (base point) and the destination tile (target point), updating each node's flags,
     * directional masks, and expansion depth until the two fronts meet.
     *
     * @see NodeFlag, FloodTile, FloodLevelRange
     */
    class FloodNode {
    public:
        FloodNode() {
            flags = 0;
            dirsAbove = 0;
            dirsSame = 0;
            dirsBelow = 0;
            depth = 0;
        }

        /*!
         * @brief Bitfield describing the current state of this node in the flood-fill.
         *
         * Combines one or more NodeFlag values via bitwise OR. A value of kNone (0) means
         * the node has not been visited yet.
         */
        uint8_t flags;

        /*!
         * @brief Bitmask of walkable neighbour directions at level z+1 (one floor above).
         *
         * Each of the 8 bits encodes one cardinal or diagonal direction:
         * - 0x01 = S  (x,   y+1, z)   0x02 = SE (x+1, y+1, z)
         * - 0x04 = E  (x+1, y,   z)   0x08 = NE (x+1, y-1, z)
         * - 0x10 = N  (x,   y-1, z)   0x20 = NW (x-1, y-1, z)
         * - 0x40 = W  (x-1, y,   z)   0x80 = SW (x-1, y+1, z)
         *
         * Bits can be combined (e.g. 0x01|0x02). The same encoding applies to
         * dirsSame and dirsBelow.
         */
        uint8_t dirsAbove;
        //! Bitmask of walkable neighbour directions at the same level z. Same bit encoding as dirsAbove.
        uint8_t dirsSame;
        //! Bitmask of walkable neighbour directions at level z-1 (one floor below). Same bit encoding as dirsAbove.
        uint8_t dirsBelow;

        //! Flood-fill expansion depth. Zero means unvisited; incremented each time the front expands one step.
        unsigned short depth;

        /*!
         * @brief Returns true if dirsAbove contains the given direction bit(s).
         * @param bmDirection Bitmask of one or more directions to test (see dirsAbove for encoding).
         * @return true if all bits in bmDirection are set in dirsAbove.
         */
        bool isDirectionUpContains(uint8_t bmDirection) {
            return fs_utl::isBitsOnWithMask(dirsAbove, bmDirection);
        }

        /*!
         * @brief Returns true if dirsSame contains the given direction bit(s).
         * @param bmDirection Bitmask of one or more directions to test.
         * @return true if all bits in bmDirection are set in dirsSame.
         */
        bool isDirectionGroundContains(uint8_t bmDirection) {
            return fs_utl::isBitsOnWithMask(dirsSame, bmDirection);
        }

        /*!
         * @brief Returns true if dirsBelow contains the given direction bit(s).
         * @param bmDirection Bitmask of one or more directions to test.
         * @return true if all bits in bmDirection are set in dirsBelow.
         */
        bool isDirectionDownContains(uint8_t bmDirection) {
            return fs_utl::isBitsOnWithMask(dirsBelow, bmDirection);
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

    /*!
     * @brief Bitfield flags describing the state of a FloodNode during pathfinding.
     *
     * Values are designed to be combined with bitwise OR on FloodNode::flags.
     * A node starts at kNone (0) and transitions through these states as the
     * flood-fill algorithm classifies and expands across the map.
     *
     * @note kPending and kSafeWalk intentionally share value 0x40: kPending is used
     *       during the flood-fill queue phase, while kSafeWalk marks ground that is
     *       safe to walk on (no highway or railway), set alongside kWalkable once
     *       tile classification is complete.
     */
    enum NodeFlag {
        kNone        = 0,   //!< Initial state: node not yet visited.
        kBasePoint   = 1,   //!< Node is the flood-fill source (start tile).
        kTargetPoint = 2,   //!< Node is the flood-fill destination (goal tile).
        kLink        = 4,   //!< The two flood fronts have met at this node.
        kWalkable    = 8,   //!< Tile is passable by pedestrians.
        kConstant    = 16,  //!< Destination is permanent (not cleared between searches).
        kNonWalkable = 32,  //!< Tile is blocked (wall, void, obstacle).
        kPending     = 64,  //!< Node is queued for classification during flood-fill expansion.
        kSafeWalk    = 64   //!< Tile is safe to walk on (not a highway or railway). Set alongside kWalkable.
    };

    struct FloodTile {
        WorldPoint pos;
        FloodNode *pNode;
    };
    
    struct FloodLevelRange {
        uint16_t startIdx;
        uint16_t count;
    };

}

#endif

