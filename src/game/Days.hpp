// Humeur du jour, énergie (temps réel) et cadeau quotidien.
#pragma once
#include "../core/Save.hpp"

struct MoodFx {
    const char* nameFr;
    const char* nameEn;
    const char* blurbFr;
    const char* blurbEn;
    float heat = 1.0f;       // refroidissement (plus haut = plus dur)
    float pay = 1.0f;
    float spread = 1.0f;     // distances
    float wind = 1.0f;       // intensité météo
    float streak = 1.0f;     // bonus de série
    float nightHours = 0.0f; // avance l'horloge
};

const MoodFx& moodFor(int day);

namespace vitality {
void refresh(SaveData& d);
void spend(SaveData& d);
int  secondsToNext(const SaveData& d);
}

namespace rewards {
bool ready(const SaveData& d);
int  peek(const SaveData& d);     // pièces du prochain cadeau
int  claim(SaveData& d);          // crédite et renvoie le montant (0 si déjà pris)
int  streakAfterClaim(const SaveData& d);
}

struct StoreOffer {
    const char* sku;
    int         coins;            // 0 si ce n'est pas un pack de pièces
    bool        removeAds;
    const char* price;
    const char* nameFr;
    const char* nameEn;
};

namespace offers {
void sync(SaveData& d);             // remet les compteurs de pubs à minuit
int  energyAdsLeft(const SaveData& d);
bool grantEnergyAd(SaveData& d);    // +5 journées, max 3 / jour
int  goldAdsLeft(const SaveData& d);
int  grantGoldAd(SaveData& d);      // pièces, ou 0 si plafond atteint
int  storeCount();
const StoreOffer& store(int i);
bool buy(SaveData& d, int i);       // crédite le pack (paiement store à brancher)
}
