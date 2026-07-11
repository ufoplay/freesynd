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
