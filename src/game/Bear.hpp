// Barnabé : deux commandes (gauche / droite), quatre rollers, zéro frein (au début).
#pragma once
#include "../core/Assets.hpp"
#include "../core/Save.hpp"

#include <raylib.h>

enum Paw { PAW_BL = 0, PAW_BR, PAW_TL, PAW_TR, PAW_COUNT };

struct BearEnv {
    Vector2 wind{0, 0};      // accélération déjà atténuée par le lest
    float   extraDamping = 0.0f;
    bool    water = false;
};

class Bear {
public:
    Vector2 pos{0, 0}, vel{0, 0};
    float   heading = 0.0f;          // cap physique (degrés, 0 = haut, sens horaire)
    float   angle = 0.0f;            // cap affiché (lissé)
    float   pawPulse[PAW_COUNT]{};

    void push(Paw p, float impulse, float turnScale = 1.0f);
    void update(float dt, const BearEnv& env);
    Vector2 forward() const;
    Vector2 pawWorld(Paw p) const;   // position monde d'une patte (particules)
    float   speed() const;
};

// Dessine l'ours avec les cosmétiques équipés. `scale` = unités monde par pixel de Body.png.
void drawBear(Assets& a, const SaveData& s, Vector2 pos, float angleDeg, float scale,
              const float* pawPulse = nullptr, bool shadow = true, bool defaultBag = true,
              const Texture2D* lookOverride = nullptr);
