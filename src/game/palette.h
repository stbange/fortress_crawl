#ifndef FC_PALETTE_H
#define FC_PALETTE_H

/* The colours of the elements and the rarities, normal and for the three colour-vision
 * deficiencies (openspec accessibility: not by colour alone). Each alternative keeps the
 * colours' meaning (fire orange, frost blue...) and moves them just enough that every pair
 * stays at least 20 apart in CIELAB as seen with that deficiency. tools/check_palettes.py
 * checks it (Machado 2009 simulation); change a colour here, then run it. */

/* [palette][element]: fire, frost, lightning, poison, arcane, holy, shadow, earth */
static const vec3 ELEMENT_PAL[PAL_COUNT][EL_COUNT] = {
    { { 1.00f, 0.50f, 0.15f }, { 0.55f, 0.85f, 1.00f }, { 0.75f, 0.80f, 1.00f }, { 0.45f, 0.95f, 0.30f }, { 0.85f, 0.40f, 1.00f }, { 1.00f, 0.95f, 0.60f }, { 0.55f, 0.25f, 0.75f }, { 0.75f, 0.60f, 0.40f } },
    { { 1.00f, 0.50f, 0.15f }, { 0.55f, 0.85f, 1.00f }, { 0.51f, 0.58f, 0.85f }, { 0.45f, 0.95f, 0.30f }, { 0.85f, 0.40f, 1.00f }, { 1.00f, 0.95f, 0.60f }, { 0.49f, 0.17f, 0.67f }, { 0.75f, 0.60f, 0.40f } },
    { { 0.91f, 0.43f, 0.05f }, { 0.55f, 0.85f, 1.00f }, { 0.99f, 0.92f, 1.00f }, { 0.56f, 1.00f, 0.35f }, { 0.85f, 0.40f, 1.00f }, { 1.00f, 0.98f, 0.67f }, { 0.50f, 0.20f, 0.69f }, { 0.75f, 0.61f, 0.40f } },
    { { 1.00f, 0.50f, 0.15f }, { 0.58f, 0.90f, 1.00f }, { 0.75f, 0.80f, 1.00f }, { 0.18f, 0.71f, 0.05f }, { 0.86f, 0.38f, 0.99f }, { 1.00f, 0.95f, 0.60f }, { 0.55f, 0.25f, 0.75f }, { 0.75f, 0.60f, 0.40f } },
};

/* [palette][rarity]: common, magic, rare, epic, legendary */
static const vec3 RARITY_PAL[PAL_COUNT][RAR_COUNT] = {
    { { 0.80f, 0.78f, 0.72f }, { 0.45f, 0.65f, 1.00f }, { 1.00f, 0.85f, 0.30f }, { 1.00f, 0.50f, 0.15f }, { 0.35f, 1.00f, 0.65f } },
    { { 0.80f, 0.78f, 0.72f }, { 0.45f, 0.65f, 1.00f }, { 1.00f, 0.85f, 0.30f }, { 1.00f, 0.50f, 0.15f }, { 0.35f, 1.00f, 0.65f } },
    { { 0.75f, 0.75f, 0.74f }, { 0.45f, 0.65f, 1.00f }, { 1.00f, 0.87f, 0.31f }, { 0.91f, 0.43f, 0.06f }, { 0.35f, 1.00f, 0.65f } },
    { { 0.78f, 0.78f, 0.72f }, { 0.45f, 0.65f, 1.00f }, { 0.95f, 0.80f, 0.25f }, { 1.00f, 0.50f, 0.15f }, { 0.35f, 1.00f, 0.65f } },
};

#endif
