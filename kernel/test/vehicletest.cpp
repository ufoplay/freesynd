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

#include "fs-kernel/model/vehicle.h"

TEST_CASE( "Vehicle", "[kernel][vehicle]" ) {
    fs_knl::GenericCar cut(1, fs_knl::Vehicle::kVehicleTypeRegularCar, nullptr, 100);

    SECTION( "Should set animations with right offset") {
        // Test is not complete
        cut.setAnimations(2);
        REQUIRE( cut.regularAnimation() == 0 );
        REQUIRE( cut.burntAnimation() == 2 );
    }
}

/*!
 * Test subclass that removes external dependencies from doMove():
 * - overrides checkForBlockers() so it never needs g_missionCtrl
 * - exposes addPathPoint() to push directly into the protected dest_path_
 */
class TestGenericCar : public fs_knl::GenericCar {
public:
    explicit TestGenericCar(int maxSpeed = 100)
        : fs_knl::GenericCar(1, fs_knl::Vehicle::kVehicleTypeRegularCar, nullptr, maxSpeed) {}

    void addPathPoint(const fs_knl::TilePoint &pt) {
        dest_path_.push_back(pt);
    }

    bool checkForBlockers([[maybe_unused]] bool checkForCrossings) override {
        return false;
    }
};

TEST_CASE( "GenericCar::doMove", "[kernel][vehicle]" ) {

    SECTION( "returns false when path is empty" ) {
        TestGenericCar cut;
        REQUIRE( !cut.doMove(100) );
        REQUIRE( !cut.hasDestination() );
    }

    SECTION( "snaps to waypoint when already within arrival threshold" ) {
        // diffx = 6, diffy = 0 — both below the 16-unit threshold,
        // so the movement branch is skipped and the car snaps directly.
        TestGenericCar cut;
        cut.setPosition(5, 5, 2, 128, 128, 0);
        cut.setSpeedToMax();
        cut.addPathPoint(fs_knl::TilePoint(5, 5, 2, 134, 128, 0));

        REQUIRE( cut.doMove(100) );
        REQUIRE( !cut.hasDestination() );
        REQUIRE( !cut.isMoving() );
        REQUIRE( cut.tileX() == 5 );
        REQUIRE( cut.tileY() == 5 );
        REQUIRE( cut.offX() == 134 );
        REQUIRE( cut.offY() == 128 );
    }

    SECTION( "moves toward distant waypoint within one tick" ) {
        // diffx = 256, speed = 100 units/s, elapsed = 100 ms
        // dx = (256 * 100 * 100 / 256) / 1000 = 10 units moved
        TestGenericCar cut(100);
        cut.setPosition(5, 5, 2, 128, 128, 0);
        cut.setSpeedToMax();
        cut.addPathPoint(fs_knl::TilePoint(6, 5, 2, 128, 128, 0));

        REQUIRE( cut.doMove(100) );
        REQUIRE( cut.hasDestination() );   // waypoint not yet reached
        REQUIRE( cut.tileX() == 5 );       // still on the same tile
        REQUIRE( cut.offX() == 138 );      // 128 + 10
    }

    SECTION( "reaches waypoint and clears path when elapsed time is sufficient" ) {
        // diffx = 256, speed = 100, full travel time = 2560 ms
        // elapsed 10000 ms covers it entirely: car lands exactly on the waypoint,
        // triggers the arrival check, snaps, pops the path, and stops.
        TestGenericCar cut(100);
        cut.setPosition(5, 5, 2, 128, 128, 0);
        cut.setSpeedToMax();
        cut.addPathPoint(fs_knl::TilePoint(6, 5, 2, 128, 128, 0));

        REQUIRE( cut.doMove(10000) );
        REQUIRE( !cut.hasDestination() );
        REQUIRE( !cut.isMoving() );
        REQUIRE( cut.tileX() == 6 );
        REQUIRE( cut.offX() == 128 );
    }

    SECTION( "traverses multiple waypoints when elapsed time covers the whole path" ) {
        TestGenericCar cut(100);
        cut.setPosition(5, 5, 2, 128, 128, 0);
        cut.setSpeedToMax();
        cut.addPathPoint(fs_knl::TilePoint(6, 5, 2, 128, 128, 0));
        cut.addPathPoint(fs_knl::TilePoint(7, 5, 2, 128, 128, 0));

        REQUIRE( cut.doMove(100000) );
        REQUIRE( !cut.hasDestination() );
        REQUIRE( !cut.isMoving() );
        REQUIRE( cut.tileX() == 7 );
        REQUIRE( cut.offX() == 128 );
    }
}
