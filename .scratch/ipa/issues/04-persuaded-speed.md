# 04: Persuaded NPCs get half the Adrenaline bonus

**What to build:** A persuaded pedestrian moves with half of their persuader's Adrenaline bonus or malus: their speed multiplier is `1 + (persuader Adrenaline multiplier − 1) / 2`. This replaces today's step function, which only looks at which side of neutral the persuader is on (×2 / ×1 / ×0.5, encoded as the doubled integers 4 / 2 / 1). The owner boost becomes a float multiplier, and the "divide by 2" convention disappears.

**Blocked by:** 01 (Lock in the current IPA behaviour with tests)

**Status:** ready-for-agent

- [ ] Persuader at ×1 → persuaded ped at ×1
- [ ] Persuader at ×1.8 → persuaded ped at ×1.4
- [ ] Persuader at ×0.6 → persuaded ped at ×0.8
- [ ] A non-agent owner gives ×1
- [ ] The doubled-integer convention and its comment are removed from every caller
- [ ] Covered by ped tests; in game, followers visibly keep pace with a boosted agent
