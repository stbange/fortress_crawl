#include "world/eco.h"

void eco_start(Eco *e, float plants, float flowers, bool predators_live_here) {
    e->plants = plants;
    e->flowers = flowers;
    e->grazers = eco_capacity(e) * 0.6f;
    e->predators = predators_live_here ? 0.4f : 0.05f;
    e->insects = flowers * 0.3f;
    e->carrion = 0;
    e->init = true;
}

float eco_step(Eco *e, float hours) {
    float eaten = 0;
    while (hours > 0) {
        float h = hours > 0.05f ? 0.05f : hours;
        hours -= h;
        float k = eco_capacity(e), g = e->grazers, p = e->predators;
        /* grazers: grow toward what the plants feed, eaten by predators; a few always wander in */
        float dg = 0.5f * g * (1.0f - g / k) - 0.1f * g * p + 0.02f;
        /* predators: fed by grazers (and bats by insects), starve without; a few wander in */
        float dp = 0.04f * g * p + 0.004f * e->insects * p - 0.2f * p + 0.003f;
        e->grazers = fmaxf(0.0f, fminf(g + dg * h, 3.0f * k));
        e->predators = fmaxf(0.0f, p + dp * h);
        /* insects follow the flowers and the dead; the dead rot */
        float want = e->flowers * 0.3f + e->carrion * 2.0f;
        e->insects += (want - e->insects) * fminf(1.0f, h);
        e->carrion = fmaxf(0.0f, e->carrion - 0.4f * e->carrion * h);
        /* too many grazers eat the plants down faster than they grow back */
        if (g > 0.9f * k) eaten += (g - 0.9f * k) * 6.0f * h;
    }
    return eaten;
}

void eco_wander(Eco *a, Eco *b, float hours) {
    float k = fminf(1.0f, 0.05f * hours);
    /* each moves a little toward where the food per head is better */
    float ga = a->grazers / eco_capacity(a), gb = b->grazers / eco_capacity(b);
    float mg = (ga - gb) * k * 0.5f;
    if (mg > 0) { float m = fminf(mg * eco_capacity(a), a->grazers); a->grazers -= m; b->grazers += m; }
    else { float m = fminf(-mg * eco_capacity(b), b->grazers); b->grazers -= m; a->grazers += m; }
    float mp = (a->predators - b->predators) * k * 0.3f;
    a->predators -= mp;
    b->predators += mp;
}
