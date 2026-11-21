## 1. Input
- [x] 1.1 Attack buffer (120 ms) in combat.c update_player
- [x] 1.2 Spell buffer (120 ms)
- [x] 1.3 Throw buffer (120 ms) for X and the gamepad
- [x] 1.4 Coyote time, jump buffer and apex hang in player.c

## 2. Fairness
- [x] 2.1 Enemy shot box 0.45 -> 0.25 m, near-miss band to match
- [x] 2.2 Wind-ups for wisps and shamans; mimic, spider, knight's second blow to 0.35 s
- [x] 2.3 Melee release slack 0.6 -> 0.25 m

## 3. Visibility
- [x] 3.1 batch3d_set_rim and the rim term
- [x] 3.2 Creature light floor 0.3 and the rim in monsters_draw and dragon_draw
- [x] 3.3 Contrast measured on --demo dragonfight at 3.5:1 or better

## 4. Verification
- [x] 4.1 Build, test_worldgen 1337 and 42 (with new jump checks), browser build and smoke test
- [x] 4.2 Re-run the human-proxy, oracle and spam personas: dropped presses, near-misses, deaths by arrows
- [x] 4.3 Count presses near readiness in the FUN line; fix the buffer's accounting
- [x] 4.4 Update docs/FUN_AUDIT.md with the after numbers
