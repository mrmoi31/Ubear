// Défis quotidiens : 3 défis (facile / moyen / difficile) tirés à partir de la date du jour.
#pragma once
#include "../core/Save.hpp"

#include <string>
#include <vector>

enum ChalType {
    CH_DELIVERIES, CH_HAIL, CH_DRY, CH_FOG, CH_WIND, CH_STREAK, CH_COINS,
    CH_HOT, CH_NIGHT, CH_BLIZZARD, CH_AURORA, CH_REGULARS, CH_TYPE_COUNT
};

struct ChallengeDef {
    ChalType type;
    int      target;
    int      reward;
};

// Ce qui s'est passé pendant une livraison réussie.
struct DeliveryReport {
    bool  hail = false, fog = false, wind = false, water = false;
    bool  night = false, blizzard = false, aurora = false, regular = false;
    float heatLeft = 0.0f;
    int   coins = 0;
    int   streak = 0;     // série après cette livraison
};

namespace challenges {
int          today();                      // AAAAMMJJ (heure locale)
int          secondsUntilReset();
bool         refresh(SaveData& d);         // nouveau jour : nouveaux défis (true si changé)
ChallengeDef get(const SaveData& d, int slot);
std::string  describe(const ChallengeDef& c);
const char*  tierName(int slot);
int          doneCount(const SaveData& d);

struct Completion { int slot; int reward; bool allBonus; };
// Met à jour la progression ; crédite les récompenses ; renvoie les défis tout juste accomplis.
std::vector<Completion> onDelivery(SaveData& d, const DeliveryReport& r);
}
