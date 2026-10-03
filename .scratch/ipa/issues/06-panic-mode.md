# 06: Panic Mode

**What to build:** The player presses the left and right mouse buttons together on the map to trigger Panic Mode: all three IPA amounts of every selected, living agent jump to 100. There is no special state afterwards; the levels then evolve under the normal rules (effect catch-up, wear-off, dependency). The kernel exposes a Panic Mode method on the ped; the gameplay menu detects the two-button press and calls it for the selection. The two-button press must not also trigger the single-button map actions (move, shoot). Panic Mode is unrelated to civilian panic, which stays unchanged; keep the names distinct (see `CONTEXT.md`).

**Blocked by:** 01 (Lock in the current IPA behaviour with tests)

**Status:** done

- [x] Calling Panic Mode on a living agent sets Adrenaline, Perception and Intelligence amounts to 100
- [x] Effect and dependency are not changed by the call; they evolve normally afterwards
- [x] Calling Panic Mode on a dead agent does nothing
- [x] In game, left + right on the map applies Panic Mode to every selected agent and to no unselected agent
- [x] In game, the two-button press does not also move or fire
- [x] Civilian panic logic is untouched
- [x] Kernel behaviour covered by ped tests (done); input checked by hand (pending)

**Note:** a click on the map is now held back for 80 ms (`kPanicModeWindowMs` in `GameplayMenu`) so that pressing the other button meanwhile triggers Panic Mode instead of moving or shooting. The tile, target and Ctrl state are captured at press time. Because a short right click can now be released before the shooting action has started, `AutomaticShootAction::stop()` called before the action starts now stops it right after its first shot (before, it was ignored and the weapon kept firing).
