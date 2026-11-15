#include "Catalog.hpp"

#include <cmath>

const UpgradeDef UPGRADES[UP_COUNT] = {
    {UP_SKATES, Str::UpSkatesName, Str::UpSkatesDesc, {220, 640, 1500},
     "Roulements neufs. Enfin, presque.", "New bearings. Well, almost."},
    {UP_BRAKES, Str::UpBrakesName, Str::UpBrakesDesc, {180, 520, 1300},
     "Un ours qui freine, j'aurai tout vu.", "A bear that brakes. Now I've seen it all."},
    {UP_GPS, Str::UpGpsName, Str::UpGpsDesc, {150, 480, 1200},
     "Il parle avec l'accent du Groenland, c'est normal.", "It speaks with a Greenland accent. That's normal."},
    {UP_BAG, Str::UpBagName, Str::UpBagDesc, {200, 600, 1450},
     "Doublé en poils de yéti. Il était d'accord.", "Lined with yeti fur. He agreed."},
    {UP_ANCHOR, Str::UpAnchorName, Str::UpAnchorDesc, {240, 720, 1700},
     "C'est un caillou. Un bon caillou.", "It's a rock. A good rock."},
};

const CosmeticDef COSMETICS[] = {
    {"skin_polaire", COS_SKIN, Str::SkinPolaire, nullptr, 0, WHITE, {}},
    {"skin_brun", COS_SKIN, Str::SkinBrun, "cosmetics/skins/skin_brun.png", 450, Color{150, 100, 60, 255}, {1.10f}},
    {"skin_marron", COS_SKIN, Str::SkinMarron, "cosmetics/skins/skin_marron.png", 600, Color{110, 62, 36, 255}, {1, 1.06f}},
    {"skin_panda", COS_SKIN, Str::SkinPanda, "cosmetics/skins/skin_panda.png", 750, Color{40, 40, 40, 255}, {1, 1, 1, 1.12f}},
    {"skin_dore", COS_SKIN, Str::SkinDore, "cosmetics/skins/skin_dore.png", 2400, Color{240, 190, 60, 255}, {1.06f, 1, 1, 1.18f}},

    {"hat_none", COS_HAT, Str::CosNone, nullptr, 0, WHITE, {}},
    {"hat_bonnet_rouge", COS_HAT, Str::HatBonnetRouge, "cosmetics/hats/hat_bonnet_rouge.png", 240, RED, {1, 1, 1.08f}},
    {"hat_bonnet_vert", COS_HAT, Str::HatBonnetVert, "cosmetics/hats/hat_bonnet_vert.png", 240, GREEN, {1.08f}},
    {"hat_casquette", COS_HAT, Str::HatCasquette, "cosmetics/hats/hat_casquette.png", 450, BLUE, {1, 1, 1, 1.10f}},
    {"hat_livreur", COS_HAT, Str::HatLivreur, "cosmetics/hats/hat_livreur.png", 650, ORANGE, {1, 1, 1.10f, 1.06f}},
    {"hat_haut_de_forme", COS_HAT, Str::HatHautDeForme, "cosmetics/hats/hat_haut_de_forme.png", 1100, BLACK, {1, 1, 1, 1.16f}},
    {"hat_couronne", COS_HAT, Str::HatCouronne, "cosmetics/hats/hat_couronne.png", 2800, GOLD, {1, 1.08f, 1, 1.22f}},

    {"bag_original", COS_BAG, Str::CosOriginal, nullptr, 0, BLUE, {}},
    {"bag_bleu", COS_BAG, Str::BagBleu, "cosmetics/bags/bag_bleu.png", 320, BLUE, {1, 1, 1.12f}},
    {"bag_rouge", COS_BAG, Str::BagRouge, "cosmetics/bags/bag_rouge.png", 380, RED, {1.08f}},
    {"bag_vert", COS_BAG, Str::BagVert, "cosmetics/bags/bag_vert.png", 520, GREEN, {1, 1, 1, 1.10f}},
    {"bag_noir", COS_BAG, Str::BagNoir, "cosmetics/bags/bag_noir.png", 720, BLACK, {1, 1, 1, 1, 0.82f}},
    {"bag_or", COS_BAG, Str::BagOr, "cosmetics/bags/bag_or.png", 1600, GOLD, {1, 1, 1.14f, 1.12f}},

    {"skate_original", COS_SKATE, Str::CosOriginal, nullptr, 0, RED, {}},
    {"skate_rouge", COS_SKATE, Str::SkateRouge, "cosmetics/skates/skate_rouge.png", 320, RED, {1.14f}},
    {"skate_bleu", COS_SKATE, Str::SkateBleu, "cosmetics/skates/skate_bleu.png", 380, BLUE, {1, 0.78f}},
    {"skate_vert", COS_SKATE, Str::SkateVert, "cosmetics/skates/skate_vert.png", 520, GREEN, {1, 1, 1, 1, 1, 0.62f}},
    {"skate_noir", COS_SKATE, Str::SkateNoir, "cosmetics/skates/skate_noir.png", 720, BLACK, {1, 1, 1, 1, 0.75f}},
    {"skate_or", COS_SKATE, Str::SkateOr, "cosmetics/skates/skate_or.png", 1600, GOLD, {1.16f, 1.10f}},

    {"trail_none", COS_TRAIL, Str::CosNone, nullptr, 0, WHITE, {}},
    {"trail_snow", COS_TRAIL, Str::TrailSnow, nullptr, 380, Color{235, 245, 255, 255}, {1, 1, 1.08f}},
    {"trail_sparks", COS_TRAIL, Str::TrailSparks, nullptr, 700, Color{255, 190, 60, 255}, {1.10f}},
    {"trail_bubbles", COS_TRAIL, Str::TrailBubbles, nullptr, 850, Color{120, 200, 255, 255}, {1, 1, 1, 1, 1, 0.55f}},
    {"trail_rainbow", COS_TRAIL, Str::TrailRainbow, nullptr, 1400, Color{230, 90, 200, 255}, {1, 1, 1, 1.14f}},
};
const int COSMETIC_COUNT = sizeof(COSMETICS) / sizeof(COSMETICS[0]);

NextBuy nextPurchase(const SaveData& s)
{
    NextBuy best;
    int bestPrice = 1000000000;
    for (int i = 0; i < UP_COUNT; i++) {
        int lvl = s.upgrades[i];
        if (lvl >= 3) continue;
        int price = UPGRADES[i].price[lvl];
        if (price < bestPrice) { bestPrice = price; best.found = true; best.name = tr(UPGRADES[i].name); best.price = price; }
    }
    for (int i = 0; i < COSMETIC_COUNT; i++) {
        const CosmeticDef& c = COSMETICS[i];
        if (c.price <= 0 || s.owned.count(c.id)) continue;
        if (c.price < bestPrice) { bestPrice = c.price; best.found = true; best.name = tr(c.name); best.price = c.price; }
    }
    if (best.found) best.missing = s.coins >= best.price ? 0 : best.price - s.coins;
    return best;
}

const CosmeticDef* findCosmetic(const std::string& id)
{
    for (int i = 0; i < COSMETIC_COUNT; i++)
        if (id == COSMETICS[i].id) return &COSMETICS[i];
    return nullptr;
}

CosBonus equippedBonus(const SaveData& s)
{
    CosBonus b;
    for (int c = 0; c < COS_COUNT; c++) {
        const CosmeticDef* d = findCosmetic(s.equipped[c]);
        if (!d) continue;
        const CosBonus& x = d->bonus;
        b.push *= x.push; b.turn *= x.turn; b.heat *= x.heat;
        b.tip *= x.tip; b.wind *= x.wind; b.water *= x.water;
    }
    return b;
}

std::string bonusLine(const CosBonus& b)
{
    struct Bit { float v; const char* fr; const char* en; };
    Bit bits[] = {
        {b.push, "Vitesse", "Speed"},
        {b.turn, "Virage", "Turn"},
        {b.heat, "Chaleur", "Heat"},
        {b.tip, "Pourboire", "Tips"},
        {b.wind, "Vent", "Wind"},
        {b.water, "Eau", "Water"},
    };
    std::string out;
    for (auto& bit : bits) {
        int n = (int)std::lround((bit.v - 1.0f) * 100.0f);
        if (n == 0) continue;
        if (!out.empty()) out += "  ";
        out += L(bit.fr, bit.en);
        out += n > 0 ? " +" : " ";
        out += std::to_string(n);
        out += "%";
    }
    return out;
}

const char* gastonQuote(Upgrade u)
{
    return L(UPGRADES[u].gastonFr, UPGRADES[u].gastonEn);
}

const char* gastonIdle(int i)
{
    static const char* fr[] = {
        "Tout est reconditionné. Comprendre : trouvé sur la glace.",
        "Pas de remboursement. Pas de garantie. Pas de sourire.",
        "Tu veux freiner ? Achète des freins. C'est pas sorcier.",
        "Ça sert à rien, mais quelle allure.",
        "Ça glisse mieux, mais c'est pas donné.",
    };
    static const char* en[] = {
        "Everything is refurbished. Meaning: found on the ice.",
        "No refunds. No warranty. No smiles.",
        "Want to stop? Buy brakes. It's not rocket science.",
        "Totally useless, but what style.",
        "Slides better, but it ain't cheap.",
    };
    int n = (int)(sizeof(fr) / sizeof(fr[0]));
    i = ((i % n) + n) % n;
    return L(fr[i], en[i]);
}
