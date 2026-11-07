#ifndef FC_BOT_H
#define FC_BOT_H

#include "game/game.h"

/* Playtest bot (bot.c): plays player 1 and logs what happens. */
void bot_init(Game *g, const char *style, const char *log_path, float minutes);
/* Fills the input for the next tick; false when the run is over (the log is written). */
bool bot_step(Game *g, PlayerInput *in, float dt);

#endif
