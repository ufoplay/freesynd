# 01: Lock in the current IPA behaviour with tests

**What to build:** Characterisation tests at the ped seam that lock in how IPA levels behave today, so every later ticket changes behaviour knowingly. Covers the IPA multiplier curve, the effect and dependency timers, and the two effects that already exist (Adrenaline on speed, Perception on accuracy). Also fix the misleading comment on the multiplier, which claims full Adrenaline doubles speed when dependency is centred; in fact that gives ×1.5 (see spec, "Implementation Decisions"). Add a small test helper that advances an agent's IPA levels by a given elapsed time, for later tickets to reuse.

**Blocked by:** None (can start immediately).

**Status:** done

- [x] Tests assert the IPA multiplier: ×1 at neutral, ×1.5 for amount 100 / dependency 50, ×2 for amount 100 / dependency 0, 1/1.5 for amount 0 / dependency 50
- [x] Tests assert the effect catches up with the amount by 1 point per second, then the amount drifts toward the dependency with the effect following it
- [x] Tests assert the dependency moves 1 point per 4.5 s toward the amount, then both drift together back to 50
- [x] Tests assert that agent speed scales with the Adrenaline multiplier and accuracy with the Perception multiplier, and that Adrenaline does not affect accuracy
- [x] The multiplier comment describes the real curve
- [x] All tests go through the ped's public API only
- [x] No production behaviour change; all tests pass, no new warnings
