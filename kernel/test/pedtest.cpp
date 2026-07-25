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

#include "fs-kernel/model/ped.h"
#include "testcase.h"
#include "fs-engine/appcontext.h"

TEST_CASE( "Ped", "[kernel][ped]" ) {
    fs_eng::AppContext appCtx;
    ConfigFile config;
    initWeaponConfigFile(config);

    fs_knl::PedInstance cut(1, nullptr, fs_knl::PedInstance::kPedTypeAgent, true, 50);
    cut.setStartHealth(10, true);

    SECTION( "State management") {
        cut.goToState(fs_knl::kPedActionStateWalking);
        REQUIRE( cut.isState(fs_knl::kPedActionStateWalking) );
    }

    SECTION( "Firing state combines with current standing/walking state") {
        cut.goToState(fs_knl::kPedActionStateWalking);
        cut.goToState(fs_knl::kPedActionStateFiring);
        REQUIRE( cut.isState(fs_knl::kPedActionStateWalkingFiring) );

        // Stopping while firing keeps firing active
        cut.leaveState(fs_knl::kPedActionStateWalking);
        REQUIRE( cut.isState(fs_knl::kPedActionStateStandingFiring) );

        cut.leaveState(fs_knl::kPedActionStateFiring);
        REQUIRE( cut.isState(fs_knl::kPedActionStateStanding) );
    }

    SECTION("Mods") {
        SECTION ("Speed should be default with no mods and no load") {
            cut.setSpeedToMax();
            REQUIRE( cut.speed() == 50 );
        }

        SECTION ("Speed should be modified with no mods and overload") {
            fs_knl::Weapon shieldClass(fs_knl::Weapon::EnergyShield, config);
            fs_knl::Weapon gaussClass(fs_knl::Weapon::GaussGun, config);
            fs_knl::WeaponInstance energyShield(&shieldClass, 0, nullptr);
            fs_knl::WeaponInstance gauss(&gaussClass, 1, nullptr);

            // inventory is only above max weight
            cut.addWeapon(&energyShield);
            cut.setSpeedToMax();
            REQUIRE( cut.speed() == (50 / 2) );

            // Inventory is now more than double max weight
            cut.addWeapon(&gauss);
            cut.setSpeedToMax();
            REQUIRE( cut.speed() == fs_knl::PedInstance::kAgentMaxSpeedWithOverweight );
        }

        SECTION ("Speed should be higher with Leg Mod and no load") {
            fs_knl::Mod legV1("LegV1", fs_knl::Mod::MOD_LEGS, fs_knl::Mod::MOD_V1, 0, "", 0);
            fs_knl::Mod legV2("LegV2", fs_knl::Mod::MOD_LEGS, fs_knl::Mod::MOD_V2, 0, "", 0);
            fs_knl::Mod legV3("LegV3", fs_knl::Mod::MOD_LEGS, fs_knl::Mod::MOD_V3, 0, "", 0);

            cut.addMod(&legV1);
            cut.setSpeedToMax();
            REQUIRE( cut.speed() == 62 );

            cut.addMod(&legV2);
            cut.setSpeedToMax();
            REQUIRE( cut.speed() == 75 );

            cut.addMod(&legV3);
            cut.setSpeedToMax();
            REQUIRE( cut.speed() == 87 );
        }

        SECTION ("Damage should be reduced with Chest Mod") {
            // health is clamped to 255, so keep the damage values well below that
            cut.setStartHealth(200, true);

            fs_knl::Mod chestV1("ChestV1", fs_knl::Mod::MOD_CHEST, fs_knl::Mod::MOD_V1, 0, "", 0);
            fs_knl::Mod chestV2("ChestV2", fs_knl::Mod::MOD_CHEST, fs_knl::Mod::MOD_V2, 0, "", 0);
            fs_knl::Mod chestV3("ChestV3", fs_knl::Mod::MOD_CHEST, fs_knl::Mod::MOD_V3, 0, "", 0);

            fs_knl::DamageToInflict damage;
            damage.dtype = fs_knl::kDmgTypeBullet;
            damage.dvalue = 40;
            damage.d_owner = nullptr;
            damage.pWeapon = nullptr;

            int expectedHealth = cut.startHealth();

            cut.addMod(&chestV1);
            cut.takeDamage(damage);
            expectedHealth -= static_cast<int>(40.f * 0.9f);
            REQUIRE( cut.health() == expectedHealth );

            cut.addMod(&chestV2);
            cut.takeDamage(damage);
            expectedHealth -= static_cast<int>(40.f * 0.75f);
            REQUIRE( cut.health() == expectedHealth );

            cut.addMod(&chestV3);
            cut.takeDamage(damage);
            expectedHealth -= static_cast<int>(40.f * 0.6f);
            REQUIRE( cut.health() == expectedHealth );

            // Persuasion damage is not reduced by the Chest mod
            damage.dtype = fs_knl::kDmgTypePersuasion;
            cut.takeDamage(damage);
            expectedHealth -= 40;
            REQUIRE( cut.health() == expectedHealth );
        }
    }
}