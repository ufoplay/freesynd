/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
 *   Copyright (C) 2006  Tarjei Knapstad <tarjei.knapstad@gmail.com>
 *   Copyright (C) 2010  Bohdan Stelmakh <chamel@users.sourceforge.net>
 *   Copyright (C) 2013, 2025-2026  Benoit Blancard <benblan@users.sourceforge.net>
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

#ifndef PEDACTIONSTATE_H
#define PEDACTIONSTATE_H

namespace fs_knl {

//! This is the list of possible state for a PedInstance
enum PedActionState {
    kPedActionStateStanding,
    kPedActionStateStandingFiring,
    kPedActionStateWalking,
    kPedActionStateWalkingFiring,
    kPedActionStateHit,
    kPedActionStatePickUp,
    kPedActionStatePutDown,
    //! Pseudo-state: only used as a parameter to goToState()/leaveState() to
    //! start/stop firing from the current state ; never stored as-is in state_.
    kPedActionStateFiring
};

}
#endif // PEDACTIONSTATE_H
