#ifndef FC_SUPPORT_H
#define FC_SUPPORT_H

#include "core/common.h"
#include "world/block.h"

/* Weight and collapse (docs/SIMULATION.md): which blocks lose their support when some are
 * removed. A block is held if it rests on a held block, or reaches one sideways or from above
 * within its material's span (Substance.span: earth 1, wood 4, masonry 5, rock 6, metal 8).
 * The box round the break is checked twice, with the removed cells as they were and as they
 * are: only what was held before and is not now falls, so the world as it was made stands even
 * where it spans further than these rules allow. Cells on the edge of the box and cells the
 * caller fixes (protected places) hold. */

/* What is at a cell: its block material (B_AIR or a non-solid for nothing), -1 if not known. */
typedef int (*SupportBlockFn)(void *user, int x, int y, int z);
/* Cells that always hold (protected); may be NULL. */
typedef bool (*SupportFixedFn)(void *user, int x, int y, int z);

typedef struct SupportCell { int x, y, z; } SupportCell;

/* removed[i] were mat[i] before the break. Writes up to max cells that fall into out (top first)
 * and returns how many. r: the box's half size (at most SUPPORT_MAX_R). */
#define SUPPORT_MAX_R 8
int support_falls(SupportBlockFn block, SupportFixedFn fixed, void *user, int cx, int cy, int cz, int r,
                  const SupportCell *removed, const u8 *mat, int nremoved, SupportCell *out, int max);

#endif
