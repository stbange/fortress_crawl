## ADDED Requirements

### Requirement: Special wares by trust
The market SHALL sell its people's own wares at their price, and three special wares: one for friends, one for the honoured and one from the caravan the player paid for (moonsilver, epic weapons, keystones, formula pages, titan hearts...), each once a day.

#### Scenario: Honoured
- **WHEN** an honoured player opens the market
- **THEN** the friends' and the honoured's special wares are listed

### Requirement: The town's works
Crouching before the elder SHALL give gold to the town's next work: rebuild the burned houses (400 gold and more by depth), arm the guards (900 and more), send a caravan (1600 and more); each SHALL give 2 reputation and be seen (a new family, two more guards, the caravan's ware).

#### Scenario: Arms for the guards
- **WHEN** a player pays for the guards' arms
- **THEN** two more guards walk the walls

### Requirement: Kin
A town that calls the player kin (10+) SHALL send one of its people to live in the Sanctum, who SHALL give a gift from home once a day.

#### Scenario: A gift from home
- **WHEN** a player talks to their kin in the Sanctum for the first time in a day
- **THEN** they receive the people's gift

### Requirement: After the king
After the Old King is settled, the towns SHALL celebrate (lights over the plazas, talk of the king's end); after the word ending every town SHALL have two more guards; after the artefact ending new towns start at guest.

#### Scenario: Celebration
- **WHEN** a player enters a town after the king is settled
- **THEN** fireworks burst over the plaza

## MODIFIED Requirements

### Requirement: The healer
The healer SHALL cure wounds, burns, poison, cold and the after-effects of elixirs for 10 gold plus 1 per danger level, half that for friends, free for the honoured, who also get a blessing that takes the next blows.

#### Scenario: A friend healed
- **WHEN** a friend of the town is healed
- **THEN** they pay half the price

## REMOVED Requirements

### Requirement: Market discounts
**Reason**: Discounts made gold pile up further; trust now opens special wares instead (Special wares by trust).

**Migration**: Prices are the people's own for everyone; friends, the honoured and payers of a caravan get the special wares of Special wares by trust.
