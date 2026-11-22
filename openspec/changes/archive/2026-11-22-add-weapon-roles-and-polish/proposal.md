# Change: add-weapon-roles-and-polish

## Why
After P2 the fun audit scored 81.5. Choice was held back because the bots, and the game's reasons, used only the blade and attack spells: the bow, the staff and every alternate attack went unused (the alternate attacks were locked behind the last skill of each branch, and no foe asked for a weapon). The line at the top and its hint sat over the view all game, the browser loaded 57 MB before play, and frame times had only been measured in software.

## What Changes
- Weapon roles: arrows deal 1.5x to fliers and shooters; armour (knights, golems, mimics) takes 1.4x from maces, flails, slings and cannons, 1.3x from the staff and spells, and turns other blades (0.8x) and arrows (0.7x); a spark shows which, and a banner says why at most once in five minutes.
- Every weapon has its alternate attack from the start: a small whirl, a power shot (one heavy arrow through three), a small nova; the last skill of each branch makes it the old Whirlwind, Arrow Storm and Nova.
- Z swaps back to the weapon held before; the swing timer is the player's, so a swap neither resets nor adds to it.
- The bots switch weapons for the foe and use alternate attacks (the spam bot still only swings); the FUN line counts the swaps.
- Less text at the top: the line dims to a third after 20 s unchanged and brightens when it changes or on Y or G; the "(Y: ANOTHER LINE)" hint shows only in the first five minutes and for 10 s after a new line opens.
- The browser build packs its sounds as IMA ADPCM (24 MB to 6 MB) and fetches the music (8 MB) after the first frame; it logs its TIMING lines in the console.

## Impact
- Affected specs: guidance, playtest-bots, weapon-roles, web-loading
- Affected code and docs: src/game/combat.c, spells.c, bot.c, game.c, game.h, skills.c, skills.h, touch.c, king.c, quests.c, town.c, sfx.c; CMakeLists.txt, web/build.sh; README.md, docs/FUN_AUDIT.md.
