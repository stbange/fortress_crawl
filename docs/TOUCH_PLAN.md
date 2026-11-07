# Touch controls: plan for easier steering on phones and tablets

Status: **plan only.** Everything here applies only while the touch controls are active
(`g->touch.active`, set by the first touch, or `--touch`); mouse, keyboard and gamepad play
does not change.

## 1. Why touch is harder today

With mouse and keyboard you move with one hand and aim with the other, precisely and at any
speed, and you attack, cast and drink with spare fingers. On a phone:

1. **Two thumbs, three jobs.** The left thumb moves; the right thumb must both look and
   press buttons. The HIT button doubles as a look drag (good), but casting, the potion,
   jumping and switching weapons all mean lifting the aiming thumb.
2. **Turning is slow and coarse.** A full-width swipe turns about 180 degrees, linearly:
   turning round to face something behind you takes two swipes, and small corrections are
   jumpy on a small screen.
3. **Vertical aim** (pitch) drifts with every swipe and is rarely what a thumb means.
   Monsters a step below, wisps and bats above, need it.
4. **Aim assist is partial.** Bow and staff shots get a 6-degree assist on touch
   (`aim_dir`, combat.c); spells get none (they fly exactly where you look), and melee
   needs you to face the target within the weapon's arc.
5. **Fighting needs precise facing**, now more than before the balance pass: knights must
   be circled, skeleton arrows dodged, and brutes and horned beasts hit hard.
6. **Getting around** in a 3D maze of stairs and corridors with a stick is tiring, and
   the Golden Thread only shows the way.

## 2. Changes

Ordered by how much they help, for the least work.

### T1. Aim assist that matches a mouse (the biggest gain)

- **Facing assist in fights.** While HIT (or CAST) is held and a monster is awake within
  12 m and 35 degrees of the view, the view turns toward it at up to 4 rad/s, with a small
  dead zone. The thumb still steers and can override it. Melee then lands the way a mouse
  player's does.
- **Pitch assist.** The vertical aim follows the target's centre automatically (within
  ±30 degrees); with no target it eases back to level while you walk. Vertical drags are
  damped to a third, so a sideways swipe no longer tilts the view.
- **Assist for spells too.** Projectile spells (bolt, barrage, chain, orb, breath) use the
  same assisted direction as arrows; placed spells (rune, storm, meteor, flare) land on the
  assisted target's feet when one is within the cone.
- **A wider cone on touch:** 6 to 12 degrees, the nearest target in the cone weighted by
  distance, and a faint ring on the target being assisted so the player sees what will be
  hit.

Code: `aim_dir` (combat.c) becomes `assist_target()` shared by combat.c, spells.c and a
new `touch_assist(g, ps, dt)` called from `game_update` before `update_player`.

### T2. Turning that feels like a mouse

- **Acceleration:** the look drag uses `turn = k * dx * (1 + a * |speed|)`, so slow drags
  are precise and quick flicks turn far (a flick across half the screen: 180 degrees).
- **Size by the real screen, not its pixels:** the scale comes from the screen width in
  centimetres (`SDL_GetDisplayContentScale` and the CSS pixel ratio on the web), so a
  phone and a tablet feel the same.
- **A turn-round gesture:** a quick double tap on the look side turns 180 degrees over
  0.2 s.
- **Look sensitivity slider** in the menu (stored in `localStorage` on the web).

### T3. Fewer thumbs needed

- **Auto-attack (on by default on touch):** while a monster is in reach and in front (the
  assist of T1 keeps it there), the weapon swings or fires by itself; HIT stays for
  attacking walls, levers and statues. Turned off in the menu for those who prefer it.
- **CAST and ALT also drag-look**, like HIT, so the aiming thumb never has to lift.
- **Auto-jump:** walking into a one-block step or a gap of one block jumps by itself
  (the player already steps up and ducks automatically).
- **Smart potion:** the POT button glows red below 35% health; a long press drinks mana
  instead.
- **Context button:** near a shrine, a lever or a door the HIT button shows USE.

### T4. Moving with less effort

- **Steering when calm:** with no enemy awake nearby and no finger on the look side,
  pushing the stick sideways turns the view (like a car) instead of strafing; with an
  enemy near it strafes, as now. The camera then follows the way you walk.
- **Follow the thread:** a long press on PATH walks you along the Golden Thread by itself
  (the pathfinding of the playtest bot, `bot.c`, moved into a shared `pathfind.c`), until
  a monster wakes or you touch the stick.
- **Tap to go:** a double tap on the floor walks there by the same pathfinding.

### T5. A layout for thumbs

- Buttons sized in centimetres (at least 9 mm), placed by the measured screen (the
  current layout uses the 480x270 low-res frame, which makes them small on tall phones).
- A one-time layout editor (drag buttons, saved in `localStorage`).
- Menus: swipe left and right to change tabs; rows at least 9 mm high in the shop and
  the inventory.
- Hide buttons that are not usable now (WPN with one weapon, CAST without spells).

### T6. Speed on phones

Steering also suffers from a low frame rate. On the web build: generate chunks over
several frames with a time budget (the web build generates them on the main thread, which
causes hitches while you turn), and a smaller view radius on phones.
Target 30 fps or more on a mid-range phone.

## 3. How to measure it

1. **A touch persona for the playtest bot** (`--bot STYLE --bot-touch`): the bot's view
   turns at most 3 rad/s with some lag and noise, it cannot aim vertically except through
   the assist, and it presses one button at a time while aiming. Run the usual 16-run
   batch three ways: mouse bot, touch bot without assists, touch bot with T1-T4. The goal:
   the touch bot with assists within 15% of the mouse bot in kills per minute, damage
   taken per kill and deaths.
2. **Real devices**: two or three phones (one older), the web build, 15 minutes each with
   and without the assists, noting deaths, fights won and how it felt.
3. An opt-in on-screen counter (only with `?debug` in the URL): fps, input latency, assist
   on or off.

## 4. Order and effort

| Step | Changes | Time |
|---|---|---|
| 1 | T1 aim and pitch assist, spells included; the touch persona of the bot | 3 days |
| 2 | T2 turning (acceleration, real screen size, turn-round, slider) | 2 days |
| 3 | T3 auto-attack, drag-look on CAST/ALT, auto-jump, smart potion, context button | 2-3 days |
| 4 | T5 layout by centimetres, editor, menus | 2-3 days |
| 5 | T4 steering, follow the thread, tap to go (shared pathfinding) | 3-4 days |
| 6 | T6 speed on phones | 2-3 days |
| | **Total** | **about 3 weeks** |

Steps 1-3 give most of the benefit (about a week).

## 5. Open questions

- **Gyro aiming** (tilting the phone for fine aim) works in browsers through
  DeviceOrientation, but iOS asks for permission and SDL's web sensor support needs
  checking first. Worth an experiment after T1, as an option.
- **Should assists also be offered to gamepad players?** They already have the 6-degree
  arrow assist; T1's facing and pitch assist would suit them too, as an option.
- **Fairness in co-op**: touch and mouse players can share a game only in split screen on
  one device today, so assists cannot give anyone an edge over another player.
