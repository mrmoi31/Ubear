#include "World.hpp"
#include "../core/Config.hpp"

#include <raymath.h>
#include <algorithm>
#include <cmath>

static float smooth01(float a, float b, float x)
{
    float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void World::generate()
{
    const int N = cfg::MAP_IMAGE_SIZE;
    n_ = N;
    Image cell = GenImageCellular(N, N, cfg::CELL_SIZE);
    Image detail = GenImagePerlinNoise(N, N, GetRandomValue(0, 5000), GetRandomValue(0, 5000), 18.0f);
    const Color* cp = (const Color*)cell.data;
    const Color* dp = (const Color*)detail.data;

    water_.assign((size_t)N * N, 0.0f);
    Image img = GenImageColor(N, N, WHITE);
    Color* out = (Color*)img.data;
    const float half = N * 0.5f;

    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            int i = y * N + x;
            float nb = cp[i].r / 255.0f;
            for (int k = 0; k < cfg::RIVER_POWER - 1; k++) nb *= nb;

            float d = std::hypot(x - half, y - half) / N / 2.0f;
            float isl;
            if (d < cfg::ISLAND_INNER) isl = 0.0f;
            else if (d > cfg::ISLAND_OUTER) isl = 1.0f;
            else isl = (d - cfg::ISLAND_INNER) / (cfg::ISLAND_OUTER - cfg::ISLAND_INNER);

            float w = nb + (1.0f - nb) * isl;
            water_[i] = w;

            float n = dp[i].r / 255.0f;
            float t = smooth01(0.285f, 0.305f, w);
            // glace : légères variations + liseré bleuté près du bord
            Color ice = {(unsigned char)(232 + 18 * n), (unsigned char)(241 + 12 * n), 255, 255};
            ice = ColorLerp(ice, Color{196, 224, 248, 255}, smooth01(0.2f, 0.29f, w));
            // eau : bord sombre (paroi de glace immergée) puis dégradé vers le large
            float deep = std::clamp((w - 0.3f) / 0.7f, 0.0f, 1.0f);
            Color water = ColorLerp(Color{70, 160, 225, 255}, Color{22, 72, 156, 255}, deep);
            water = ColorLerp(Color{40, 110, 190, 255}, water, smooth01(0.3f, 0.36f, w));
            out[i] = ColorLerp(ice, water, t);
        }
    }

    UnloadImage(cell);
    UnloadImage(detail);
    tex_ = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex_, TEXTURE_FILTER_BILINEAR);
}

void World::unload()
{
    if (tex_.id) UnloadTexture(tex_);
    tex_ = Texture2D{};
}

void World::draw() const
{
    DrawTexturePro(tex_, {0, 0, (float)tex_.width, (float)tex_.height},
                   {0, 0, cfg::WORLD_SIZE, cfg::WORLD_SIZE}, {0, 0}, 0.0f, WHITE);
}

float World::waterAt(Vector2 p) const
{
    if (n_ == 0) return 1.0f;
    float fx = p.x / cfg::MAP_SCALE - 0.5f, fy = p.y / cfg::MAP_SCALE - 0.5f;
    if (fx < 0 || fy < 0 || fx >= n_ - 1 || fy >= n_ - 1) return 1.0f;
    int x0 = (int)fx, y0 = (int)fy;
    float tx = fx - x0, ty = fy - y0;
    const float* r0 = &water_[(size_t)y0 * n_ + x0];
    const float* r1 = r0 + n_;
    float a = r0[0] + (r0[1] - r0[0]) * tx;
    float b = r1[0] + (r1[1] - r1[0]) * tx;
    return a + (b - a) * ty;
}

bool World::solidIce(Vector2 p, float margin) const
{
    if (waterAt(p) > 0.08f) return false;
    for (int k = 0; k < 8; k++) {
        float a = k * PI / 4.0f;
        if (waterAt({p.x + std::cos(a) * margin, p.y + std::sin(a) * margin}) > 0.2f) return false;
    }
    return true;
}

Vector2 World::center() const { return {cfg::WORLD_SIZE * 0.5f, cfg::WORLD_SIZE * 0.5f}; }

Vector2 World::randomIcePoint(Vector2 from, float minD, float maxD) const
{
    for (int tries = 0; tries < 400; tries++) {
        float a = GetRandomValue(0, 3600) / 3600.0f * 2.0f * PI;
        float d = minD + (maxD - minD) * (GetRandomValue(0, 1000) / 1000.0f);
        Vector2 p = {from.x + std::cos(a) * d, from.y + std::sin(a) * d};
        if (solidIce(p)) return p;
    }
    for (int tries = 0; tries < 2000; tries++) {
        Vector2 p = {(float)GetRandomValue(800, (int)cfg::WORLD_SIZE - 800), (float)GetRandomValue(800, (int)cfg::WORLD_SIZE - 800)};
        if (solidIce(p)) return p;
    }
    return center();
}
