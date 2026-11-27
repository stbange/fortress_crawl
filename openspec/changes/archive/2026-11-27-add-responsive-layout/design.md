# Design

## Decisions
- **Scale the frame, not the font**: drawing a screen into W/s x H/s and letting the viewport
  stretch it makes text, boxes and gaps grow together, so every layout that works at a split-screen
  quarter (240 x 135) works at 2x; nothing had to compute characters per line anew.
- **Split screen stays plain**: views of two to four players keep the target's own pixels.
- **Portrait is allowed**: the target grows in height, the HUD anchors to the edges.
