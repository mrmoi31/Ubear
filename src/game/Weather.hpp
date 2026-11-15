// Météo caractérielle : zones (vent, brouillard, grêle), cycle jour/nuit
// et événements extrêmes (blizzard = vent + brouillard global, aurore boréale la nuit).
#pragma once
#include "../core/Assets.hpp"

#include <raylib.h>
#include <vector>

enum WeatherZone { WZ_WIND = 0, WZ_FOG, WZ_HAIL, WZ_COUNT };

enum WeatherEvent {
    EV_BLIZZARD_WARN, EV_BLIZZARD_START, EV_BLIZZARD_END,
    EV_NIGHT, EV_DAWN, EV_AURORA_START, EV_AURORA_END
};

struct WeatherSample {
    Vector2 wind{0, 0};           // accélération (unités/s²), avant lest
    float   hail = 0.0f, fog = 0.0f;
    bool    inWind = false, inHail = false, inFog = false;
};

class Weather {
public:
    void generate();
    void unload();
    void update(float dt, Rectangle view, bool eventsEnabled);

    WeatherSample sample(Vector2 p) const;

    void drawGroundZones() const;                              // monde : vent + grêle, sous les bâtiments
    void drawFogZones() const;                                 // monde : nappes de brouillard
    void drawMinimapZones(Rectangle r) const;
    void drawParticles(const Assets& a) const;                 // monde, au-dessus de l'ours
    void drawAurora(float sw, float sh) const;                 // écran
    void drawBlizzard(const Assets& a, float sw, float sh) const; // écran

    float hour() const;
    float elapsed() const;               // secondes réelles depuis le début de la journée
    void  setDayClock(float daySeconds); // cale 17 h → fermeture sur cette durée
    void  shiftHours(float hours);       // avance l'horloge (nuit précoce)
    void  setIntensity(float k);         // vent et blizzards, 1 = jour 1
    float darkness() const;              // 0 jour .. 1 nuit noire
    bool  isNight() const;
    float blizzard() const;              // intensité 0..1
    bool  blizzardActive() const { return bState_ == B_ACTIVE; }
    float blizzardTimeLeft() const { return bState_ == B_ACTIVE ? bTimer_ : 0.0f; }
    float blizzardWarning() const { return bState_ == B_WARN ? bTimer_ : -1.0f; }
    float aurora() const;                // intensité 0..1
    bool  auroraActive() const { return auroraOn_; }
    float auroraTimeLeft() const { return auroraOn_ ? auroraEnd_ - t_ : 0.0f; }
    Vector2 windDir() const;
    Vector2 blizzardDir() const;

    const std::vector<WeatherEvent>& events() const { return events_; }

    // Pour les tests et la démo.
    void forceBlizzard();
    void forceNight(bool withAurora);

private:
    float mask(int z, Vector2 p) const;
    void  spawnParticles(Rectangle view);

    static constexpr int RES = 256;
    std::vector<float> masks_[WZ_COUNT];
    Texture2D          tex_[WZ_COUNT]{};

    float t_ = 0.0f, clockShift_ = 0.0f, windAngle_ = 0.0f;
    float clockRate_ = 48.0f;            // secondes réelles pour une heure de jeu
    float intensity_ = 1.0f;
    enum BState { B_IDLE, B_WARN, B_ACTIVE } bState_ = B_IDLE;
    float bTimer_ = 0.0f;
    bool  wasNight_ = false, auroraPlanned_ = false, auroraOn_ = false;
    float auroraStart_ = 0.0f, auroraEnd_ = 0.0f;
    std::vector<WeatherEvent> events_;

    struct Part { Vector2 pos, vel; float life, maxLife, size; int kind; };
    std::vector<Part> parts_;
};
