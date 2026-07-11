/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2026  Benoit Blancard <benblan@users.sourceforge.net>
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

#include "fs-kernel/model/weapon.h"
#include "fs-engine/appcontext.h"

#include "testcase.h"

TEST_CASE( "WeaponInstance", "[kernel][weapon]" ) {
    fs_eng::AppContext appCtx;
    ConfigFile config;
    initWeaponConfigFile(config);
    fs_knl::Weapon energyClass(fs_knl::Weapon::EnergyShield, config);

    // weapon.13.auto.fire_rate=75, weapon.13.ammopershot=1, weapon.13.ammo.nb=200
    REQUIRE( energyClass.fireRate() == 75 );
    REQUIRE( energyClass.ammoPerShot() == 1 );

    SECTION( "consumeAmmoForEnergyShield" ) {
        SECTION( "Should not consume ammo before fireRate is elapsed" ) {
            fs_knl::WeaponInstance shield(&energyClass, 0, nullptr);

            REQUIRE_FALSE( shield.consumeAmmoForEnergyShield(50) );

            REQUIRE( shield.ammoRemaining() == 200 );
        }

        SECTION( "Should consume one shot of ammo once fireRate is elapsed" ) {
            fs_knl::WeaponInstance shield(&energyClass, 0, nullptr);

            shield.consumeAmmoForEnergyShield(50);
            REQUIRE_FALSE( shield.consumeAmmoForEnergyShield(30) );

            REQUIRE( shield.ammoRemaining() == 199 );
        }

        SECTION( "Should consume at most one shot of ammo per call, however large elapsed is" ) {
            fs_knl::WeaponInstance shield(&energyClass, 0, nullptr);

            REQUIRE_FALSE( shield.consumeAmmoForEnergyShield(1000) );

            REQUIRE( shield.ammoRemaining() == 199 );
        }

        SECTION( "Should return true once ammo is depleted" ) {
            fs_knl::WeaponInstance shield(&energyClass, 0, nullptr, 1);

            REQUIRE( shield.consumeAmmoForEnergyShield(80) );

            REQUIRE( shield.ammoRemaining() == 0 );
        }

        SECTION( "Should keep returning true and not go negative once ammo is depleted" ) {
            fs_knl::WeaponInstance shield(&energyClass, 0, nullptr, 1);
            shield.consumeAmmoForEnergyShield(80);

            REQUIRE( shield.consumeAmmoForEnergyShield(80) );

            REQUIRE( shield.ammoRemaining() == 0 );
        }
    }
}
