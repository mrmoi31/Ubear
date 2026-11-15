// Catalogue de la Boutique de Gaston : équipement (3 niveaux) et cosmétiques.
#pragma once
#include "../core/Locale.hpp"
#include "../core/Save.hpp"

#include <raylib.h>
#include <string>

struct UpgradeDef {
    Upgrade     id;
    Str         name, desc;
    int         price[3];
    const char* gastonFr;
    const char* gastonEn;
};

// 1 = neutre. push/turn/heat/tip : plus haut = mieux. wind/water : plus bas = moins de malus.
struct CosBonus {
    float push = 1.0f, turn = 1.0f, heat = 1.0f, tip = 1.0f, wind = 1.0f, water = 1.0f;
};

struct CosmeticDef {
    const char* id;
    CosCat      cat;
    Str         name;
    const char* file;      // chemin relatif à assets/, nullptr si rien à dessiner
    int         price;     // 0 = gratuit
    Color       swatch;    // couleur d'aperçu (traînées)
    CosBonus    bonus;
};

extern const UpgradeDef  UPGRADES[UP_COUNT];
extern const CosmeticDef COSMETICS[];
extern const int         COSMETIC_COUNT;

struct NextBuy {
    bool found = false;
    const char* name = "";
    int price = 0;
    int missing = 0;          // pièces qui manquent encore (0 = déjà assez)
};

const CosmeticDef* findCosmetic(const std::string& id);
NextBuy            nextPurchase(const SaveData& s);
CosBonus           equippedBonus(const SaveData& s);
std::string        bonusLine(const CosBonus& b);   // vide si aucun effet
const char*        gastonQuote(Upgrade u);
const char*        gastonIdle(int i);      // phrases d'accueil, i quelconque
