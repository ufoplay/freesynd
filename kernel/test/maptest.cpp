/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2013, 2024-2025  Benoit Blancard <benblan@users.sourceforge.net>
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
#include <catch2/catch_test_macros.hpp>

#include "fs-kernel/model/map.h"

TEST_CASE( "Map", "[kernel][map]" ) {
    fs_eng::TileManager tileMgr;
    fs_knl::Map map(&tileMgr, 0);

    map.setTiles(4, 5, 4, nullptr);

    SECTION( "mapDimensions") {
        SECTION( "should return map dimensions") {
            int maxTx, maxTy, maxTz;

            map.mapDimensions(&maxTx, &maxTy, &maxTz);

            REQUIRE( maxTx == 4);
            REQUIRE( maxTy == 5);
            REQUIRE( maxTz == 4);
        }
    }
}