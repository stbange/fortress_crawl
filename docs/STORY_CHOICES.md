# Fortress Crawl: Choices and Consequences

The player's deeds change the story: whom they kill, spare or deal with, which lords they
bring down, and how the mystery ends. This builds on the generated histories
([STORY_GEN.md](STORY_GEN.md)): the history is where the world starts, and a ledger of what the
players did is laid over it. The generator and every story text read both.

Status: built (save version 14). Worlds with generated stories get all of it. Older worlds get
the changed domains, the epithet and the deeds list for lords.

Code: the ledger and the world's answers in `src/world/story.c` (`story_deed`, `story_ledger`,
domains, posters, orders, epithets, the deeds list); standing, grudges and the sequel arcs in
`src/world/history.c`; their texts in `src/world/history_text.c`; the choice itself, hunters,
residents and the STORY tab in `src/game/quests.c`. `worldmap choices [policy]` prints every
plea, every choice, the deeds list, the three endings and the arcs that follow each.

## Decisions taken

- **Choices are permanent.** There is no undo; the game saves as it goes.
- **They belong to the world.** In co-op the first player to choose decides for everyone, as
  kills already did. Each character still has their own motive and chapter.
- **Bots choose by a fixed policy** (their style, the chapter and the seed), so playtests cover
  every choice.

## 1. Choosing at the people in the story

A chapter target of the mystery (the first nine) that drops below 15% of its health yields
instead of dying: it stops fighting, kneels, and the banner says so ("ISHUSUT YIELDS - GO TO
THEM AND CHOOSE"). Standing within a few metres opens the choice. Alone, the game pauses; in
co-op, monsters wait while someone chooses.

The target pleads in its own words, chosen from its part in the history and its traits: the
blamed exile swears it did not open the seal, a coward begs ("I only did what Gerwyn told
me!"), a proud one dares you, a greedy one names a price. Then three choices:

| Choice | What you get | What it costs |
|--------|--------------|---------------|
| **Kill** | the motive's reward, the revelation, the legendary it carries | its people remember: they hunt you harder |
| **Spare** | the motive's reward, the revelation (said as it goes, not found on a body) and one more truth it tells you quietly | some are grateful and come back to help, some bear a grudge and come back to kill you |
| **Bargain** | the motive's reward, the revelation, gold and the legendary it carries, handed over | its people let you pass; their enemies hunt you |

The finale (the ninth target, the opener or whoever speaks for them) has its own three:
**kill** them at the seal, **spare** them and let them finish what they began, or **take the
key** (three keystones, and the seal answers to you). Each ends with an epilogue that depends
on the choice and the world's tone ("You let Thyvik go. The bones below will be seen at last,
and the Grey Abbots will never forgive you for it.").

Killing a target outright, before it yields, counts as a kill.

## 2. The world remembers

- **The deeds list:** the STORY tab has "WHAT YOU DID - THEY CALL YOU ...", newest first, each
  deed with what came of it: "You spared Ishusut. Ishusut is grateful, and waits in your
  Sanctum." / "You took Tamusut's offer. The Drowned Hand hunt you for it." / "You killed
  Amesut the Quiet. The Vaults of Nefushar stands empty."
- **An epithet:** the Butcher of the fortress, the Merciful, the Broker, or the stranger at
  the gate. The Roll of the Dead carves it beside your name (glory motive), hunters' orders and
  posters use it.
- **Posters:** bounty boards get a newer poster beside the old one: "the Grey Abbots pay 250
  crowns for the Butcher of Edhold, who killed Tamumun."
- **Hunters' orders** name your deeds: "Bring me their head: the Merciful killed Ishusut."

## 3. Factions and domains

**Standing.** Each faction of the history keeps a standing with you: −2 for each of its people
you kill, +1 for each you spare, +3 for each you deal with, and −3 for each deal you make with
its enemies (the liars and the opener's side oppose each other). When the current target's
people stand at 3 or more they send no hunters ("CEDOWYN SENDS NO HUNTERS: YOU STAND WITH THEIR
PEOPLE"); at −4 or less they send five instead of three, two of them elites ("... THEY HAVE NOT
FORGOTTEN WHAT YOU DID").

**Domains.** Killing a domain lord changes the domain for good. Half of them empty ("THE VAULTS
OF NEFUSHAR - EMPTY SINCE YOU KILLED AMESUT THE QUIET": no lord, the inscriptions speak of an
abandoned place); in the other half the rival faction moves in under a new, stronger lord
("BRANEMUND'S UNDERCROFT - TAKEN BY THE UNBURIED WATCH SINCE YOU KILLED VARIHOLD GRAVEWED").
Kill that one too and the domain empties. Bounty boards follow: a board for a lord who is gone
points at whoever holds the place now.

## 4. The spared come back

Whether a spared target is grateful or bears a grudge comes from its traits (cruel, ambitious,
proud or greedy ones resent the mercy; loyal, merciful or devout ones are grateful).

- **The grateful** live in your Sanctum, in the court in front of where you arrive. Standing
  by them, they tell you another truth of the history ("You let me live. So hear this: ...").
  When hunters come, one of them may warn you first, and fewer come.
- **The resentful** come back once, at the head of the next band of hunters: "ISHUSUT RETURNS
  WITH HUNTERS. YOU SHOULD NOT HAVE SPARED THEM." The deeds list says so afterwards.

## 5. After the mystery: two arcs of your own making

Chapters 10 to 15 (arcs IV and V) are no longer the old procedural pattern. They come of what
you did:

- **Arc IV, the reckoning.** If you killed someone of weight before the finale (a Power before a
  general before a captain), their kin, or a sworn friend, comes for you: "Ysaine, Ulawyn's
  sworn friend, swore at the grave to bury you beside it." If your hands are clean, a spared one with a grudge leads it
  ("Ishusut pays back your mercy, in blood"), else the enemies of those you dealt with.
- **Arc V, what the end made**, from the finale: killing the opener brings their heir after the
  key; sparing them brings a Herald of what came up (cosmic horror), the ones below walking the
  halls (tragedy), or the liars burying the truth again, and everyone who saw it (human drama);
  taking the key brings the faction you hurt most, to take it from you.

Their hooks, revelations and hunters' orders speak of your deeds ("Papers on the body: a list of
names. Yours is first, for Ulawyn."). From chapter 16 on the old pattern continues, carrying
the facts no chapter has told yet.

Example, seed 1337 (Edhold), every third choice a kill, then each ending
(`worldmap choices 1`):

| Ending | Arc IV's Power | Arc V's Power |
|--------|----------------|---------------|
| Kill Thyvik | Ysaine, sworn friend of Ulawyn (killed in chapter 6) | Ingoulf, heir of Thyvik, wants the Bone Key |
| Spare Thyvik | Aumioc, sworn friend of Ulawyn | Skemund, Steward of Edhold, comes for everyone who saw the bones |
| Take the key | Malamar, sworn friend of Ulawyn | Eyydis, claimant of Edhold, for the faction you hurt most |

The names differ between endings because a new name is drawn for each path; within one world
they are fixed once the deeds that decide them are done.

## Saves

Version 14 adds the ledger to the world: each deed's kind, chapter, choice, who did it, flags
(a resentful one has come back) and the named enemy's key, and for lords the domain cell. Lore
gained a kind for what residents tell. Older saves load with an empty ledger.

## Tests and tools

- `test_worldgen` plays every combination of three policies and three endings on the test
  world: chapter targets keep their keys and names whatever is done, the sequel's six targets
  are distinct and found by their keys, every hook, revelation, order, plea, choice and deed
  line exists and fits, and killed lords change their domains (a taken domain has a new lord
  and faction; a killed lord never comes back).
- `worldmap choices [policy]` for reading a world's choices and endings.
- `--demo parley --cast N` (the choice at chapter N), `--demo residents` (the grateful in the
  Sanctum); `tools/screenshots.sh story_choice story_finale story_residents`.

## In play

Six bot runs of 75 minutes in six new worlds (three questers, an explorer, an archer, a
caster; bots choose by policy). No crashes; every consequence turned up:

- **Edhold (1337):** killed Tamumun after it yielded, spared Ishusut, bargained with the
  Steward Tamusut, killed Ostiwyn. At minute 30, "A WARNING FROM ISHUSUT: HUNTERS ARE COMING.
  FEWER THAN THEY MEANT TO SEND." Four domains changed hands or emptied behind it.
- **Aldburg (8080):** bargained with Halewyn at minute 3; ten minutes later "CEDOWYN SENDS NO
  HUNTERS: YOU STAND WITH THEIR PEOPLE". It killed Cedowyn when it finally found him.
- **Gerhold (2025):** spared Morawyn, who told it what lies below on the spot; ten minutes later
  "MORAWYN RETURNS WITH HUNTERS. YOU SHOULD NOT HAVE SPARED THEM."
- **Ulwall (9001):** the same with Raket, at minute 40.
- **Cedmark, Halmark (explorer, archer):** no chapters, but six domains taken or emptied after
  their lords fell, one of them twice (taken, then emptied when the new lord died too).

The bots still reach few chapters (one to four), so the finale and the arcs after it were
checked with `worldmap choices` and the tests rather than in play.

## In co-op

Everyone who stands at the one who yielded gets the choice. Each chooses, and the most decide (a
tie spares). Whoever has chosen waits for the others; after 25 seconds those who chose decide.
The banner says what was chosen together. The same holds at the faltering Old King
([OLD_KING.md](OLD_KING.md)): more than half of those at the throne must strike for the same
ending.

The end of the chronicle (the opener) also completes step 8 of the Old King's way: the opener
served him.

## Not done

- Domain lords do not yield; only the chapter targets of the mystery do.
- The resentful come back once. A grateful one is not yet useful beyond what it says and the
  warnings.
