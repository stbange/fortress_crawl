## ADDED Requirements

### Requirement: Animals
Thirteen kinds of animal SHALL live in the world by region and never attack; monsters SHALL ignore them, they SHALL flee when a player comes close (crouching gets closer), and hunted ones SHALL come back after about 15 minutes.

#### Scenario: A hare
- **WHEN** a player walks up to a cave hare
- **THEN** it runs off

### Requirement: Gathering
Animals SHALL give meat when hunted, and some SHALL give without being killed: eggs where hens and quail sit, milk from a goat, honey from a hive smoked with fire first (or the bees sting), jelly from a glow beetle.

#### Scenario: Honey
- **WHEN** a player takes honey from a hive they smoked
- **THEN** they get honey and are not stung

### Requirement: Plants and larders
Plants SHALL grow by region and grow back every 20 minutes, and the larders of built places SHALL hold grain, salt, oil, wine and spices: 36 ingredients in all, each with tags (meat, fish, sweet, sharp, salty, fire, ...).

#### Scenario: Frost berries
- **WHEN** a player looks for food in the north
- **THEN** frost berries grow there

### Requirement: Cooking
Cooking SHALL take two ingredients at any fire or brazier (in the dungeon the smell wakes what lives within 25 m), three at a keep's or hut's hearth or the dining hall's fireplace, four in the Sanctum kitchen; 32 recipes SHALL be made of tags and learned by cooking them.

#### Scenario: A skewer
- **WHEN** a player cooks meat with something sharp
- **THEN** they make a spicy skewer and learn it

### Requirement: Meals
A dish SHALL give a meal of 10 to 20 minutes (health, mana, damage, speed, resistances, faster mana, cheaper spells, a light that shows hidden things); one meal at a time, the same dish twice in a row SHALL do half and a third time a quarter; the pouch SHALL hold four dishes; a raw ingredient SHALL give a small taste of its strongest tag for 90 s.

#### Scenario: The same again
- **WHEN** a player eats the same dish twice in a row
- **THEN** the second does half as much
