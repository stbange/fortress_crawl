# Change: add-error-messages

## Why
When something went wrong the player saw nothing useful: a browser without WebGL 2 showed a frozen page, an abort left a blank canvas, a desktop without OpenGL 3.3 closed with a line in a console, a full localStorage lost saves silently, and after a crash there was neither a word nor a report.

## What Changes
- The browser page checks for WebGL 2 before loading the game and says which browsers run it; an abort shows THE GAME STOPPED with a reload button.
- The desktop shows a message box when the window or the OpenGL 3.3 context cannot be made.
- A save that cannot be written shows a banner (the storage is full or locked: export your world), at most every two minutes; the game keeps trying.
- A session marker (fortress_crawl:session) is kept while a world is played and removed on a clean quit; one left behind makes the start screen say the last game ended unexpectedly and that CONTINUE goes on from its last save.
- On the desktop a crash (SIGSEGV, SIGABRT, SIGFPE, SIGILL; Windows' unhandled exceptions) writes crash.txt beside the saves: the build, the world, the time played and the last 50 lines of the log, written with write() only. --crash-test crashes five seconds into play.

## Impact
- Affected specs: errors
- Affected code and docs: src/game/crash.c (new), save.c, menu.c, src/main.c, web/shell.html, CMakeLists.txt; README.md.
