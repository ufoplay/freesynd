# Spec: IPA levels affect agents in missions

Status: done

Vocabulary: `CONTEXT.md`.

## Problem Statement

During a mission the player can inject drugs into their agents through the three IPA level bars (Adrenaline, Perception, Intelligence), and the amount / effect / dependency bars already move as in the original Syndicate. But apart from Adrenaline on movement speed and Perception on accuracy, the IPA levels change almost nothing: shooting reaction time ignores Adrenaline, health regeneration ignores Adrenaline, Heart / Eyes / Brain mods have no influence on the IPA levels they are tied to, persuaded pedestrians only get a crude on/off speed bonus, there is no Panic Mode shortcut, and Group mode only works while dragging. The player therefore has little reason to manage drugs and dependency, and the Heart / Eyes / Brain mods feel pointless for IPA.

## Solution

Make every IPA level and the IPA-related mods have a visible, consistent effect on agents during a mission:

- The IPA multiplier (×0.5 to ×2, from the gap between amount and dependency) drives movement speed, shooting reaction time and the speed of health regeneration (Adrenaline), and accuracy (Perception). Intelligence has a defined multiplier reserved for the future agent AI.
- Heart, Eyes and Brain mods make the boost of their linked IPA level last longer, without raising its peak.
- Persuaded pedestrians get half of their persuader's Adrenaline bonus.
- The player can trigger Panic Mode (both mouse buttons on the map) to push all IPA amounts of the selected agents to maximum.
- A click or a drag on the IPA bars of a selected agent applies to every selected agent (Group mode).

## User Stories

### IPA levels and dependency

1. As a player, I want clicking on an IPA bar to set the amount to the position under the cursor, so that I can inject exactly the dose I want.
2. As a player, I want to drag along an IPA bar to adjust the amount continuously, so that I can fine-tune a dose.
3. As a player, I want the effect (dark section) to catch up with the amount by one point per second, so that the drug takes effect progressively.
4. As a player, I want the amount to start drifting toward the dependency once the effect has caught up, so that I see the drug wearing off.
5. As a player, I want the dependency to creep toward the amount by one point every 4.5 seconds, so that keeping an IPA level high erodes its benefit.
6. As a player, I want amount and dependency to drift back together to neutral (50) once they meet, so that an agent left alone returns to normal.
7. As a player, I want the IPA multiplier to depend only on the gap between amount and dependency, so that the dependency bar tells me how much benefit I can still get.
8. As a player, I want a maximum boost of ×2 to be reachable only when dependency is low, so that letting an agent come down is rewarded with a stronger next injection.
9. As a player, I want lowering an amount below the dependency to give a multiplier below 1 (down to ×0.5), so that reducing dependency has a real short-term cost.
10. As a player, I want every agent to start each mission with neutral IPA levels, so that drug management is a per-mission concern.

### Adrenaline

11. As a player, I want a boosted Adrenaline level to make my agent move faster, so that I can sprint across the map.
12. As a player, I want a reduced Adrenaline level to make my agent move slower, so that the trade-off is visible.
13. As a player, I want Adrenaline to shorten the reaction time between two shots, so that a boosted agent fires more often.
14. As a player, I want a reduced Adrenaline level to lengthen the reaction time between two shots, so that a calmed agent is slower to respond.
15. As a player, I want the weapon reload time to stay the same whatever the Adrenaline level, so that weapon stats remain fixed.
16. As a player, I want a boosted Adrenaline level to slow down health regeneration of an agent with a Chest V2 or V3, so that sprinting and healing are in tension.
17. As a player, I want a reduced Adrenaline level to speed up health regeneration of an agent with a Chest V2 or V3, so that I can calm a wounded agent to heal them faster.
18. As a player, I want agents without a Chest V2 or V3 to still not regenerate, so that the Chest mod keeps its value.

### Perception

19. As a player, I want a boosted Perception level to make my agent shoot more accurately, so that shots land where I aim.
20. As a player, I want a reduced Perception level to widen my agent's shot spread, so that shots may hit several enemies in a group.
21. As a player, I want Adrenaline to have no effect on accuracy, so that each IPA level has a clear role.

### Intelligence

22. As a developer, I want Intelligence to expose the same IPA multiplier as the other levels, so that the future agent AI can consume it without redesign.

### Mods linked to an IPA level

23. As a player, I want a Heart mod to make a boosted Adrenaline level last longer, so that my agent can sprint longer.
24. As a player, I want an Eyes mod to make a boosted Perception level last longer, so that my agent keeps high accuracy longer.
25. As a player, I want a Brain mod to make a boosted Intelligence level last longer, so that the future AI stays sharp longer.
26. As a player, I want higher mod versions to extend the boost more (×1.25 / ×1.5 / ×2 for V1 / V2 / V3), so that upgrading pays off.
27. As a player, I want these mods not to raise the maximum multiplier, so that an unmodded and a modded agent peak at the same level.
28. As a player, I want these mods not to slow down recovery when the IPA level is at or below the dependency, so that a modded agent recovers from dependency as fast as an unmodded one.
29. As a player, I want the Eyes mod to keep its flat accuracy bonus, so that it still improves shooting when Perception is neutral.
30. As a player, I want the Arms mod to have no effect on accuracy, so that its role stays carrying capacity.
31. As a player, I want the Brain mod to keep reducing the persuasion points needed and enabling the access card at V3, so that existing behaviour is preserved.

### Persuaded pedestrians

32. As a player, I want a persuaded pedestrian to get half of their persuader's Adrenaline bonus to speed, so that my followers keep up roughly with my agent.
33. As a player, I want a persuaded pedestrian to get half of their persuader's Adrenaline malus when the persuader is calmed, so that the rule is symmetric.

### Panic Mode

34. As a player, I want to press the left and right mouse buttons together on the map to trigger Panic Mode, so that I can react instantly in an emergency.
35. As a player, I want Panic Mode to push all three IPA amounts of every selected agent to maximum, so that the whole selection is boosted at once.
36. As a player, I want the IPA levels to evolve normally after Panic Mode (effect catch-up, wear-off, dependency), so that Panic Mode has a cost if overused.
37. As a player, I want Panic Mode to ignore dead agents, so that nothing strange happens to corpses.

### Group mode

38. As a player, I want a click on an IPA bar of a selected agent to apply the new amount to every selected agent, so that I can manage my squad's drugs in one gesture.
39. As a player, I want a drag on an IPA bar of a selected agent to apply to every selected agent, so that click and drag behave the same.
40. As a player, I want a click or drag on an IPA bar of an unselected agent to affect only that agent, so that I can still tune one agent individually.

## Implementation Decisions

- **IPA multiplier curve is kept as implemented**: when amount > dependency the multiplier is `1 + gap/100`; otherwise `1 / (1 + gap/100)`, where `gap = |amount − dependency|`. The multiplier comment that claims full Adrenaline doubles speed with a centred dependency is wrong and must be corrected to describe this curve.
- **IPA timers**: effect timer period 1 s, dependency timer period 4.5 s, steps of 1 point, rules unchanged from the current IPA stim behaviour.
- **Hold multiplier for linked mods**: the IPA stim gains a way to receive a hold multiplier (default 1). The ped sets it for the linked IPA level according to the installed mod: Heart → Adrenaline, Eyes → Perception, Brain → Intelligence. Both timer periods are multiplied by the hold multiplier **only while amount > dependency**; otherwise the base periods apply. The values live in a single table so they are easy to rebalance:

  | Mod version | Hold multiplier |
  |---|---|
  | none | ×1 |
  | V1 | ×1.25 |
  | V2 | ×1.5 |
  | V3 | ×2 |

  The hold multiplier must be refreshed whenever a mod is added or removed (same place where speed is already refreshed for Legs / Arms).
- **Speed**: unchanged — static speed (base, Legs, weight) is multiplied by the Adrenaline multiplier for agents.
- **Accuracy**: unchanged — Perception multiplier plus the flat Eyes bonus. Arms do not contribute.
- **Reaction time**: the time between two shots becomes `default shoot reaction time / Adrenaline multiplier + weapon reload time` for agents. Other peds keep the default reaction time.
- **Health regeneration**: the Chest regeneration period (10 s for V2, 4 s for V3) is multiplied by the Adrenaline multiplier for agents. Agents without Chest V2+ do not regenerate.
- **Persuaded pedestrian speed**: the current step-function owner boost (×2 / ×1 / ×0.5 encoded as 4 / 2 / 1) is replaced by a multiplier `1 + (persuader Adrenaline multiplier − 1) / 2`. The owner-boost interface returns a float multiplier instead of the doubled integer.
- **Panic Mode**: a new public method on the ped sets all three IPA amounts to 100 (no-op on dead peds). It creates no special state. It must not be confused with the existing civilian panic logic (`isInPanic` and related), which is unrelated and stays unchanged.
- **Panic Mode input**: the gameplay menu detects left + right mouse buttons pressed together on the map view and calls the Panic Mode method on every selected agent. The simultaneous press must not also trigger the single-button map actions (move, shoot).
- **Group mode**: in the gameplay menu, a click on an IPA bar follows the same rule as the existing drag: if the clicked agent is selected, the amount is applied to all selected agents; otherwise only to the clicked agent.
- **Enemy agents**: not a requirement of this feature. As it stands, the IPA rules are tied to the agent ped type, so enemy agents are affected by their mission-data IPA levels (speed, accuracy, time between shots) as a side effect; they do not regenerate. Only the player's squad has its IPA levels updated each tick. How enemies should use IPA is deferred to a future feature.
- **Mission start**: agents' IPA levels are neutral (50 / 50 / 50) at the start of each mission; levels are not saved between missions.
- **Intelligence**: its multiplier is computed like the others but has no consumer in this feature.

## Testing Decisions

- A good test drives an agent through its public API only — set IPA amounts, install mods, advance time, trigger Panic Mode — and asserts observable outcomes (speed, accuracy fraction, time between shots, regeneration period, persuaded speed, IPA amount / effect / dependency values). No test reaches into private members or relies on how the timers are implemented internally.
- **Single seam**: the ped (`PedInstance`) public API, tested in the existing ped test file. The IPA stim is exercised through the ped, not tested separately.
- Scenarios to cover:
  - multiplier values at neutral, boosted and reduced gaps (e.g. amount 100 / dependency 50 → ×1.5; amount 100 / dependency 0 → ×2; amount 0 / dependency 50 → ×1/1.5);
  - timer evolution over elapsed time, with and without Heart / Eyes / Brain of each version, both while boosted and while at or below dependency;
  - speed with Adrenaline boosted and reduced;
  - time between shots with Adrenaline boosted and reduced, reload time unchanged;
  - regeneration period with Chest V2 / V3 and Adrenaline boosted / reduced; no regeneration without Chest V2+;
  - persuaded pedestrian speed multiplier derived from the persuader;
  - Panic Mode sets all amounts to 100 and levels then evolve normally; no effect on a dead ped.
- **Prior art**: the ped test file's "Mods" section, which builds an agent, installs `Mod` objects and checks speed and damage reduction.
- **Not automated**: two-button detection, click-vs-drag Group mode and bar rendering live in the game layer, which has no test harness; they are verified manually in game.

## Out of Scope

- The Intelligence-driven agent AI: autonomous threat detection, automatic weapon choice, fleeing when unarmed or in danger, automatic defensive fire when left on guard, and the "combined behaviours" from the source document. This will be a separate feature consuming the Intelligence multiplier.
- Any link between the Brain mod and reaction time (moves to the AI feature).
- Mod catalogue, research unlocking and prices (already implemented).
- Legs speed multipliers, Arms carrying capacity and Chest damage reduction (already implemented).
- Dedicated ammo-saving rules or multi-target hit rules — these are natural consequences of the accuracy change.
- Persisting IPA levels between missions.
- Enemy agent IPA behaviour (formerly user stories 41–42: enemy IPA levels affecting speed, accuracy, reaction time and regeneration, and staying fixed during the mission), deferred to a future feature.
- Ticking enemy agents' IPA levels.
- Changes to civilian panic.

## Further Notes

- The IPA stim class keeps its name; the glossary term is **IPA level**.
- No ADR was recorded: every decision here is a balancing choice that is cheap to revisit.
