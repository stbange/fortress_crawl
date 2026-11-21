# OpenSpec instructions

This project keeps its requirements in OpenSpec's layout:

- `openspec/specs/<capability>/spec.md`: what the game does now, as requirements (SHALL) with
  scenarios (WHEN / THEN).
- `openspec/changes/<change-id>/`: a change being made: `proposal.md` (why, what, impact),
  `tasks.md` (a checklist), `design.md` when there are decisions to record, and spec deltas in
  `specs/<capability>/spec.md` under `## ADDED`, `## MODIFIED` or `## REMOVED Requirements`.
- `openspec/changes/archive/YYYY-MM-DD-<change-id>/`: changes done; their deltas are applied to
  `openspec/specs/`.

Workflow:
1. Before a change of behaviour, write its proposal, tasks and spec deltas under `changes/`.
2. Implement it, ticking the tasks.
3. When it is done and verified, move it to `changes/archive/` with the date and apply its deltas
   to the specs (a MODIFIED requirement replaces the whole requirement; REMOVED deletes it).

Every requirement has at least one scenario. Project context is in `project.md`.
