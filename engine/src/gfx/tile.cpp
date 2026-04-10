/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
 *   Copyright (C) 2024-2025  Benoit Blancard <benblan@users.sourceforge.net>
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

#include "fs-engine/gfx/tile.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

namespace fs_eng {

const int Tile::kTileWidth = 64;
const int Tile::kTileHeight = 48;
const int Tile::kSubTileWidth = 32;
const int Tile::kSubTileHeight = 16;


/*!
 * @brief 
 * @param id 
 * @param notAlpha 
 * @param type 
 * @param textLoc 
 */
Tile::Tile(int id, bool notAlpha, EType type, Point2D textLoc)
{
    id_ = id;
    type_ = type;
    notAlpha_ = notAlpha;
    textureLocation_ = textLoc;
}

/*! \brief
 *
 * @return uint8_t
 *
 */
uint8_t Tile::getWalkData() {
    // little patch to enable full surface description
    // and eliminate unnecessary data
    // 0x10 - non-surface/non-walkable, always above train stop
    // 0x11, 0x12 - train entering surface
    switch (id_) {
    case 0x80 :
        return 0x11;
    case 0x81 :
        return 0x12;
    case 0x8F :
        return 0x00;
    case 0x93 :
        return 0x00;
    default:
        // else return the type of the tile
        return type_;
    }
}

/*!
 * Returns a 8-bit bitmask encoding the valid entry/exit directions for this tile.
 * The bitmask is divided into two nibbles (4 bits each), the first for entries and
 * second for exits:
 * \code
 *   bits  7-4   Entries N S E W : if bit is 1, then a car can enter this side of the tile
 *   bits  3-0   Exits N S E W : if bit is 1, then a car can exit this side of the tile
 * \endcode
 * @return 0 when tile cannot be entered or exited
 */
uint8_t Tile::getEdgeConnexionsForRoadTile() {
    switch (id_) {             // Entry: NSEW Exit: NSEW
    case kTileLargeDoorRailEW: // Entry: 1100 Exit: 1100
        return 0xCC;
    case kTileLargeDoorRailNS: // Entry: 0011 Exit: 0011
        return 0x33;
    case kTileRoadNtoS :       // Entry: 1010 Exit: 0110
        return 0xA6;
    case kTileRoadStoN :       // Entry: 0101 Exit: 1001
        return 0x59;
    case kTileRoadWtoE :       // Entry: 1001 Exit: 1010
        return 0x9A;
    case kTileRoadEtoW :       // Entry: 0110 Exit: 0101
        return 0x65;
    case kTileCurveWtoS:       // Entry: 1001 Exit: 0110
        return 0x96;
    case kTileCurveNtoW:       // Entry: 1010 Exit: 0101
        return 0xA5;
    case kTileCurveStoE:       // Entry: 0101 Exit: 1010
        return 0x5A;
    case kTileCurveEtoN:       // Entry: 0110 Exit: 1001
        return 0x69;
    case kTileCurveNtoE:       // Entry: 1000 Exit: 0010
        return 0x82;
    case kTileCurveEtoS:       // Entry: 0010 Exit: 0100
        return 0x24;
    case kTileExtCurveStoW:    // Entry: 0100 Exit: 0001
        return 0x41;
    case kTileExtCurveWtoN:    // Entry: 0001 Exit: 1000
        return 0x18;
    case kTilePedCrossNS :     // Entry: 1100 Exit: 1100
        return 0xCC;
    case kTilePedCrossEW :     // Entry: 0011 Exit: 0011
        return 0x33;
    default:
        // else no connexion possible
        return 0;
    }
}

}

