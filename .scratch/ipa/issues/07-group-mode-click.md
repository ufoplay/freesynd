# 07: Group mode on click

**What to build:** A click on the IPA bar of a selected agent applies the new amount to every selected agent, exactly as dragging already does. A click on the bar of an unselected agent still affects only that agent. Game layer only; there is no automated test harness here.

**Blocked by:** None (can start immediately).

**Status:** ready-for-agent

- [ ] Clicking a bar of a selected agent sets that amount on all selected living agents
- [ ] Clicking a bar of an unselected agent changes only that agent
- [ ] Click and drag share the same group rule (no duplicated logic)
- [ ] Dead agents are ignored
- [ ] Checked by hand in game
