## ADDED Requirements

### Requirement: Drops
Monsters SHALL drop gold, health and mana potions, spell scrolls, weapons and now and then their kind's reagent; picking up SHALL be by walking over them.

#### Scenario: A kill
- **WHEN** a player kills an elite
- **THEN** something drops where it fell

### Requirement: Chests
About a third of the chunks SHALL hold a chest, opened by walking into it; a chest taken SHALL stay taken.

#### Scenario: Reloading
- **WHEN** a player reloads after opening a chest
- **THEN** the chest is still open

### Requirement: Relics
Twelve kinds of relic SHALL permanently add health, damage, mana or speed; duplicates SHALL stack.

#### Scenario: Two of a kind
- **WHEN** a player finds a second relic of strength
- **THEN** its bonus adds to the first's

### Requirement: Potions
A character SHALL carry at most five healing potions (a Draught of Vigor counts as one) and nine mana potions; one more SHALL stay on the ground; potions SHALL be drunk with their keys.

#### Scenario: Full
- **WHEN** a player with five health potions walks over another
- **THEN** it stays on the ground

### Requirement: Weapon bases
There SHALL be twelve weapon bases found in every world (dagger, sword, axe, mace, spear, greatsword; shortbow, longbow, crossbow; wand, staff, scepter) and nine lost arts (whip, flail, scythe, rapier, sling, chakram, hand cannon, tome, censer), of which each world SHALL know four: one per class and one more, chosen by its seed; each base SHALL have its own speed, reach, arc or projectile.

#### Scenario: Lost arts
- **WHEN** two worlds with different seeds are compared
- **THEN** they usually know different lost arts

### Requirement: Rarity and grade
A weapon SHALL have a rarity (common, magic, rare, epic, legendary) and an item level shown as a grade (Fine, Tempered, Masterwork, Runed, Starmetal, Mythic); the odds of better rarities SHALL climb with every step out (from step 4 nothing common, from step 8 nothing below rare), legendaries only from step 6.

#### Scenario: Far out
- **WHEN** a weapon drops at step 9
- **THEN** it is rare or better

### Requirement: Affixes
Magic weapons SHALL carry one affix and rare or better two: Burning, Frozen, Crackling, Venomous, Vampiric, Swift, Keen or Heavy; the first SHALL lean to the region (frost north, fire south, shock east, venom west, swift above, vampiric below).

#### Scenario: A northern sword
- **WHEN** a magic sword drops in the north
- **THEN** it is often Frozen

### Requirement: Materials
A weapon SHALL be made of iron or bronze near the gate and of one of its direction's three materials farther out, the seed choosing which rules there: rime-iron (hits chill), silver (bane of the undead), crystal (more critical hits), embersteel (hits smoulder), obsidian (+20% damage, slower), jade (poison), bone (draws life), starmetal (15% faster).

#### Scenario: Silver
- **WHEN** a silver blade strikes a skeleton
- **THEN** it deals more than against a brute

### Requirement: Powers
Rare and better weapons SHALL carry one or two powers, a trigger and an effect (every third hit lightning leaps to two foes; on a kill a wisp fights beside you; ...).

#### Scenario: A power
- **WHEN** a weapon's trigger happens
- **THEN** its effect fires and the inventory names it

### Requirement: Legendaries
Every world SHALL have twenty named weapons of its own, each with a line of lore and two powers, carried by named enemies from 200 m out; each SHALL be found once.

#### Scenario: Found
- **WHEN** a player kills the named enemy who carries Dawnthorn
- **THEN** Dawnthorn drops, and no other drops it again

### Requirement: Memory
A weapon SHALL count its kills: thirty of one kind SHALL earn it a title (25% more damage against them), and at 75 kills it SHALL awaken a hidden third affix.

#### Scenario: Knightbane
- **WHEN** a sword has killed thirty knights
- **THEN** it is called Knightbane and hits knights harder

### Requirement: Scrapping
A weapon SHALL be scrappable for gold from the inventory; the shop SHALL scrap every weapon in the bag worse than the one worn.

#### Scenario: Scrap
- **WHEN** a player scraps a weapon
- **THEN** it is gone and gold is added
