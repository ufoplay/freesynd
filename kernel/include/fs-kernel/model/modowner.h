/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
 *   Copyright (C) 2006  Tarjei Knapstad <tarjei.knapstad@gmail.com>
 *   Copyright (C) 2011  Bohdan Stelmakh <chamel@users.sourceforge.net>
 *   Copyright (C) 2011  Joey Parrish  <joey.parrish@gmail.com>
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
#ifndef MODOWNER_H
#define MODOWNER_H

#include "fs-kernel/model/mod.h"

namespace fs_knl {

class ModOwner {
public:
    ModOwner() {
        for (int i = 0; i < 6; i++)
            slots_[i] = NULL;
    }
    virtual ~ModOwner() {}
    /*!
    * Returns true if the agent can be equiped with that mod version.
    */
    bool canHaveMod(Mod *pNewMod) {
        if (pNewMod == NULL) {
            return false;
        }

        Mod *pMod = slots_[pNewMod->getType()];
        if (pMod) {
            // Agent has a mod of the same type
            // Returns true if equiped version if less than new version
            return (pMod->getVersion() < pNewMod->getVersion());
        }

        // There is no mod of that type so agent can be equiped
        return true;
    }

    void addMod(Mod *pNewMod) {
        if (pNewMod) {
            slots_[pNewMod->getType()] = pNewMod;
            handleModAdded(pNewMod);
        }
    }

    Mod *slot(int n) {
        assert(n < 6);
        return slots_[n];
    }

    /*!
     * Returns true if the owner is equiped with mod of the
     * given type and at least of given version.
     */
    bool hasMinimumVersionOfMod(Mod::EModType type, Mod::EModVersion version) {
        Mod *pMod = slots_[type];
        return (pMod && pMod->getVersion() >= version);
    }

    void clearSlots() {
        for (int i = 0; i < 6; i++)
            slots_[i] = NULL;
    }

    /*!
     * @brief Return true if ped has the right version of mod (Chest) to auto heal
     * @return Chest must be at least V2
     */
    bool hasHealthRegeneration() {
        Mod *pMod = slots_[Mod::MOD_CHEST];
        return pMod && pMod->getVersion() >= Mod::MOD_V2;
    }

    /*!
     * @brief Returns the base amount of time before health is restored
     * when a ped owns the right version of Chest.
     * @return Period in milliseconds, 0 if ped does not have a Chest V2+.
     */
    uint16_t getChestRegenerationPeriod() {
        Mod *pMod = slots_[Mod::MOD_CHEST];
        if (pMod) {
            switch(pMod->getVersion()) {
                case Mod::MOD_V2:
                    return 10000;
                case Mod::MOD_V3:
                    return 4000;
                default:
                    return 0;
            }
        }

        return 0;
    }

    /*!
     * Return a damage reduction multiplier corresponding to the mod Chest.
     * @return x1 (no reduction) if Agent has no mod for chest
     */
    float getDamageResistance() {
        Mod *pMod = slots_[Mod::MOD_CHEST];
        if (pMod) {
            switch(pMod->getVersion()) {
                case Mod::MOD_V1:
                    return 0.9f;
                case Mod::MOD_V2:
                    return 0.75f;
                case Mod::MOD_V3:
                    return 0.6f;
            }
        }
        return 1.0f;
    }

    /*!
     * Return a multiplier factor corresponding to the mod Leg.
     * @return x1 if Agent has no mod for leg
     */
    float getSpeedMultiplier() {
        Mod *pMod = slots_[Mod::MOD_LEGS];
        if (pMod) {
            switch(pMod->getVersion()) {
                case Mod::MOD_V1:
                    return 1.25;
                case Mod::MOD_V2:
                    return 1.50;
                case Mod::MOD_V3:
                    return 1.75;
            }
        }
        return 1.0;
    }

    int getMaxWeight() {
        Mod *pMod = slots_[Mod::MOD_ARMS];
        if (pMod) {
            switch(pMod->getVersion()) {
                case Mod::MOD_V1:
                    return 10;
                case Mod::MOD_V2:
                    return 20;
                case Mod::MOD_V3:
                    return 200;
            }
        }
        return 5;
    }

    void transferMods(ModOwner &modOwner) {
        for (int i = 0; i < 6; i++) {
            modOwner.addMod(slots_[i]);
        }
    }

protected:
    //! Called when a mod has been added/upgraded.
    virtual void handleModAdded([[maybe_unused]] Mod *pNewMod) {}

    Mod *slots_[6];
};

};

#endif
