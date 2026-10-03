# 04: Persuaded NPCs get half the Adrenaline bonus

**What to build:** A persuaded pedestrian moves with half of their persuader's Adrenaline bonus or malus: their speed multiplier is `1 + (persuader Adrenaline multiplier − 1) / 2`. This replaces today's step function, which only looks at which side of neutral the persuader is on (×2 / ×1 / ×0.5, encoded as the doubled integers 4 / 2 / 1). The owner boost becomes a float multiplier, and the "divide by 2" convention disappears.

**Blocked by:** 01 (Lock in the current IPA behaviour with tests)

**Status:** done

- [x] Persuader at ×1 → persuaded ped at ×1
- [x] Persuader at ×1.8 → persuaded ped at ×1.4
- [x] Persuader at ×0.6 → persuaded ped at ×0.8
- [x] A non-agent owner gives ×1
- [x] The doubled-integer convention and its comment are removed from every caller
- [x] Covered by ped tests
- [ ] In game, followers visibly keep pace with a boosted agent (manual check)

**Note:** the old code never applied the `/2` correction (it was commented out), so persuaded peds actually moved at ×4 / ×2 / ×1. With a neutral persuader they now move at their own speed (×1), i.e. half as fast as before this change.
