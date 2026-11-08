# Food and cooking: implementation plan

Status: **implemented** (November 2026): `src/game/wildlife.c` (the animals), `src/game/food.c`
(ingredients, recipes, the larder, cooking, meals, the FOOD tab), plants and larders in
`items.c`, the garden's food in `realm.c`, save version 8. Differences from the plan below:
eggs lie where hens and quail sit rather than in separate nests; the Sky-carp pie makes you
drift down slowly instead of a short glide; the cookbook is part of the FOOD tab (N), not the
Story tab; a cooked meal keeps its place when you walk past the Sanctum's table (the feast
heals you but does not replace it).

## 1. Goals

- Food is a **reason to explore and to go home**: ingredients come from the world, and the
  best meals are cooked in the Sanctum kitchen.
- Food is a **preparation**, not a second potion. Potions are for the fight; meals are
  eaten before it and last for minutes.
- **Monsters drop no food.** Food comes from the living world around the fighting: neutral
  animals, plants, and the larders of the fortress. The dungeon feels inhabited by more
  than things that want to kill you.
- Each region tastes different: what lives and grows there depends on the stratum (up and
  down) and the style (north, south, east, west), like the monsters and weapons do. A dish
  from far away is a souvenir of the trip.
- No hunger meter, no spoiling, no farming minigame. You never have to eat. Eating makes you
  stronger for a while.

## 2. How it plays

1. **Gather.**
   - **Hunt** small game (one or two hits, best with a bow).
   - **Collect** without killing: eggs from nests, milk from goats, honey from hives, fish
     from pools.
   - **Pick** plants that grow on the floors and walls (mushrooms, herbs, berries).
   - **Raid** larders, cellars and kitchens for grain, salt, oil, wine and spices.
   Everything goes into the **larder**, a list of its own beside the weapon bag.
2. **Eat raw** for a small, short effect, or **cook**:
   - at any **campfire** (a brazier, a Hut of Rest fire, the hearth of a Warden's Keep):
     two ingredients, the simple dishes, meals of 10 minutes;
   - at the **dining hall's fireplace** in the Sanctum (dining stage 2): three ingredients,
     15 minutes;
   - in the **Sanctum kitchen** (dining stage 3): four ingredients, every recipe, 20
     minutes, and better versions of each dish.
3. **Eat** from the larder or a quick key. One **meal** at a time; a new one replaces the
   old. The feast on the dining table counts as a meal (home cooking, Well Fed), so eating
   something else replaces it.

## 3. Wildlife

Neutral animals: they never attack, monsters ignore them, and they give no XP. They graze,
wander, roost and flee. A few kinds defend themselves weakly (a goat butts you back, bees
sting) so that there is a right way to do it.

| Animal | Lives | Behaviour | Gives |
|---|---|---|---|
| Cave hare | mines, roots, castle courtyards | grazes in pairs, bolts when you come within 6 m | hare haunch (hunted) |
| Fortress hen | castle kitchens and courts, the Sanctum garden | pecks about, flutters off when chased | eggs from its nest (collect, every 10 min); fowl (hunted) |
| Mountain goat | eagle aeries, spires, steppe | climbs ledges no player reaches, butts back once if hit | goat milk (walk up slowly and use, every 10 min); goat meat (hunted) |
| Cloud ram | cloud cloisters, hanging gardens | drifts on cloud floors in small flocks | mutton (hunted) |
| Blind cave fish | underground pools, drowned and fungal places | schools that scatter when you enter the water | cave fish (strike in the water, or freeze the pool) |
| Sky carp | heaven, sun court, the orrery | floats in the air in slow circles | sky carp (shot down) |
| Shore crab | coral, brine, pearl and drowned styles | scuttles sideways, hides in its shell (hit twice) | crab |
| Ember salamander | inferno, ember, phoenix, sulphur | basks on lava edges, dives into lava if scared | salamander tail (only if caught before it dives) |
| Frost grouse | frostbitten, rime-iron, glacial, everwinter | sits still in snow, explodes into flight | grouse (hunted) |
| Glow beetle | fungal deep, crystal abyss | crawls on walls, lights the cave | beetle jelly (collect from a resting beetle, it flies off) |
| Salt snail | salt and mines | slow, leaves a glittering trail | snail (picked up) |
| Honeybees | hives in hanging gardens, druidic and bamboo styles | a cloud round the hive; if disturbed, stings for a few seconds | honeycomb (smoke the hive first: any fire spell or a torch nearby calms it); royal jelly (rare, heaven hives only) |
| Steppe quail | steppe, sandstone, sunscorched | runs in coveys, hides in grass | quail eggs (nests), quail (hunted) |

Rules:
- **Scarce, not endless.** A chunk holds a few animals (seeded like monsters). Hunted ones
  come back after about 15 minutes, collected ones (eggs, milk, honey) refill on a timer.
  You cannot strip an area for good, and you cannot farm one spot forever.
- **Your spells matter.** *Lure* draws animals as it draws monsters. *Reveal the Hidden*
  shows nests and hives. *Frost* spells on a pool float the fish up. Fire calms bees.
  Invisibility lets you walk up to anything.
- **No lock-on.** Auto-aim and target lock skip animals; hitting one is always a choice.
- **The Sanctum garden keeps animals** (see section 6): hens at stage 2, beehives and a fish
  pond at stage 3.

## 4. Ingredients

Each ingredient carries one to three **tags** that recipes use. 36 ingredients in four
groups.

**From animals (13)**

| Ingredient | Tags | Source |
|---|---|---|
| Hare haunch | meat, lean | cave hare |
| Fowl | meat | fortress hen |
| Grouse | meat, frost | frost grouse |
| Quail | meat, swift | steppe quail |
| Goat meat | meat, hearty | mountain goat |
| Mutton | meat, hearty, fat | cloud ram |
| Salamander tail | meat, fire | ember salamander |
| Hen egg | egg | fortress hen nests |
| Quail egg | egg, swift | steppe quail nests |
| Goat milk | dairy | mountain goat |
| Cave fish | fish | blind cave fish |
| Sky carp | fish, holy | sky carp |
| Crab | fish, salty | shore crab |

**From hives and small creatures (4)**

| Ingredient | Tags | Source |
|---|---|---|
| Honeycomb | sweet | hives |
| Royal jelly | sweet, rare, mana | heaven hives (rare) |
| Beetle jelly | sweet, mana, glow | glow beetles |
| Salt snail | earthy, salty | salt snails |

**Growing (13)**

| Ingredient | Tags | Found |
|---|---|---|
| Cave mushroom | earthy | cave floors everywhere |
| Glow cap | earthy, mana, glow | deep caves, glowing in the dark |
| Wild herb | fresh | gardens, druidic and overgrown styles, the Sanctum garden |
| River moss | fresh, earthy | beside underground water |
| Root tuber | staple, earthy | the roots stratum |
| Bamboo shoot | fresh, staple | bamboo style |
| Jade leaf | fresh, mana | jade and silk styles |
| Ember pepper | fire, sharp | inferno, ember, sulphur |
| Brimstone onion | sharp, fire | sulphur, obsidian |
| Frost berry | frost, sweet | frozen styles |
| Sun melon | sweet, fresh | sunscorched, sandstone, sunblast |
| Sea grape | fresh, salty | coral, brine, pearl |
| Barrow root | bitter | barrow, druidic, ossuary |

**From larders, cellars and kitchens (6)**

| Ingredient | Tags | Found |
|---|---|---|
| Grain | staple | sacks in kitchens and storerooms |
| Salt | salty | mines, salt style, dwarf halls |
| Oil | fat | kitchens, imperial and terracotta styles |
| Wine | holy, mana | cellars, dining halls, gothic and boyar styles |
| Trade spices | sharp, rare | imperial, silk and brass styles, rare |
| Star fruit | rare, all tags | very far out only (rank 8+), one per chunk at most |

Stacks of 20 per kind in `Stats.food[FOOD_COUNT]`; known recipes as a bit set.

## 5. Recipes and effects

A recipe is a list of **tags**, not exact ingredients, so the player can improvise: "meat +
sharp" is a spicy skewer whether it is hare or salamander. Better ingredients make a better
version of the same dish: *hearty*, *rare* or an ingredient from far out (tier 6+) raise the
quality by one step, up to three steps (the dish is named "fine", "excellent", "legendary"
and its effect grows by a quarter per step). The cooking place sets how long it lasts: 10
minutes at a campfire, 15 at the dining hall's fireplace, 20 in the kitchen.

### Campfire dishes (two ingredients, 14)

| Dish | Needs | Effect |
|---|---|---|
| Roast | meat + salty | +15% max health |
| Spicy skewer | meat + sharp | +10% weapon damage |
| Grilled fish | fish + fresh | health comes back twice as fast out of a fight |
| Fried eggs | egg + fat | +10% max health, +10% max mana |
| Fried caps | earthy + fat | +20% max mana |
| Glow tea | mana + fresh | mana flows twice as fast |
| Honey bread | sweet + staple | +15% movement speed |
| Flatbread | staple + salty | travel food: lasts three times as long, +5% max health |
| Frost sorbet | frost + sweet | frost resistance, never chilled |
| Pepper jerky | meat + fire | fire resistance, burns end at once |
| Bitter tonic | bitter + fresh | poison immunity |
| Sweet curd | dairy + sweet | potions heal 30% more |
| Crab on the coals | fish + salty | projectiles hurt 15% less |
| Glowing broth | glow + salty | a soft light round you; hidden things show within 8 m |

### Fireplace dishes (three ingredients, 10; the dining hall at stage 2 or the kitchen)

| Dish | Needs | Effect |
|---|---|---|
| Hunter's stew | meat + earthy + fresh | +10% max health, health comes back in fights |
| Omelette | egg + dairy + fresh | +8% weapon damage, +8% max health |
| Mushroom pie | earthy + staple + fat | +25% max mana, spells 5% stronger |
| Fire broth | fire + earthy + salty | fire resistance and lava burns half as much |
| Honey cake | sweet + staple + egg | +20% movement speed, jump a little higher |
| Milk porridge | dairy + staple + sweet | stun resistance, half knockback |
| Root mash | staple + dairy + salty | +20% max health |
| Sea-grape salad | fresh + salty + fish | poison immunity, health comes back slowly |
| Grouse pot | frost + meat + earthy | frost resistance, +10% max health |
| Quail on a stick | swift + meat + fire | attacks 10% faster |

### Kitchen dishes (four ingredients, 8; the kitchen only)

| Dish | Needs | Effect |
|---|---|---|
| Salamander curry | fire + sharp + meat + staple | +15% weapon damage, hits sometimes set foes alight |
| Dragon's-breath chili | fire + sharp + sharp + fat | +25% weapon damage, but you take 10% more |
| Pilgrim's supper | holy + mana + staple + fresh | spells cost 15% less |
| Sky-carp pie | fish + holy + staple + fat | falls hurt half, you glide a moment after a jump |
| Jade tea ceremony | mana + fresh + sweet + glow | mana flows twice as fast, spells 10% stronger |
| Royal tart | rare + sweet + dairy + staple | spell cooldowns 20% shorter |
| Fisherman's chowder | fish + dairy + salty + earthy | +15% max health, +15% max mana |
| Feast of the Far Reaches | star fruit + any three | a little of everything: +10% health, mana, damage, speed |

**32 recipes** in all. Raw ingredients give a weak, short version of their strongest tag (a
frost berry: 1 minute of frost resistance; a honeycomb: a little health). Something cooked
from tags that fit no recipe is "a simple meal": +5% max health.

Recipes are learned the first time they are cooked. The **Cookbook** page in the Story tab
lists the known ones, greys out the others with their tags hinted ("meat + ? + ?"), and shows
which ingredients you hold for each. Old cookbooks found in kitchens and libraries reveal a
recipe without cooking it.

Effects reuse what exists: `stats_refresh` already applies Well Fed (+20% max health);
resistances become checks where elements hit the player (`player_element_hit`); speed
multiplies `speed_mult` in `combat_update`; mana cost and spell power go through
`spell_cast_id` like the Focus skill.

## 6. Food in the Sanctum

The new Sanctum (`docs/SANCTUM_PLAN.md`) gives food a home without new plots:

| Plot and stage | Food |
|---|---|
| Dining hall 1 | the table: the free feast, Well Fed 10 min (as now) |
| Dining hall 2 | the fireplace: cook three-ingredient dishes, 15 min; feast 20 min |
| Dining hall 3 | the kitchen: four-ingredient dishes, 20 min, the larder shelves show what you hold; feast 30 min |
| Garden 1 | wild herbs grow back every 20 minutes |
| Garden 2 | a henhouse: three hens, eggs every 10 minutes; fruit trees (sun melon, frost berry by the island's style) |
| Garden 3 | beehives (honeycomb) and a fish pond (cave fish) |
| Library 2+ | the old cookbooks you find can be read here (one recipe each) |

The feast on the table could later show the dishes you cooked last.

## 7. Implementation

### Step 1: wildlife (4-5 days)
- `src/game/wildlife.c/.h`: a `Critter` pool apart from monsters (about 96), kinds from the
  table above, spawned per chunk from the seed like monsters, by stratum and style. States:
  graze, wander, flee, roost, swim, fly; a hive is a fixed critter with a swarm round it.
- Drawing in the `monster_draw.c` style, small and simple (boxes, a few moving parts), with
  the region's colours.
- Hits from weapons and spells (the same hit test as monsters); auto-aim and lock skip them;
  monsters ignore them.
- Collecting (eggs, milk, honey, beetle jelly) with the use key, with per-critter timers.
  Respawn after 15 minutes for chunks out of view.
- Sounds (ElevenLabs): hen clucks, goat bleat, wings, bees, a splash; quiet, ambient.

### Step 2: ingredients and the larder (2-3 days)
- `src/game/food.c/.h`: the ingredient table (name, tags, colour, model, regions), recipes,
  the meal buff (`Stats.meal`, `meal_t`, `meal_quality`).
- Pickups: `PK_FOOD` with a food id, from critters and from plants placed by the chunk
  generators as block markers (the same path as chests). Larder sacks and shelves in
  kitchens and storerooms.
- Save: `food[]`, known recipes, the meal and its time, critter timers by chunk;
  `SAVE_VERSION` 8, older saves start with an empty larder.

### Step 3: cooking (3-4 days)
- **Cooking spot**: next to a fire (`B_FIRE`, `B_BRAZIER`), a Hut or Keep hearth
  (`EF_HEARTH`), the dining hall's fireplace or the kitchen stove. A prompt "COOK" appears.
- **The cooking menu**: a COOK tab in the character menu, shown only at a cooking spot. Pick
  two, three or four ingredients by place; the dish shows before you confirm. Gamepad and
  touch use the inventory's grid navigation.
- A sizzle and steam; the dish goes into the larder (dishes stack, so you can cook ahead).

### Step 4: eating and effects (2 days)
- Eat from the larder or a quick key (F / D-pad down), with a sound and a banner.
- The meal shows in the status list beside the utility spells (`utility_draw_hud`).
- Hook the effects into `stats_refresh`, `combat_update`, `player_element_hit` and
  `spell_cast_id`. The feast becomes a meal.

### Step 5: the Sanctum (2 days)
- `gen_pocket.c`: the fireplace at dining stage 2 is a cooking spot; the kitchen at stage 3
  gets a stove, hanging herbs and larder shelves; the garden gets a henhouse (stage 2),
  hives and a pond (stage 3).
- `realm.c`: the cooking spots, the garden's harvests on timers, the hens and bees as
  critters.
- The SHOP tab's building lines name the food each stage brings.

### Step 6: bot, balance, docs (2 days)
- The playtest bot hunts what crosses its path, cooks at fires when it holds a recipe, and
  eats before fights; log what it eats and how often.
- Balance: a meal is worth about one skill point while it lasts; no two meals stack. A run
  of 20 minutes should give ingredients for 2-3 meals.
- README and PLAN, screenshots of wildlife, the cooking menu and the kitchen.

**Total: about 3 to 3.5 weeks.**

## 8. Open questions

- **Co-op**: a dish cooked by one player could be shared (drop it like loot for the others),
  or one pot could feed everyone at the fire.
- **Cooking at a campfire**: should the smell draw monsters? A small risk makes the Sanctum
  kitchen more valuable.
- **Taming**: later, could a hare or a goat follow you home to the Sanctum garden?
- **Fishing**: striking fish in the water is enough for now; a rod could come later.
