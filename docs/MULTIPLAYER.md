# Multiplayer: implementation plan

Status: **plan only, nothing implemented yet.** This describes how to turn the local
split-screen co-op into online co-op for the Linux and Windows versions, with a server.
Estimates are for one experienced developer working full time.

## 1. Goal and scope

- Co-op (players against the dungeon) over LAN and the internet, **2 to 4 players** first,
  designed so that 8 is a configuration change later.
- **Linux and Windows** clients. The server runs on Linux and Windows.
- One program, three ways to run it: **single player** (as today), **host** (a player's game
  is also the server, friends join), **dedicated server** (no window, no sound, runs on its
  own).
- Split screen keeps working, and a split-screen client can join a server with several
  local players.

Not in scope: the web version (browsers cannot use UDP; that would need WebSockets or
WebRTC and a bridge), player against player, anti-cheat, matchmaking or a server list.

## 2. Decisions

| Question | Recommendation | Why |
|---|---|---|
| Who decides what happens | **The server, always** (authoritative server) | The simulation uses time-based and static random numbers (about 30 places) and float physics, so two machines cannot run in lockstep. The server simulates, the clients show it. |
| Network library | **ENet** (MIT, one small C library, Linux and Windows) | Reliable and unreliable channels over UDP, connection handling, fragmentation of big messages. SDL3_net would also work but only gives plain sockets: reliability, ordering and fragmentation would have to be written by hand. Plain TCP is simplest but stalls everything behind a lost packet. |
| Host or dedicated | **Both, same code**: the dedicated server is the host without window and sound | Friends can just host; a dedicated server can keep a world running. |
| Who keeps the characters | **The server**, per world, keyed by player name and a secret token the client keeps | The same save format as today (save.c), moved to the server. |
| Loot | **Per player**: each player sees and picks up their own drops from a kill | No fights over drops, no ownership bookkeeping for others' items. Chests per player too. Gold and potions likewise. |
| Rates | Simulation 60 Hz (as now), client input sent every tick, **world updates 20 Hz** | Standard for action games of this size; the client smooths between updates. |

## 3. Architecture

```
 client                                   server (or host)
 ------                                   ----------------
 input -> predict own player  --input-->  game_update (60 Hz), all players
 draw world from seed                     chunk store (blocks only, no meshes)
 draw entities from snapshots <-snapshot- interest per player: what is near
 play events (sound, particles) <-events- events from the simulation
 menus send commands          --cmd---->  inventory, skills, potions, story
```

- **The world itself is never sent.** Both sides generate chunks from the seed. Only
  changes travel: gates opened, chests and relics taken, puzzle state, explored map.
- **Snapshots** carry what each player can see (within about 64 m): players, awake and
  nearby monsters, projectiles, pickups, spell effects, devices (levers, statues, mirrors,
  traps, gates).
- **Events** carry everything that is presentation: sounds, particle bursts, flashes,
  banners, "you picked up", "level up". The client plays them; the server does not.
- **Commands** carry what a player decides outside the 60 Hz input: equip, scrap, learn or
  select a spell, buy a skill, drink a potion, pick a motive, cast the golden thread.

## 4. Phases

### Phase 0: Windows build in CI (2-3 days)

- Add a Windows job (MSVC, CMake, SDL3 fetched) to `.github/workflows/build.yml`.
- Fix what MSVC finds (it already gets `/W3 /D_CRT_SECURE_NO_WARNINGS`; expect a handful of
  C11 and POSIX differences, e.g. file paths in save.c and `strncasecmp`).
- Done when: the game builds and the world tests pass on Linux and Windows in CI.

### Phase 1: separate simulation from presentation (1-2 weeks)

Everything here is useful on its own and keeps single player unchanged.

1. **Event queue instead of direct calls.** The simulation files call presentation
   directly: `particles_burst` about 60 times, `flash_add` about 35, `sfx_*` about 110,
   `game_banner` about 40 (combat.c, monsters.c, spells.c, items.c, mech.c, quests.c,
   game.c, weapons.c). Replace them with `event_push(g, EV_SOUND/EV_PARTICLES/EV_FLASH/
   EV_BANNER, ...)` into a per-tick queue. In single player the queue is played back at once
   by a new `present.c`, so nothing changes on screen.
2. **Chunk store without the renderer.** `chunkmap.c` both loads chunks and builds their
   GL meshes (about 30 renderer calls). Split it into `chunkstore` (generate, keep blocks,
   lights, `block()`, `get()`, `settled()`; no GL) and `chunkmesh` (mesh and draw, client
   only). `chunkmap_update` already takes several centres, so the server can keep chunks
   around every player.
3. **No hidden state in the simulation.** Move static random states (`g_loot_rng`,
   `static u64 rs/st` in spells.c, monsters.c, weapons.c) and static timers (`pick_cool`,
   `nav_cool` in game.c) into `Game` or `PlayerSlot`. Seeds from `g->time` stay allowed (the
   server owns them), but every random number comes from state the server holds.
4. **A headless build target** `fortress_server`: links fcworld, the chunk store and the
   game simulation, not the renderer, font, batches or audio.
5. **Inputs per player per tick** become a plain struct (movement, look angles, buttons
   held, buttons pressed this tick) so the same struct can come from the keyboard, a pad,
   the touch controls, a test script or the network.

Done when: single player, split screen and every `--demo` look and sound as before; a test
runs `fortress_server` headless for 10 minutes with two scripted players fighting and
looting, without crashing.

### Phase 2: network layer and protocol (1-1.5 weeks)

1. **`net.c`** wraps ENet: host/listen, connect, three channels (0 unreliable snapshots,
   1 reliable ordered events and commands, 2 reliable bulk such as the explored map and
   the character on join), a simple message writer/reader (little-endian, quantised floats).
2. **Join handshake**: client sends protocol version, player name and token; server answers
   with seed, seed text, world state (looted keys, solved vaults, gates), the player's slot
   and character (from its save or a new one). A version mismatch is refused with a message.
3. **Messages**

   | Message | Direction | Channel | Content |
   |---|---|---|---|
   | Input | client to server | unreliable, every tick, last 3 ticks repeated | tick number, move, look, buttons |
   | Snapshot | server to client | unreliable, 20 Hz | server tick, last input processed, entities in range (delta against the last acknowledged snapshot) |
   | Events | server to client | reliable | sound id and position, particles, flash, banner, pickup, level up, death |
   | Command | client to server | reliable | equip, scrap, learn, select spell, buy skill, potion, motive, thread |
   | Stats | server to client | reliable, on change | the player's full Stats (HP, mana, inventory, spells, missions) |
   | World change | server to client | reliable | chest taken, gate opened, device turned, lore found |

4. **Interest management**: each client gets entities within about 64 m of its players,
   sorted by distance, capped per snapshot.

Done when: two clients on a LAN join one server, see each other walk and see the same
monsters (no prediction yet, so their own movement lags a little).

Rough sizes per snapshot after quantisation: player 24 bytes, monster 16 (position,
yaw, state, health fraction, type, grade, element, flags), projectile 14, pickup 12. With
40 monsters and 30 projectiles near a player that is about 1.2 KB, about 25 KB/s at 20 Hz
before delta compression. Delta against the last acknowledged snapshot usually saves
60-80%.

### Phase 3: the client feels local (1.5-2 weeks)

1. **Prediction of your own movement**: run `player_update` (deterministic for the same
   input and blocks) locally right away, keep the inputs not yet confirmed, and when a
   snapshot confirms tick N, start from the server's position and replay the inputs after N.
   Smooth small corrections over a few frames; snap large ones.
2. **Interpolation of everything else**: draw other players, monsters and projectiles about
   100 ms in the past, between the two snapshots around that time.
3. **Instant feedback for your own actions**: swings, shots, casts, their sounds and the
   weapon animation play at once on the client; damage, kills and loot come from the server.
   Your own projectiles are drawn from the moment you fire, then replaced by the server's.
4. **Menus as commands**: the inventory, spell book, skill tree, motive picker and story tab
   read the Stats the server sent and send Commands instead of changing Stats directly.
5. **Mechanisms**: levers, statues and mirrors are used by attacking them, so they work
   through the normal input; their state comes back in snapshots.

Done when: with 100 ms of simulated lag your own movement and attacks feel the same as
offline; others move smoothly.

### Phase 4: game systems for several players (1-1.5 weeks)

- **More than 4 players**: `MAX_PLAYERS` is 4 and sizes several arrays and the flow fields
  (one per player, 48 x 48 x 16 u16). Raise it to 8 and check the monster targeting and
  flow update cost.
- **Joining and leaving**: a joining player appears at their saved shrine (or the gate);
  a leaving player's character is saved and their monsters' targets reset.
- **Loot per player** (see decisions): drops carry an owner, chests are taken per player.
- **Story per player**: motives, chapters, missions and lore are already per character;
  named enemies are shared, and `story_on_kill` already handles "someone else was faster".
- **Death and respawn** as now, per player; the shrine near the respawn is the player's.
- **Music and ambience** follow the local player on each client (already the case).

### Phase 5: hosting, saving and menus (1 week)

- Start screen: **Single player / Host / Join** (address and port, player name). Dedicated
  server: `fortress_server --seed words --port 27960 --save-dir path`.
- The server saves every 30 s and when a player leaves (the save format of save.c, one
  record per player name; the token prevents taking over someone else's character).
- The client remembers recent servers and its token per server.
- Optional: find servers on the LAN by a UDP broadcast. Over the internet the host needs an
  open port (router port forwarding); document this.

### Phase 6: testing and hardening (1 week, partly alongside)

- **Network trouble on purpose**: a switch in net.c that delays, reorders and drops packets
  (for example 150 ms, 5% loss) for tests.
- **Bots**: scripted clients (the `--test-pad` script driven over the network) that join,
  walk, fight and leave, for soak tests with 4-8 players.
- **Determinism checks** for prediction: replay recorded inputs offline and on the server,
  compare player positions.
- Timeouts, reconnect after a short drop, a server that never trusts a client's position,
  damage or inventory.

## 5. Effort

| Phase | Time | Playable at the end |
|---|---|---|
| 0 Windows CI | 2-3 days | |
| 1 Simulation and presentation apart | 1-2 weeks | single player as before |
| 2 Network and protocol | 1-1.5 weeks | **LAN co-op**, own movement lags a little |
| 3 Prediction and smoothing | 1.5-2 weeks | **internet co-op that feels right** |
| 4 Several players in the game systems | 1-1.5 weeks | up to 8 players, joining mid-game |
| 5 Hosting, saving, menus | 1 week | host and join from the start screen |
| 6 Testing and hardening | 1 week | |
| **Total** | **about 7-10 weeks** | |

## 6. Risks and open questions

- **Prediction against changing blocks**: gates and statues change what is solid
  (`mech_solid`). Predicted movement must use the same solid cells as the server; send
  device state reliably and early.
- **Monster cost with many players**: monster AI, flow fields and chunk streaming scale with
  players; 8 players spread far apart means 8 areas of loaded chunks on the server.
- **Story with several players** in different chapters is per character today and should
  stay that way; shared named enemies need a clear rule for who gets the chapter (today:
  everyone who has them as a target, "someone else was faster" for the others).
- **Two codebases in one**: keep single player running through the same event queue and
  input structs, so there is one game, not two.
- **ENet** must be fetched like SDL3 (CMake FetchContent) or vendored as its source files.
