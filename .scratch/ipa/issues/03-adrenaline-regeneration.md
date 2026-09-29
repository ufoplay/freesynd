# 03: Adrenaline affects health regeneration

**What to build:** An agent equipped with a Chest V2 or V3 regenerates health faster when calmed and slower when boosted: the Chest regeneration period (10 s for V2, 4 s for V3) is multiplied by the Adrenaline multiplier. Agents without a Chest V2+ still do not regenerate. Check whether enemy agents with a Chest V2+ regenerate through the same path; if they do, the Adrenaline rule applies to them too.

**Blocked by:** 01 (Lock in the current IPA behaviour with tests)

**Status:** ready-for-agent

- [ ] With neutral Adrenaline, regeneration periods are unchanged (10 s V2, 4 s V3)
- [ ] With Adrenaline at ×2, a Chest V3 agent regenerates every 8 s; at ×0.5, every 2 s
- [ ] An agent without a Chest, or with a Chest V1, does not regenerate whatever the Adrenaline level
- [ ] The period is evaluated from the current Adrenaline level, so changing the dose changes the healing speed
- [ ] Covered by ped tests
