#ifndef FC_STORY_TEXT_H
#define FC_STORY_TEXT_H

/* The story's small template engine and name makers (story.c), shared with the
 * history's text (history_text.c). Templates use {key} for context values and #symbol#
 * for a pick from a word list. */

#include "core/common.h"

typedef struct Ctx { const char *key[48]; const char *val[48]; int n; } Ctx;

void story_cset(Ctx *c, const char *k, const char *v);
/* Expands t into out, with a capital at the start of each sentence and quotation. */
void story_expand(char *out, int n, const char *t, Rng *r, const Ctx *c);

void story_make_name(u64 h, int cult, char *out, int n);
void story_make_named(u64 h, int cult, char *out, int n);
void story_faction_name(u64 seed, int cult, int idx, char *out, int n);

#endif
