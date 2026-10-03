# 02: Adrenaline affects reaction time

**What to build:** An agent's time between two shots depends on their Adrenaline level: boosted Adrenaline makes them fire again sooner, reduced Adrenaline makes them slower. The reaction part becomes `default shoot reaction time / Adrenaline multiplier`; the weapon's reload time is added unchanged. It applies to the player's agents; other peds keep the default reaction time.

**Note:** the rule is tied to the agent group, so enemy agents also get it from their mission-data Adrenaline levels. This is a side effect, not a requirement: enemy IPA behaviour is deferred to a future feature (see the spec, "Enemy agents").

**Blocked by:** 01 (Lock in the current IPA behaviour with tests)

**Status:** done

- [x] With neutral Adrenaline, the time between shots is unchanged from today
- [x] With boosted Adrenaline (e.g. ×1.5), the reaction part is divided by the multiplier
- [x] With reduced Adrenaline, the reaction part grows accordingly
- [x] The weapon reload time is never changed by Adrenaline
- [x] Non-agent peds keep the default reaction time
- [x] Covered by ped tests; the obsolete TODO about IPA and mods is resolved
