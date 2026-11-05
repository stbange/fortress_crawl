#ifndef FC_SFX_H
#define FC_SFX_H

#include "core/fmath.h"
#include "game/sfx_ids.h"

/* Game sounds (sfx.c). The files are made by tools/gen_sfx.mjs with ElevenLabs and live in
 * assets/sfx. Everything here does nothing when there is no audio device or the files are
 * missing, so the game runs the same without them. */

struct Game;
struct PlayerSlot;
struct Monster;

void sfx_init(struct Game *g);
void sfx_shutdown(void);
/* Ambience, loops near fire, lava and water, footsteps, menus (once per frame). */
void sfx_update(struct Game *g, float dt);

/* A sound at a place in the world: quieter with distance, panned for the nearest listener. */
void sfx_at(struct Game *g, int id, vec3 pos, float vol);
/* A sound of a player's own (their weapon, their pain): full volume near them. */
void sfx_player(struct Game *g, const struct PlayerSlot *ps, int id, float vol);
/* Not in the world (menus, fanfares). */
void sfx_ui(int id, float vol);

/* Monster sounds by kind: SFX_<TYPE>_<what>, what = 0 alert, 1 attack, 2 hurt, 3 death. */
enum { MSFX_ALERT, MSFX_ATTACK, MSFX_HURT, MSFX_DEATH };
void sfx_monster(struct Game *g, const struct Monster *m, int what);
/* The sound of an element (cast or impact). */
int sfx_element_cast(int element);
int sfx_element_impact(int element);

void sfx_toggle_mute(void);
bool sfx_muted(void);

#endif
