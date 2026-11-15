// Clients récurrents : identité, traits de gameplay, dialogues selon la relation.
#pragma once
#include <raylib.h>

enum CustomerId { CU_PENELOPE = 0, CU_GERARD, CU_ROXANE, CU_YETI, CU_JUMEAUX };

// Portraits dessinés en formes simples (aucune image requise).
enum Portrait { PORTRAIT_GASTON = 100, PORTRAIT_UBEAR = 101, PORTRAIT_BEAR = 102 };

struct CustomerDef {
    const char* nameFr; const char* nameEn;
    const char* descFr; const char* descEn;
    Color color;
    float tipMult;       // multiplicateur de pourboire
    float timeMult;      // patience (temps avant que le plat refroidisse)
    float speedBonusMult;
    float weight;        // fréquence des commandes
};

namespace customers {
const CustomerDef& def(int id);
const char* name(int id);
const char* desc(int id);

constexpr int TIER_COUNT = 5;
int         tier(int deliveries);            // 0..4
int         tierThreshold(int tier);         // livraisons nécessaires
const char* tierName(int tier);

const char* orderLine(int id, int tier);
const char* thanksLine(int id, int tier);
const char* failLine(int id);
const char* tierUpLine(int id, int tier);
const char* yetiBigTipLine();

float tipMult(int id, int deliveriesBefore); // gère le yéti radin

void drawPortrait(int who, Vector2 c, float r, float time = 0.0f);
}
