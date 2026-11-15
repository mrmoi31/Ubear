// La banquise de Glagla-sur-Mer : une île de glace parcourue de trous d'eau.
#pragma once
#include <raylib.h>
#include <vector>

class World {
public:
    void generate();
    void unload();
    void draw() const;

    float waterAt(Vector2 p) const;                 // 0 = glace, 1 = eau profonde
    bool  inWater(Vector2 p) const { return waterAt(p) > 0.3f; }
    bool  solidIce(Vector2 p, float margin = 120.0f) const;
    // Point de glace à une distance [minD, maxD] de `from` (repli : point de glace quelconque).
    Vector2 randomIcePoint(Vector2 from, float minD, float maxD) const;
    Vector2 center() const;
    const Texture2D& texture() const { return tex_; }

private:
    int                n_ = 0;
    std::vector<float> water_;
    Texture2D          tex_{};
};
