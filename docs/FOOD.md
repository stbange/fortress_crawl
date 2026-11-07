# Food and cooking: implementation plan

Status: **plan only.** The only food in the game so far is the feast in the Sanctum (the
pocket realm behind the Sanctum Door, `src/game/realm.c`), which gives the **Well Fed**
buff. This plan grows that into ingredients, cooking and meals. Estimates are for one
developer working full time.

## 1. Goals

- Food is a **reason to explore and to go home**: ingredients come from the dungeon, and the
  best meals are cooked in the Sanctum kitchen.
- Food is a **preparation**, not a second potion. Potions are for the fight; meals are
  eaten before it and last for minutes.
- It stays **small**: about 16 ingredients and 20 recipes, no hunger meter, no spoiling. You
  never have to eat. Eating makes you stronger for a while.
- It fits the world: what you find depends on the region (stratum and flavor), like the
  monsters and weapons do.

Not in scope: hunger or starvation, farming, fishing minigames, food going bad.

## 2. How it plays

1. **Gather.** Ingredients drop from monsters (meat, bone, venom sacs), grow in the world
   (mushrooms on cave floors, herbs in gardens, moss near water) and lie in larders and on
   tables in kitchens and dining halls. You pick them up like potions: walk over them.
2. **Eat raw** (a small effect) or **cook**:
   - at any **campfire**: a brazier, a Hut of Rest fire, the hearth of a Warden's Keep. Simple
     recipes, two ingredients.
   - in the **Sanctum kitchen**, a new room next to the dining hall with a big hearth and a
     stove. All recipes, up to four ingredients, and the meals are better.
3. **Eat** from the inventory (or the quick slot). One **meal** buff at a time; a new meal
   replaces the old. The Sanctum feast stays as the free, full-strength "home cooking":
   Well Fed for 20 minutes.

## 3. Ingredients

Each ingredient has a base (where it comes from) and one or two **tags** that recipes use.

| Ingredient | Tags | Found |
|---|---|---|
| Crawler leg | meat | crawlers, spiders |
| Boar flank | meat, hearty | brutes, horned beasts |
| Bat wing | meat, swift | bats |
| Slime jelly | sweet, mana | slimes |
| Venom sac | poison, sharp | spiders, worms (poison regions) |
| Bone marrow | hearty | skeletons, liches |
| Ember pepper | fire, sharp | hell strata (lava caves) |
| Frost berry | frost, sweet | frozen flavors |
| Cave mushroom | earthy | cave floors everywhere |
| Glow cap | mana, earthy | deep caves, glowing in the dark |
| Wild herb | fresh | gardens, overgrown flavors, the Sanctum garden |
| Sky honey | sweet, holy | heaven strata |
| Grain | staple | larders and sacks in kitchens |
| Salt | staple | mines and dwarf halls |
| Wine | holy, mana | cellars, dining halls |
| Star fruit | rare, all | very far out only (rank 8+) |

Stack size 20 per kind, kept in a separate **larder** list in `Stats` (not the weapon bag):
`u8 food[FOOD_COUNT]`, saved like the potions.

## 4. Recipes and effects

A recipe is a list of tags, not exact ingredients, so the player can improvise: "meat +
sharp" is a spicy skewer whether the meat is crawler or boar. Better ingredients (hearty,
rare) make a better version of the same dish.

| Dish | Needs | Effect (for 10 min; 20 min from the Sanctum kitchen) |
|---|---|---|
| Roast | meat + staple | +15% max health |
| Hunter's stew | meat + earthy + fresh | +10% health, health comes back in fights |
| Spicy skewer | meat + sharp | +10% weapon damage |
| Fire broth | fire + earthy | fire resistance, burn immune |
| Frost sorbet | frost + sweet | frost resistance, no chill |
| Antidote soup | poison + fresh | poison immune |
| Mushroom pie | earthy + staple | +20% mana |
| Glow tea | mana + fresh | mana flows twice as fast |
| Honey cake | sweet + staple | +15% movement speed |
| Bat crisps | swift + sharp | faster attacks (as a weak Haste) |
| Marrow bread | hearty + staple | stun resistance, less knockback |
| Holy wine | holy + mana | spells cost 15% less |
| Traveller's ration | staple + salt | lasts 30 min, small health bonus |
| Feast of the Far Reaches | rare + 3 others | all of the above, a little |

Raw ingredients give a weak, short version of their tag (a frost berry: 1 min of frost
resistance). Unknown recipes are learned the first time they are cooked; the Story tab gets a
**Cookbook** page listing known recipes and which ingredients you hold for them.

Effects reuse what exists: `stats_refresh` already applies Well Fed (+20% max health);
resistances become flags checked where elements hit the player (`player_element_hit`),
speed multiplies `speed_mult` in `combat_update`, mana cost goes through `spell_cast_id` like
the Focus skill does.

## 5. Implementation

### Step 1: ingredients and the larder (2-3 days)
- `src/game/food.c/.h`: the ingredient table (name, tags, colour, model), recipes, the meal
  buff (`Stats.meal`, `meal_t`, `meal_quality`).
- Pickups: a new `PK_FOOD` with a food id. Monster drops: a small chance per kill from a
  per-monster-kind list, scaled by region. World ingredients: placed by the chunk generators
  like other decorations, as pickups spawned from block markers when a chunk loads (the same
  path as chests).
- Save: `food[]`, known recipes (a bit set), the meal and its time. `SAVE_VERSION` 5, older
  saves start with an empty larder.
- Drawing: small models in `monster_draw.c` style (boxes and octahedra), like the feast on the
  Sanctum table.

### Step 2: cooking (3-4 days)
- **Cooking spot**: standing next to a fire block (`B_FIRE`, `B_BRAZIER`), a Hut or Keep hearth
  (`EF_HEARTH`) or the Sanctum stove. A prompt "COOK (E)" appears.
- **The cooking menu**: a new tab in the character menu, shown only at a cooking spot. Pick up
  to two (campfire) or four (Sanctum) ingredients; the result (or "something edible") shows
  before you confirm. Gamepad and touch use the same grid navigation as the inventory.
- A short cooking sound and steam; the dish goes into the larder as a meal item (dishes stack
  too, so you can cook ahead before a long trip).

### Step 3: eating and effects (2 days)
- Eat from the inventory or a quick key (F / D-pad down), with a sound and a banner.
- The buff shows in the status list that utility spells already use (`utility_draw_hud`).
- Hook the effects listed above into `stats_refresh`, `combat_update`, `player_element_hit`
  and `spell_cast_id`.

### Step 4: the Sanctum kitchen (2 days)
- `gen_pocket.c`: a kitchen room east of the dining hall: hearth, stove, hanging herbs, a
  larder with shelves. Its own furniture rectangle in `pocket.h`, like the table and bed.
- `realm.c`: the stove is a cooking spot with four slots and doubled meal duration; the
  Sanctum garden grows wild herbs that come back every in-game day.
- The feast on the dining table stays free; later it could show the meals you cooked last.

### Step 5: sounds, balance, docs (2 days)
- ElevenLabs sounds: pick up food, eat (crunchy, soup, drink), cooking sizzle, a recipe
  learned.
- Balance: a meal should be worth about one skill point for its duration; no meal stacks with
  another. Check drop rates so a run of 20 minutes gives ingredients for 2-3 meals.
- README and PLAN, screenshots of the cooking menu and the kitchen.

**Total: about 2 to 2.5 weeks.**

## 6. Open questions

- Should Well Fed from the feast stack with a cooked meal? (Proposed: no, the feast *is* a
  meal of the best quality, so eating something else replaces it.)
- Co-op: a meal cooked by one player could be shared (drop it like loot for the others).
- Should cooking at a campfire attract monsters (the smell)? A small risk makes the
  Sanctum kitchen more valuable.
