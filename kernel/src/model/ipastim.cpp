/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2012  Ryan Cocks <ryan@ryancocks.net>
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

#include "fs-kernel/model/ipastim.h"

#include <assert.h>
#include <cmath>

#ifdef _DEBUG
#include <stdio.h>

const char * IPAStim::IPANames[3] = {
    "Adrenaline",
    "Perception",
    "Intelligence"
};
#endif

IPAStim::IPAStim(IPAType ipa_type, uint8_t amount, uint8_t dependency)
:ipa_type_(ipa_type), effect_(50), effect_timer_(kEffectPeriod), dependency_timer_(kDependencyPeriod),
holdMultiplier_(1.0f)
{
    assert(ipa_type_ <= 3);
    setLevels(amount, dependency);
}

int IPAStim::getMagnitude() const
{
    return dependency_ < amount_ ? amount_ - dependency_: dependency_ - amount_;
}


float IPAStim::getMultiplier() const
{
    // The multiplier depends only on the gap between amount and dependency:
    // - amount above dependency: 1 + gap/100, from 1 up to 2
    // - amount below dependency: 1 / (1 + gap/100), from 1 down to 0.5
    // When amount equals dependency, the IPA level is neutral and returns 1.

    // With dependency at neutral (50), the amount at either extreme gives
    // only x1.5 or x1/1.5. Reaching x2 (or x0.5) requires the dependency
    // to be at the opposite extreme, which rewards the player for letting
    // dependency go down before injecting again.
    int magnitude = getMagnitude();

    if(direction() == kIPADirBoost) {
        // return value is 1 to 2 for values
        // of 'magnitude' from 0 to 100
        // If you fiddle with this equation beware of
        // values for effective which are close to 0
        float mult = part_of_two(magnitude);
        assert(mult >= 1 && mult <= 2);
        //printf("%s boost: m:%d->%fx\n", getName(), magnitude_, mult);
        return mult;
    } else {
        // < 0
        // range: 0.5 up towards 1
        float mult = 1.0/part_of_two(magnitude);
        assert(mult >= 0.5 && mult <= 1.0);
        //printf("%s reduce: m:%d->%fx\n", getName(), magnitude_, mult);
        return mult;
    }
}

void IPAStim::setLevels(uint8_t amount, uint8_t dependency, uint8_t effect)
{
    amount_ = amount;
    dependency_ = dependency;

    effect_ = effect;

    //printf("%s: A: %d, D: %d, E: %d\n", getName(), amount, dependency, effect_);
}

uint32_t IPAStim::holdPeriod(uint32_t basePeriod) const
{
    // Holding only slows down a boost: recovery keeps the base period
    if (amount_ > dependency_) {
        // Hold multipliers are small positive factors, so the period fits in 32 bits
        return static_cast<uint32_t>(std::lround(static_cast<float>(basePeriod) * holdMultiplier_));
    }
    return basePeriod;
}

void IPAStim::processTicks(uint32_t elapsed)
{
    // From observation of the original Syndicate:
    // * Effect moves about once a second.
    // * Dependency bar moves once for every 5 or 6 moves of Effect
    // * Effect is independent of amount! If you flick amount to the
    //   other side effect will stay where it was.
    // * there appear to be 50 'positions' on the bar so it looks
    //   like the levels move in notches, 1% at a time.

    effect_timer_.setMax(holdPeriod(kEffectPeriod));
    if(effect_timer_.update(elapsed))
    {
        if(effect_ > amount_)
        {
            --effect_;
        }
        else if(effect_ < amount_)
        {
            ++effect_;
        }
        else // equal
        {
            // So once effect has 'caught up' to amount then they
            // both start moving towards the value of dependency
            // together
            if(amount_ > dependency_)
            {
                effect_ = --amount_;
            }
            else if(amount_ < dependency_)
            {
                effect_ = ++amount_;
            }
        }
        assert(effect_ >= 0 && effect_ <= 100);
    }

    // The dependency indicator always creaps towards amount
    dependency_timer_.setMax(holdPeriod(kDependencyPeriod));
    if(dependency_timer_.update(elapsed))
    {
        if (dependency_ > amount_) {
            --dependency_;
        } else if (dependency_ < amount_) {
            ++dependency_;
        } else {
            // equal
            if (amount_ < 50) {
                ++amount_;
                ++dependency_;
            } else if (amount_ > 50) {
                --amount_;
                --dependency_;
            }
        }
        assert(dependency_ >= 0 && dependency_ <= 100);

    }
}
