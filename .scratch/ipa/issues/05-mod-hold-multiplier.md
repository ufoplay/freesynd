# 05: Heart, Eyes and Brain make the linked IPA level hold longer

**What to build:** A boosted IPA level lasts longer on an agent with the linked mod, without a higher peak. Heart is linked to Adrenaline, Eyes to Perception and Brain to Intelligence. While the IPA level's amount is above its dependency, both its effect timer and its dependency timer run slower by a hold multiplier; at or below the dependency the base periods (1 s and 4.5 s) apply, so recovery is not slowed. The hold multipliers are kept in a single table: none ×1, V1 ×1.25, V2 ×1.5, V3 ×2. The multiplier refreshes whenever a mod is added or removed. The Eyes' flat accuracy bonus and the Brain's persuasion and access-card effects are unchanged.

**Blocked by:** 01 (Lock in the current IPA behaviour with tests)

**Status:** done

- [x] Without the linked mod, the timers behave exactly as pinned in ticket 01
- [x] With a V3 Heart and boosted Adrenaline, the effect timer ticks every 2 s and the dependency timer every 9 s
- [x] V1 and V2 give ×1.25 and ×1.5 respectively
- [x] With the amount at or below the dependency, the base periods apply even with the mod installed
- [x] Eyes affect only Perception and Brain only Intelligence
- [x] The maximum IPA multiplier is unchanged by any mod
- [x] Installing or removing a mod updates the hold multiplier
- [x] Hold multiplier values live in one place
- [x] Covered by ped tests

**Note:** in game, mods are only ever cleared on the persistent `Agent`, never on a ped during a mission. `ModOwner::clearSlots()` now calls a `handleModsCleared()` hook, so a ped also refreshes its speed, accuracy and hold multipliers when its mods are removed.
