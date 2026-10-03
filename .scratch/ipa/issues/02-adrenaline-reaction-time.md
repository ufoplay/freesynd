# 02: Adrenaline affects reaction time

**What to build:** An agent's time between two shots depends on their Adrenaline level: boosted Adrenaline makes them fire again sooner, reduced Adrenaline makes them slower. The reaction part becomes `default shoot reaction time / Adrenaline multiplier`; the weapon's reload time is added unchanged. It applies to every ped in the agent group (player agents and enemy agents); other peds keep the default reaction time.

**Blocked by:** 01 (Lock in the current IPA behaviour with tests)

**Status:** done

- [x] With neutral Adrenaline, the time between shots is unchanged from today
- [x] With boosted Adrenaline (e.g. ×1.5), the reaction part is divided by the multiplier
- [x] With reduced Adrenaline, the reaction part grows accordingly
- [x] The weapon reload time is never changed by Adrenaline
- [x] An enemy agent with Adrenaline levels from the mission data gets the same effect
- [x] Non-agent peds keep the default reaction time
- [x] Covered by ped tests; the obsolete TODO about IPA and mods is resolved
