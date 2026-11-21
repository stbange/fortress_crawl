## 1. Input
- [ ] 1.1 Attack buffer (120 ms) in combat.c update_player
- [ ] 1.2 Spell buffer (120 ms)
- [ ] 1.3 Throw buffer (120 ms) for X and the gamepad
- [ ] 1.4 Coyote time, jump buffer and apex hang in player.c

## 2. Fairness
- [ ] 2.1 Enemy shot box 0.45 -> 0.25 m, near-miss band to match
- [ ] 2.2 Wind-ups for wisps and shamans; mimic, spider, knight's second blow to 0.35 s
- [ ] 2.3 Melee release slack 0.6 -> 0.25 m

## 3. Visibility
- [ ] 3.1 batch3d_set_rim and the rim term
- [ ] 3.2 Creature light floor 0.3 and the rim in monsters_draw and dragon_draw
- [ ] 3.3 Contrast measured on --demo dragonfight at 3.5:1 or better

## 4. Verification
- [ ] 4.1 Build, test_worldgen 1337 and 42, browser build and smoke test
- [ ] 4.2 Re-run the human-proxy and oracle personas: dropped presses, near-misses, deaths by arrows
- [ ] 4.3 Update docs/FUN_AUDIT.md with the after numbers
