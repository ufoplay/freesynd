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

#include "fs-engine/appcontext.h"
#include "fs-engine/gfx/animationmanager.h"
#include "fs-engine/sound/soundmanager.h"
#include "fs-kernel/model/mission.h"
#include "fs-kernel/model/ped.h"
#include "testcase.h"

namespace {

/*! Test-only action that always fails and records whether it was executed.*/
class FakeFailingAction : public fs_knl::MovementAction {
public:
    explicit FakeFailingAction(bool blocking) :
        fs_knl::MovementAction(fs_knl::Action::kActTypeUndefined, false, false, blocking) {}

    bool executed = false;

protected:
    bool doExecute([[maybe_unused]] uint32_t elapsed, [[maybe_unused]] fs_knl::Mission *pMission,
            [[maybe_unused]] fs_knl::PedInstance *pPed) override {
        executed = true;
        setFailed();
        return true;
    }
};

/*! Test-only action that always succeeds and records whether it was executed.*/
class FakeSpyAction : public fs_knl::MovementAction {
public:
    FakeSpyAction() : fs_knl::MovementAction(fs_knl::Action::kActTypeUndefined) {}

    bool executed = false;

protected:
    bool doExecute([[maybe_unused]] uint32_t elapsed, [[maybe_unused]] fs_knl::Mission *pMission,
            [[maybe_unused]] fs_knl::PedInstance *pPed) override {
        executed = true;
        setSucceeded();
        return true;
    }
};

/*! Test-only sound manager that never plays anything, so weapons can fire without audio.*/
class SilentSoundManager : public fs_eng::SoundManager {
public:
    SilentSoundManager() {
        disabled_ = true;
        audio_ = nullptr;
    }
};

/*! Test-only animation manager where every animation is a single frame, so map objects can play animations without game data.*/
class OneFrameAnimationManager : public fs_eng::AnimationManager {
public:
    OneFrameAnimationManager() {
        addFrameElement(fs_eng::GameSpriteFrameElement());
        // A frame whose next frame is itself
        addFrame(fs_eng::GameSpriteFrame());
        for (int i = 0; i < kNbAnimations; ++i) {
            addFramesAnimation(0);
        }
    }

private:
    //! Higher than any animation id used by the game
    static constexpr int kNbAnimations = 1024;
};

}  // namespace

TEST_CASE( "PedAction", "[kernel][ped]" ) {
    fs_knl::PedInstance cut(1, nullptr, fs_knl::PedInstance::kPedTypeAgent, true, 128);
    cut.setStartHealth(10);
    cut.goToState(fs_knl::kPedActionStateStanding);

    SECTION( "can take hit action") {
        // Reject cause health is zero
        REQUIRE_FALSE( cut.canTakeAction(fs_knl::Action::kActTypeHit) );

        // Should accept
        cut.resetHealth();
        REQUIRE( cut.canTakeAction(fs_knl::Action::kActTypeHit) );

        // Reject cause state is already hit
        cut.goToState(fs_knl::kPedActionStateHit);
        REQUIRE_FALSE( cut.canTakeAction(fs_knl::Action::kActTypeHit) );
    }

    SECTION( "a blocking action failure stops the rest of the chain") {
        cut.resetHealth();
        FakeFailingAction *pFail = new FakeFailingAction(true);
        FakeSpyAction *pSpy = new FakeSpyAction();
        pFail->link(pSpy);
        cut.addMovementAction(pFail, false);

        cut.executeAction(100, nullptr);

        REQUIRE( pFail->executed );
        REQUIRE_FALSE( pSpy->executed );
        REQUIRE( cut.currentAction() == nullptr );
    }

    SECTION( "a non-blocking action failure lets the chain continue") {
        cut.resetHealth();
        FakeFailingAction *pFail = new FakeFailingAction(false);
        FakeSpyAction *pSpy = new FakeSpyAction();
        pFail->link(pSpy);
        cut.addMovementAction(pFail, false);

        cut.executeAction(100, nullptr);

        REQUIRE( pFail->executed );
        REQUIRE( pSpy->executed );
    }
}

TEST_CASE( "Automatic shooting", "[kernel][ped][weapon]" ) {
    fs_eng::AppContext appCtx;
    SilentSoundManager soundMgr;
    OneFrameAnimationManager animMgr;
    ConfigFile config;
    initWeaponConfigFile(config);

    MemoryTileManager tileMgr;
    fs_knl::Map map(&tileMgr, 1);
    REQUIRE( loadMapFromCsv(TEST_DATA_DIR "/map-5x5x4.csv", tileMgr, map) );
    fs_knl::Mission mission;
    REQUIRE( mission.init(&map) );
    // Shots look for blocking tiles and objects: build the surfaces and,
    // with an empty first tick, the grid of moving objects
    mission.buildNavigationGraph();
    mission.handleTick(0, 0);

    fs_knl::Weapon uziClass(fs_knl::Weapon::Uzi, config);
    fs_knl::WeaponInstance uzi(&uziClass, 0, &map);
    const int fullAmmo = uzi.ammoRemaining();

    fs_knl::PedInstance agent(1, &map, fs_knl::PedInstance::kPedTypeAgent, true, 50);
    agent.setStartHealth(10, true);
    agent.setPosition(1, 1, 1);
    agent.goToState(fs_knl::kPedActionStateStanding);
    agent.addWeapon(&uzi);
    uzi.setOwner(&agent);
    agent.selectWeapon(&uzi);

    // Aim inside the map, a few tiles away from the agent
    const fs_knl::WorldPoint target(fs_knl::TilePoint(3, 3, 1));

    SECTION( "a click released before the first shot fires exactly one shot") {
        REQUIRE( agent.addActionShootAt(target) == fs_knl::ShootAction::kShootActionAutomaticShoot );
        // Player releases the button before the action had a chance to start
        agent.stopShooting();

        agent.executeUseWeaponAction(16, &mission);
        REQUIRE( uzi.ammoRemaining() == fullAmmo - 1 );

        // Many fire periods later, no other shot has been fired
        for (int i = 0; i < 20; ++i) {
            agent.executeUseWeaponAction(100, &mission);
        }
        REQUIRE( uzi.ammoRemaining() == fullAmmo - 1 );
        // and the agent is ready to shoot again
        REQUIRE_FALSE( agent.isUsingWeapon() );
    }
}
