# Design

## Context
The renderer drew boxes, octahedra and pyramids, each face lit by one of a few fixed shades, with
face culling off. Weapons are drawn every frame in view and wherever they lie in the world.

## Decisions
- **Shapes from three helpers** in weapons.c: a tube through points with a radius at each
  (parallel-transported rings, so bends do not twist), a lathe along the weapon, and a flat
  profile with a half thickness at each point (thin where it cuts). Blades are their own loft
  of ten-point cross-sections.
- **Lighting in weapons.c, not the batch**: culling is off, so each triangle's normal is turned
  away from a point known to be inside; the colour is lit there and pushed with
  `batch3d_tri_raw`. The glint is clamped and stepped so it stays in the game's look.
- **The eye** is set by `weapon_set_eye` before the world's batch (world space) and before the
  weapon in view (view space, the origin).

## Risks
- More triangles: about 4% slower with nine weapons in view in the software renderer.
