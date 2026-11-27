# Design

## Decisions
- **Check before use**: the CRC covers the whole save, so `game_load` returns why it loaded nothing
  (SAVE_DAMAGED, SAVE_NEWER) without having touched the game. Saves before version 31 have no check
  and load as before.
- **Kept aside, not deleted**: the bytes move to "<key>.bad", which the list of worlds shows with
  EXPORT and DELETE, so a damaged save can still be sent with a bug report.
- **Dialogs answer later**: SDL's file dialogs call back on their own time; transfer.c keeps the
  result for the start screen to take (`transfer_poll`).
