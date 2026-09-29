# FreeSynd

A remake of Syndicate: the player runs a syndicate, equips cyborg agents in the cryo-chamber, and leads them through real-time missions.

## Language

### Agent enhancement

**Mod**:
A cybernetic body part installed on an agent in the cryo-chamber, in one of six slots (Legs, Arms, Chest, Heart, Eyes, Brain) and one of three versions (V1–V3).
_Avoid_: Implant, upgrade

**Drug**:
The substance injected into an agent during a mission to alter one of its IPA levels.

**IPA level**:
One of an agent's three drug-controlled levels — Adrenaline, Perception or Intelligence — each described by an amount, an effect and a dependency.
_Avoid_: Hormone, stim

**Amount**:
The dose of drug set by the player for one IPA level, from 0 to 100 with 50 as neutral.
_Avoid_: Level (alone)

**Effect**:
The part of the amount the agent has absorbed so far; it catches up with the amount before the drug starts wearing off.

**Dependency**:
The tolerance an agent has built up for one IPA level; the further the amount is from it, the stronger the drug acts.
_Avoid_: Addiction

**IPA multiplier**:
The factor, from ×0.5 to ×2, that an IPA level applies to the agent's abilities, derived from the gap between amount and dependency.

**Group mode**:
An IPA change applied to every selected agent at once rather than to a single agent.

**Panic Mode**:
A player shortcut that instantly pushes all three IPA amounts of the selected agents to their maximum; afterwards the levels evolve under the normal rules.
_Avoid_: Panic (alone)

### Pedestrians

**Civilian panic**:
The behaviour of non-agent pedestrians fleeing when a shooting weapon is drawn or fired nearby.
_Avoid_: Panic Mode
