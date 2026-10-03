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
#include <catch2/catch_approx.hpp>

#include "fs-kernel/model/ped.h"
#include "testcase.h"
#include "fs-engine/appcontext.h"

/*!
 * @brief Advances the IPA levels of a ped by the given duration.
 * Time is fed one millisecond at a time, like a sequence of very short frames.
 * @param ped The ped whose IPA levels are updated
 * @param durationMs The duration in milliseconds
 */
static void elapseIPATime(fs_knl::PedInstance &ped, uint32_t durationMs) {
    for (uint32_t i = 0; i < durationMs; ++i) {
        ped.updateAllIPA(1);
    }
}

/*!
 * @brief Sets the ped's Adrenaline to x0.5 (amount 0, dependency 100).
 * Dependency 100 cannot be loaded from the 0-255 data range, so the amount
 * is held at 100 until the dependency creeps up to it.
 * @param ped The ped whose Adrenaline is set
 */
static void setLowestAdrenaline(fs_knl::PedInstance &ped) {
    ped.initAllLevelsForIPAType(IPAStim::Adrenaline, 255, 255, 255);
    for (int i = 0; i < 200 && ped.adrenaline().dependency() < 100; ++i) {
        ped.setIPAAmount(IPAStim::Adrenaline, 100);
        elapseIPATime(ped, 100);
    }
    REQUIRE( ped.adrenaline().dependency() == 100 );
    ped.setIPAAmount(IPAStim::Adrenaline, 0);
}

TEST_CASE( "Ped", "[kernel][ped]" ) {
    fs_eng::AppContext appCtx;
    ConfigFile config;
    initWeaponConfigFile(config);

    fs_knl::PedInstance cut(1, nullptr, fs_knl::PedInstance::kPedTypeAgent, true, 50);
    cut.setStartHealth(10, true);

    fs_knl::PedInstance civilian(2, nullptr, fs_knl::PedInstance::kPedTypeCivilian, false, 50);
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

    SECTION("IPA levels") {
        // A new ped starts with amount and dependency at 50 (neutral), but
        // effect at 0; in game, levels are always loaded from mission data.
        // Levels given to initAllLevelsForIPAType use the 0-255 range of the
        // original data files: 0 -> 0, 128 -> 50, 192 -> 75.

        // Weapon used to check the time between shots
        fs_knl::Weapon pistolClass(fs_knl::Weapon::Pistol, config);
        fs_knl::WeaponInstance pistol(&pistolClass, 0, nullptr);
        const int reloadTime = pistolClass.reloadTime();

        SECTION ("IPA multiplier depends on the gap between amount and dependency") {
            REQUIRE( cut.adrenaline().getMultiplier() == Catch::Approx(1.0) );

            // Full amount with a neutral dependency gives only x1.5
            cut.setIPAAmount(IPAStim::Adrenaline, 100);
            REQUIRE( cut.adrenaline().getMultiplier() == Catch::Approx(1.5) );

            // x2 needs the dependency to be at its lowest
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 0, 0, 0);
            cut.setIPAAmount(IPAStim::Adrenaline, 100);
            REQUIRE( cut.adrenaline().getMultiplier() == Catch::Approx(2.0) );

            // An amount below dependency reduces down to x0.5
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 128, 128, 128);
            cut.setIPAAmount(IPAStim::Adrenaline, 0);
            REQUIRE( cut.adrenaline().getMultiplier() == Catch::Approx(1.0 / 1.5) );

            // The same curve applies to all three IPA levels
            cut.setIPAAmount(IPAStim::Perception, 100);
            cut.setIPAAmount(IPAStim::Intelligence, 100);
            REQUIRE( cut.perception().getMultiplier() == Catch::Approx(1.5) );
            REQUIRE( cut.intelligence().getMultiplier() == Catch::Approx(1.5) );
        }

        SECTION ("Effect catches up with amount, then amount drifts toward dependency") {
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 128, 128, 128);
            cut.setIPAAmount(IPAStim::Adrenaline, 60);

            // The effect timer fires once more than 1 s has elapsed
            elapseIPATime(cut, 1000);
            REQUIRE( cut.adrenaline().effect() == 50 );
            elapseIPATime(cut, 1);
            REQUIRE( cut.adrenaline().effect() == 51 );
            REQUIRE( cut.adrenaline().amount() == 60 );

            // After 10 effect ticks, effect has reached the amount.
            // Meanwhile dependency has moved twice (at 4.5 s and 9 s)
            elapseIPATime(cut, 9 * 1001);
            REQUIRE( cut.adrenaline().effect() == 60 );
            REQUIRE( cut.adrenaline().amount() == 60 );
            REQUIRE( cut.adrenaline().dependency() == 52 );

            // Then amount goes down toward dependency, effect stuck to it
            elapseIPATime(cut, 1001);
            REQUIRE( cut.adrenaline().amount() == 59 );
            REQUIRE( cut.adrenaline().effect() == 59 );
            elapseIPATime(cut, 1001);
            REQUIRE( cut.adrenaline().amount() == 58 );
            REQUIRE( cut.adrenaline().effect() == 58 );
            REQUIRE( cut.adrenaline().dependency() == 52 );
        }

        SECTION ("Dependency moves toward amount every 4.5 s") {
            cut.setIPAAmount(IPAStim::Adrenaline, 100);

            elapseIPATime(cut, 4500);
            REQUIRE( cut.adrenaline().dependency() == 50 );
            elapseIPATime(cut, 1);
            REQUIRE( cut.adrenaline().dependency() == 51 );
            elapseIPATime(cut, 4501);
            REQUIRE( cut.adrenaline().dependency() == 52 );
        }

        SECTION ("Amount and dependency drift together back to neutral once equal") {
            // Below neutral (Adrenaline): both go up
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 0, 0, 0);
            // Above neutral (Perception): both go down
            cut.initAllLevelsForIPAType(IPAStim::Perception, 192, 192, 192);
            REQUIRE( cut.perception().amount() == 75 );

            elapseIPATime(cut, 4501);
            REQUIRE( cut.adrenaline().amount() == 1 );
            REQUIRE( cut.adrenaline().dependency() == 1 );
            REQUIRE( cut.perception().amount() == 74 );
            REQUIRE( cut.perception().dependency() == 74 );

            elapseIPATime(cut, 4501);
            REQUIRE( cut.adrenaline().amount() == 2 );
            REQUIRE( cut.adrenaline().dependency() == 2 );
            REQUIRE( cut.perception().amount() == 73 );
            REQUIRE( cut.perception().dependency() == 73 );
        }

        SECTION ("Adrenaline changes agent speed") {
            // IPA effects apply only to peds in the agent group
            cut.setObjGroupDef(fs_knl::PedInstance::og_dmAgent);

            cut.setSpeedToMax();
            REQUIRE( cut.speed() == 50 );

            cut.setIPAAmount(IPAStim::Adrenaline, 100);
            cut.setSpeedToMax();
            REQUIRE( cut.speed() == 75 );

            cut.setIPAAmount(IPAStim::Adrenaline, 0);
            cut.setSpeedToMax();
            REQUIRE( cut.speed() == 33 );
        }

        SECTION ("Perception changes agent accuracy, Adrenaline does not") {
            // Agent base accuracy is 0.5; a result closer to 0 is more precise
            cut.setObjGroupDef(fs_knl::PedInstance::og_dmAgent);
            const double weaponAccuracy = 0.7;
            double base_acc = weaponAccuracy;

            cut.getAccuracy(base_acc);
            REQUIRE( base_acc == Catch::Approx(0.65) );

            // Adrenaline x2 has no effect on accuracy
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 0, 0, 0);
            cut.setIPAAmount(IPAStim::Adrenaline, 100);
            base_acc = weaponAccuracy;
            cut.getAccuracy(base_acc);
            REQUIRE( base_acc == Catch::Approx(0.65) );

            // Perception x1.5 adds 0.2 to the agent's accuracy
            cut.setIPAAmount(IPAStim::Perception, 100);
            base_acc = weaponAccuracy;
            cut.getAccuracy(base_acc);
            REQUIRE( base_acc == Catch::Approx(0.51) );

            // Perception x1/1.5 removes about 0.13
            cut.setIPAAmount(IPAStim::Perception, 0);
            base_acc = weaponAccuracy;
            cut.getAccuracy(base_acc);
            REQUIRE( base_acc == Catch::Approx(0.7433333) );
        }

        SECTION ("Adrenaline changes agent time between shots, not reload time") {
            const int reactionTime = fs_knl::PedInstance::kDefaultShootReactionTime;
            cut.setObjGroupDef(fs_knl::PedInstance::og_dmAgent);

            // Neutral Adrenaline keeps the default reaction time
            REQUIRE( cut.getTimeBetweenShoots(&pistol) == reactionTime + reloadTime );

            // Adrenaline x1.5 divides the reaction part by 1.5
            cut.setIPAAmount(IPAStim::Adrenaline, 100);
            REQUIRE( cut.getTimeBetweenShoots(&pistol) == 133 + reloadTime );

            // Adrenaline x2 halves the reaction part
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 0, 0, 0);
            cut.setIPAAmount(IPAStim::Adrenaline, 100);
            REQUIRE( cut.getTimeBetweenShoots(&pistol) == 100 + reloadTime );

            // Adrenaline x1/1.5 makes the reaction part 1.5 times longer
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 128, 128, 128);
            cut.setIPAAmount(IPAStim::Adrenaline, 0);
            REQUIRE( cut.getTimeBetweenShoots(&pistol) == 300 + reloadTime );
        }

        SECTION ("Enemy agent Adrenaline from mission data changes time between shots") {
            fs_knl::PedInstance enemy(3, nullptr, fs_knl::PedInstance::kPedTypeAgent, false, 50);
            enemy.setObjGroupDef(fs_knl::PedInstance::og_dmAgent);

            // Amount 75 / dependency 0 gives Adrenaline x1.75: 200 / 1.75 = 114
            enemy.initAllLevelsForIPAType(IPAStim::Adrenaline, 192, 0, 192);
            REQUIRE( enemy.getTimeBetweenShoots(&pistol) == 114 + reloadTime );
        }

        SECTION ("Non-agent peds keep the default time between shots") {
            civilian.setObjGroupDef(fs_knl::PedInstance::og_dmCivilian);

            civilian.initAllLevelsForIPAType(IPAStim::Adrenaline, 0, 0, 0);
            civilian.setIPAAmount(IPAStim::Adrenaline, 100);
            REQUIRE( civilian.getTimeBetweenShoots(&pistol) ==
                fs_knl::PedInstance::kDefaultShootReactionTime + reloadTime );
        }

        SECTION ("Adrenaline changes health regeneration period with Chest V2+") {
            fs_knl::Mod chestV1("ChestV1", fs_knl::Mod::MOD_CHEST, fs_knl::Mod::MOD_V1, 0, "", 0);
            fs_knl::Mod chestV2("ChestV2", fs_knl::Mod::MOD_CHEST, fs_knl::Mod::MOD_V2, 0, "", 0);
            fs_knl::Mod chestV3("ChestV3", fs_knl::Mod::MOD_CHEST, fs_knl::Mod::MOD_V3, 0, "", 0);
            cut.setObjGroupDef(fs_knl::PedInstance::og_dmAgent);

            // Neutral Adrenaline keeps the Chest periods
            cut.addMod(&chestV2);
            REQUIRE( cut.getHealthRegenerationPeriod() == 10000 );
            cut.addMod(&chestV3);
            REQUIRE( cut.getHealthRegenerationPeriod() == 4000 );

            // Adrenaline x2 doubles the period
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 0, 0, 0);
            cut.setIPAAmount(IPAStim::Adrenaline, 100);
            REQUIRE( cut.getHealthRegenerationPeriod() == 8000 );
            cut.addMod(&chestV2);
            REQUIRE( cut.getHealthRegenerationPeriod() == 20000 );

            // Adrenaline x0.5 halves the period
            setLowestAdrenaline(cut);
            REQUIRE( cut.getHealthRegenerationPeriod() == 5000 );
            cut.addMod(&chestV3);
            REQUIRE( cut.getHealthRegenerationPeriod() == 2000 );

            // Chest V1 does not regenerate whatever the Adrenaline level
            cut.addMod(&chestV1);
            REQUIRE_FALSE( cut.hasHealthRegeneration() );
            REQUIRE( cut.getHealthRegenerationPeriod() == 0 );
        }

        SECTION ("Agent without Chest does not regenerate whatever the Adrenaline level") {
            cut.setObjGroupDef(fs_knl::PedInstance::og_dmAgent);
            setLowestAdrenaline(cut);

            REQUIRE_FALSE( cut.hasHealthRegeneration() );
            REQUIRE( cut.getHealthRegenerationPeriod() == 0 );
        }

        SECTION ("Regeneration follows the current Adrenaline level") {
            fs_knl::Mod chestV3("ChestV3", fs_knl::Mod::MOD_CHEST, fs_knl::Mod::MOD_V3, 0, "", 0);
            cut.setObjGroupDef(fs_knl::PedInstance::og_dmAgent);
            cut.addMod(&chestV3);
            cut.behaviour().addComponent(new fs_knl::CommonAgentBehaviourComponent(&cut));

            cut.setStartHealth(200, true);
            cut.decreaseHealth(50);
            cut.behaviour().handleBehaviourEvent(fs_knl::Behaviour::kBehvEvtHit);

            // Neutral Adrenaline: one point every 4 s
            cut.behaviour().execute(4000, nullptr);
            REQUIRE( cut.health() == 150 );
            cut.behaviour().execute(1, nullptr);
            REQUIRE( cut.health() == 151 );

            // Calming the agent (x0.5) after the component was created heals every 2 s
            setLowestAdrenaline(cut);
            cut.behaviour().execute(2000, nullptr);
            REQUIRE( cut.health() == 151 );
            cut.behaviour().execute(1, nullptr);
            REQUIRE( cut.health() == 152 );

            // Boosting the agent (x2) heals every 8 s
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 0, 0, 0);
            cut.setIPAAmount(IPAStim::Adrenaline, 100);
            cut.behaviour().execute(4001, nullptr);
            REQUIRE( cut.health() == 152 );
            cut.behaviour().execute(4000, nullptr);
            REQUIRE( cut.health() == 153 );
        }

        SECTION ("Persuaded ped gets half of the persuader's Adrenaline bonus to speed") {
            cut.setObjGroupDef(fs_knl::PedInstance::og_dmAgent);
            civilian.handlePersuadedBy(&cut);

            // Neutral persuader: the persuaded ped keeps its own speed
            civilian.setSpeedToMax();
            REQUIRE( civilian.speed() == 50 );

            // Persuader at x1.8 -> persuaded ped at x1.4
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 0, 0, 0);
            cut.setIPAAmount(IPAStim::Adrenaline, 80);
            REQUIRE( cut.adrenaline().getMultiplier() == Catch::Approx(1.8) );
            civilian.setSpeedToMax();
            REQUIRE( civilian.speed() == 70 );

            // Persuader at about x0.6 -> persuaded ped at about x0.8
            setLowestAdrenaline(cut);
            cut.setIPAAmount(IPAStim::Adrenaline, 33);
            civilian.setSpeedToMax();
            REQUIRE( civilian.speed() == 39 );
        }

        SECTION ("Persuader Adrenaline multiplier is halved around x1") {
            cut.setObjGroupDef(fs_knl::PedInstance::og_dmAgent);
            REQUIRE( cut.getPersuadedSpeedMultiplier() == Catch::Approx(1.0) );

            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 0, 0, 0);
            cut.setIPAAmount(IPAStim::Adrenaline, 80);
            REQUIRE( cut.getPersuadedSpeedMultiplier() == Catch::Approx(1.4) );

            // Amount 33 with dependency 100 gives about x0.6
            setLowestAdrenaline(cut);
            cut.setIPAAmount(IPAStim::Adrenaline, 33);
            REQUIRE( cut.adrenaline().getMultiplier() == Catch::Approx(0.6).margin(0.01) );
            REQUIRE( cut.getPersuadedSpeedMultiplier() == Catch::Approx(0.8).margin(0.01) );
        }

        SECTION ("A non-agent persuader gives no speed bonus") {
            fs_knl::PedInstance owner(3, nullptr, fs_knl::PedInstance::kPedTypeCivilian, false, 50);
            owner.setIPAAmount(IPAStim::Adrenaline, 100);
            REQUIRE( owner.getPersuadedSpeedMultiplier() == Catch::Approx(1.0) );

            civilian.handlePersuadedBy(&owner);
            civilian.setSpeedToMax();
            REQUIRE( civilian.speed() == 50 );
        }
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

        SECTION ("Accuracy should improve with Eyes Mod only") {
            // cut is a kPedTypeAgent, base accuracy is 0.5 (see getBaseAccuracyFor)
            const double baseAccuracy = 0.5;
            // setObjGroupDef(og_dmAgent) is needed for the perception branch
            // in getAccuracy(); with neutral (default) perception level, its
            // contribution is zero
            cut.setObjGroupDef(fs_knl::PedInstance::og_dmAgent);

            fs_knl::Mod eyesV1("EyesV1", fs_knl::Mod::MOD_EYES, fs_knl::Mod::MOD_V1, 0, "", 0);
            fs_knl::Mod legsV3("LegsV3", fs_knl::Mod::MOD_LEGS, fs_knl::Mod::MOD_V3, 0, "", 0);

            const double weaponAccuracy = 0.7;

            double base_acc = weaponAccuracy;
            cut.getAccuracy(base_acc);
            double expected = weaponAccuracy * (1.0 - baseAccuracy) + (1.0 - weaponAccuracy);
            REQUIRE( base_acc == Catch::Approx(expected) );

            cut.addMod(&eyesV1);
            const double eyesBonus = 0.006 * (fs_knl::Mod::MOD_V1 + 1);
            base_acc = weaponAccuracy;
            cut.getAccuracy(base_acc);
            expected = weaponAccuracy * (1.0 - (baseAccuracy + eyesBonus)) + (1.0 - weaponAccuracy);
            REQUIRE( base_acc == Catch::Approx(expected) );

            // Other mods (Legs here) do not affect accuracy
            cut.addMod(&legsV3);
            base_acc = weaponAccuracy;
            cut.getAccuracy(base_acc);
            REQUIRE( base_acc == Catch::Approx(expected) );
        }

        SECTION ("Accuracy mod bonus is not applied to non-agent peds") {
            // civilian is a kPedTypeCivilian, base accuracy is 0.2 (see getBaseAccuracyFor)
            const double baseAccuracy = 0.2;
            civilian.setObjGroupDef(fs_knl::PedInstance::og_dmCivilian);

            // handleModAdded() ignores mods added to a ped whose type is not
            // kPedTypeAgent, so this mod has no effect on accuracy
            fs_knl::Mod eyesV3("EyesV3", fs_knl::Mod::MOD_EYES, fs_knl::Mod::MOD_V3, 0, "", 0);
            civilian.addMod(&eyesV3);

            const double weaponAccuracy = 0.7;
            double base_acc = weaponAccuracy;
            civilian.getAccuracy(base_acc);
            double expected = weaponAccuracy * (1.0 - baseAccuracy) + (1.0 - weaponAccuracy);
            REQUIRE( base_acc == Catch::Approx(expected) );
        }

        SECTION ("Heart, Eyes and Brain make the linked IPA level hold longer") {
            // All levels start neutral (amount, dependency and effect at 50).
            // Timers fire once more than their period has elapsed.
            cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 128, 128, 128);
            cut.initAllLevelsForIPAType(IPAStim::Perception, 128, 128, 128);
            cut.initAllLevelsForIPAType(IPAStim::Intelligence, 128, 128, 128);

            fs_knl::Mod heartV1("HeartV1", fs_knl::Mod::MOD_HEART, fs_knl::Mod::MOD_V1, 0, "", 0);
            fs_knl::Mod heartV2("HeartV2", fs_knl::Mod::MOD_HEART, fs_knl::Mod::MOD_V2, 0, "", 0);
            fs_knl::Mod heartV3("HeartV3", fs_knl::Mod::MOD_HEART, fs_knl::Mod::MOD_V3, 0, "", 0);
            fs_knl::Mod eyesV3("EyesV3", fs_knl::Mod::MOD_EYES, fs_knl::Mod::MOD_V3, 0, "", 0);
            fs_knl::Mod brainV3("BrainV3", fs_knl::Mod::MOD_BRAIN, fs_knl::Mod::MOD_V3, 0, "", 0);

            SECTION ("V3 Heart doubles both timer periods while Adrenaline is boosted") {
                cut.addMod(&heartV3);
                cut.setIPAAmount(IPAStim::Adrenaline, 100);

                elapseIPATime(cut, 2000);
                REQUIRE( cut.adrenaline().effect() == 50 );
                elapseIPATime(cut, 1);
                REQUIRE( cut.adrenaline().effect() == 51 );

                elapseIPATime(cut, 9000 - 2001);
                REQUIRE( cut.adrenaline().dependency() == 50 );
                elapseIPATime(cut, 1);
                REQUIRE( cut.adrenaline().dependency() == 51 );
            }

            SECTION ("V1 Heart gives x1.25") {
                cut.addMod(&heartV1);
                cut.setIPAAmount(IPAStim::Adrenaline, 100);

                elapseIPATime(cut, 1250);
                REQUIRE( cut.adrenaline().effect() == 50 );
                elapseIPATime(cut, 1);
                REQUIRE( cut.adrenaline().effect() == 51 );

                elapseIPATime(cut, 5625 - 1251);
                REQUIRE( cut.adrenaline().dependency() == 50 );
                elapseIPATime(cut, 1);
                REQUIRE( cut.adrenaline().dependency() == 51 );
            }

            SECTION ("V2 Heart gives x1.5") {
                cut.addMod(&heartV2);
                cut.setIPAAmount(IPAStim::Adrenaline, 100);

                elapseIPATime(cut, 1500);
                REQUIRE( cut.adrenaline().effect() == 50 );
                elapseIPATime(cut, 1);
                REQUIRE( cut.adrenaline().effect() == 51 );

                elapseIPATime(cut, 6750 - 1501);
                REQUIRE( cut.adrenaline().dependency() == 50 );
                elapseIPATime(cut, 1);
                REQUIRE( cut.adrenaline().dependency() == 51 );
            }

            SECTION ("Base periods apply when amount is below dependency") {
                cut.addMod(&heartV3);
                cut.setIPAAmount(IPAStim::Adrenaline, 0);

                elapseIPATime(cut, 1001);
                REQUIRE( cut.adrenaline().effect() == 49 );

                elapseIPATime(cut, 4501 - 1001);
                REQUIRE( cut.adrenaline().dependency() == 49 );
            }

            SECTION ("Eyes and Brain keep base periods when amount is below dependency") {
                cut.addMod(&eyesV3);
                cut.addMod(&brainV3);
                cut.setIPAAmount(IPAStim::Perception, 0);
                cut.setIPAAmount(IPAStim::Intelligence, 0);

                elapseIPATime(cut, 1001);
                REQUIRE( cut.perception().effect() == 49 );
                REQUIRE( cut.intelligence().effect() == 49 );

                elapseIPATime(cut, 4501 - 1001);
                REQUIRE( cut.perception().dependency() == 49 );
                REQUIRE( cut.intelligence().dependency() == 49 );
            }

            SECTION ("Base period applies when amount equals dependency") {
                cut.addMod(&heartV3);
                cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 192, 192, 192);

                elapseIPATime(cut, 4501);
                REQUIRE( cut.adrenaline().amount() == 74 );
                REQUIRE( cut.adrenaline().dependency() == 74 );
            }

            SECTION ("Eyes affect only Perception and Brain only Intelligence") {
                cut.setIPAAmount(IPAStim::Adrenaline, 100);
                cut.setIPAAmount(IPAStim::Perception, 100);
                cut.setIPAAmount(IPAStim::Intelligence, 100);

                cut.addMod(&eyesV3);
                elapseIPATime(cut, 1001);
                REQUIRE( cut.adrenaline().effect() == 51 );
                REQUIRE( cut.perception().effect() == 50 );
                REQUIRE( cut.intelligence().effect() == 51 );

                cut.addMod(&brainV3);
                elapseIPATime(cut, 1000);
                REQUIRE( cut.perception().effect() == 51 );
                REQUIRE( cut.intelligence().effect() == 51 );
                elapseIPATime(cut, 1001);
                REQUIRE( cut.adrenaline().effect() == 52 );
                REQUIRE( cut.intelligence().effect() == 52 );
            }

            SECTION ("The maximum IPA multiplier is unchanged by mods") {
                cut.addMod(&heartV3);
                cut.addMod(&eyesV3);
                cut.addMod(&brainV3);
                cut.initAllLevelsForIPAType(IPAStim::Adrenaline, 0, 0, 0);
                cut.setIPAAmount(IPAStim::Adrenaline, 100);
                REQUIRE( cut.adrenaline().getMultiplier() == Catch::Approx(2.0) );
            }

            SECTION ("Removing mods restores the base periods") {
                cut.addMod(&heartV3);
                cut.clearSlots();
                cut.setIPAAmount(IPAStim::Adrenaline, 100);

                elapseIPATime(cut, 1001);
                REQUIRE( cut.adrenaline().effect() == 51 );
                elapseIPATime(cut, 4501 - 1001);
                REQUIRE( cut.adrenaline().dependency() == 51 );
            }
        }
    }
}