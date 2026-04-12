/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2013, 2024-2026  Benoit Blancard <benblan@users.sourceforge.net>
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

#include "testcase.h"

#include "fs-kernel/model/map.h"
#include "fs-engine/gfx/tilemanager.h"


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

TEST_CASE( "Map::adjustClickOnRoad", "[kernel][map]" ) {
    MemoryTileManager tileMgr;
    fs_knl::Map map(&tileMgr, 1);
    // 5x5 map, 4 Z levels. All tiles default to kNone (index 0).
    REQUIRE( loadMapFromCsv(TEST_DATA_DIR "/map-5x5x4.csv", tileMgr, map) );

    // The method inspects the tile at (tx, ty, tz - 1).
    // We use tz=1 so the inspected Z is 0.
    const int tz = 1;

    SECTION( "returns false for a non-road tile" ) {
        // Default tile is kNone — not a road, not a ped crossing, not a road mark.
        fs_knl::TilePoint tilePt(1, 3, 3);
        REQUIRE_FALSE( map.adjustClickOnRoad(tilePt) );
        REQUIRE( tilePt.tx == 1 );
        REQUIRE( tilePt.ty == 3 );
    }

    SECTION( "returns true for a road tile without modifying position" ) {
        fs_knl::TilePoint tilePt(2, 2, tz);
        REQUIRE( map.adjustClickOnRoad(tilePt) );
        REQUIRE( tilePt.tx == 2 );
        REQUIRE( tilePt.ty == 2 );
    }

    SECTION( "pedestrian crossing" ) {
        SECTION( "returns true for kTileIdPedCrossNS" ) {
            fs_knl::TilePoint tilePt(2, 1, tz);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
        }

        SECTION( "returns true for kTileIdPedCrossEW" ) {
            fs_knl::TilePoint tilePt(1, 3, tz);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
        }
    }

    SECTION( "road mark separator NS (id 100)" ) {
        
        SECTION( "ox < 60: tx is decremented and returns true" ) {
            fs_knl::TilePoint tilePt(3, 2, tz, 30, 128, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.tx == 2 );
        }

        SECTION( "ox > 180: tx is incremented and returns true" ) {
            fs_knl::TilePoint tilePt(3, 2, tz, 200, 128, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.tx == 4 );
        }

        SECTION( "ox in [60, 180]: position unchanged and returns false" ) {
            fs_knl::TilePoint tilePt(3, 2, tz, 120, 128, 0);
            REQUIRE_FALSE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.tx == 3 );
        }
    }

    SECTION( "road mark separator end NS1 (id 114)" ) {
        
        SECTION( "ox < 60: tx is decremented and returns true" ) {
            fs_knl::TilePoint tilePt(3, 4, tz, 30, 128, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.tx == 2 );
        }

        SECTION( "ox > 180: tx is incremented and returns true" ) {
            fs_knl::TilePoint tilePt(3, 4, tz, 200, 128, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.tx == 4 );
        }
    }

    SECTION( "road mark separator end NS2 (id 116)" ) {
        
        SECTION( "ox < 60: tx is decremented and returns true" ) {
            fs_knl::TilePoint tilePt(1, 4, tz, 30, 128, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.tx == 0 );
        }

        SECTION( "ox > 180: tx is incremented and returns true" ) {
            fs_knl::TilePoint tilePt(1, 4, tz, 200, 128, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.tx == 2 );
        }
    }

    SECTION( "road mark separator EW (id 101)" ) {
        
        SECTION( "oy < 60: ty is decremented and returns true" ) {
            fs_knl::TilePoint tilePt(1, 2, tz, 128, 30, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.ty == 1 );
        }

        SECTION( "oy > 180: ty is incremented and returns true" ) {
            fs_knl::TilePoint tilePt(1, 2, tz, 128, 200, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.ty == 3 );
        }

        SECTION( "oy in [60, 180]: position unchanged and returns false" ) {
            fs_knl::TilePoint tilePt(1, 2, tz, 128, 120, 0);
            REQUIRE_FALSE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.ty == 2 );
        }
    }

    SECTION( "road mark separator end EW1 (id 115)" ) {
        
        SECTION( "oy < 60: ty is decremented and returns true" ) {
            fs_knl::TilePoint tilePt(3, 1, tz, 128, 30, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.ty == 0 );
        }

        SECTION( "oy > 180: ty is incremented and returns true" ) {
            fs_knl::TilePoint tilePt(3, 1, tz, 128, 200, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.ty == 2 );
        }
    }

    SECTION( "road mark separator end EW2 (id 117)" ) {
        //map.patchMap(tx, ty, tz - 1, fs_eng::Tile::kTileIdRoadMarkSeparatorEndEW2);

        SECTION( "oy < 60: ty is decremented and returns true" ) {
            fs_knl::TilePoint tilePt(3, 3, tz, 128, 30, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.ty == 2 );
        }

        SECTION( "oy > 180: ty is incremented and returns true" ) {
            fs_knl::TilePoint tilePt(3, 3, tz, 128, 200, 0);
            REQUIRE( map.adjustClickOnRoad(tilePt) );
            REQUIRE( tilePt.ty == 4 );
        }
    }
}