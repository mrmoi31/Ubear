#include "Weather.hpp"
#include "../core/Config.hpp"

#include <raymath.h>
#include <algorithm>
#include <cmath>

static float rnd(float a, float b) { return a + (b - a) * (GetRandomValue(0, 10000) / 10000.0f); }

static float smooth01(float a, float b, float x)
{
    float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

enum { PK_WIND = 0, PK_HAIL };

void Weather::generate()
{
    static const int   thresholds[WZ_COUNT] = {160, 168, 164};
    static const Color colors[WZ_COUNT] = {{150, 200, 255, 70}, {200, 206, 222, 160}, {120, 120, 175, 80}};
    const float center = RES * 0.5f;

    for (int z = 0; z < WZ_COUNT; z++) {
        Image img = GenImagePerlinNoise(RES, RES, GetRandomValue(0, 10000), GetRandomValue(0, 10000), 5.0f);
        Color* px = (Color*)img.data;
        masks_[z].assign(RES * RES, 0.0f);
        for (int y = 0; y < RES; y++) {
            for (int x = 0; x < RES; x++) {
                int i = y * RES + x;
                float m = smooth01(thresholds[z] - 8.0f, thresholds[z] + 8.0f, (float)px[i].r);
                // zone de départ calme autour du centre de l'île
                float d = std::hypot(x - center, y - center) * (cfg::WORLD_SIZE / RES);
                m *= smooth01(650.0f, 1150.0f, d);
                masks_[z][i] = m;
                Color c = colors[z];
                c.a = (unsigned char)(colors[z].a * m);
                px[i] = c;
            }
        }
        tex_[z] = LoadTextureFromImage(img);
        SetTextureFilter(tex_[z], TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
    }

    t_ = 0.0f;
    clockShift_ = 0.0f;
    windAngle_ = rnd(0.0f, 2.0f * PI);
    bState_ = B_IDLE;
    bTimer_ = rnd(cfg::BLIZZARD_FIRST_MIN, cfg::BLIZZARD_FIRST_MAX);
    wasNight_ = isNight();
    auroraPlanned_ = auroraOn_ = false;
    parts_.clear();
}

void Weather::unload()
{
    for (auto& t : tex_) if (t.id) UnloadTexture(t);
    for (auto& t : tex_) t = Texture2D{};
}

float Weather::mask(int z, Vector2 p) const
{
    float fx = p.x / cfg::WORLD_SIZE * RES - 0.5f, fy = p.y / cfg::WORLD_SIZE * RES - 0.5f;
    if (fx < 0 || fy < 0 || fx >= RES - 1 || fy >= RES - 1) return 0.0f;
    int x0 = (int)fx, y0 = (int)fy;
    float tx = fx - x0, ty = fy - y0;
    const float* r0 = &masks_[z][(size_t)y0 * RES + x0];
    const float* r1 = r0 + RES;
    float a = r0[0] + (r0[1] - r0[0]) * tx;
    float b = r1[0] + (r1[1] - r1[0]) * tx;
    return a + (b - a) * ty;
}

float Weather::elapsed() const { return t_ + clockShift_; }

void Weather::shiftHours(float hours) { clockShift_ += hours * clockRate_; }

void Weather::setDayClock(float daySeconds)
{
    float hours = cfg::DAY_END_HOUR - cfg::DAY_START_HOUR;
    if (hours < 0.5f) hours = 0.5f;
    clockRate_ = daySeconds / hours;
    if (clockRate_ < 1.0f) clockRate_ = 1.0f;
}

void Weather::setIntensity(float k)
{
    if (k < 1.0f) k = 1.0f;
    intensity_ = k;
    if (bState_ == B_IDLE) bTimer_ /= intensity_;
}

float Weather::hour() const
{
    float h = cfg::DAY_START_HOUR + elapsed() / clockRate_;
    return std::fmod(h, 24.0f);
}

bool Weather::isNight() const
{
    float h = hour();
    return h >= cfg::NIGHT_START || h < cfg::NIGHT_END;
}

float Weather::darkness() const
{
    float h = hour();
    const float tw = cfg::TWILIGHT_HOURS;
    if (h >= cfg::NIGHT_START || h < cfg::NIGHT_END) return 1.0f;
    if (h >= cfg::NIGHT_START - tw) return (h - (cfg::NIGHT_START - tw)) / tw;
    if (h < cfg::NIGHT_END + tw) return 1.0f - (h - cfg::NIGHT_END) / tw;
    return 0.0f;
}

float Weather::blizzard() const
{
    if (bState_ == B_WARN) return 0.15f * (1.0f - bTimer_ / cfg::BLIZZARD_WARNING);
    if (bState_ != B_ACTIVE) return 0.0f;
    float in = (cfg::BLIZZARD_DURATION - bTimer_) / 2.5f;
    float out = bTimer_ / 2.5f;
    return std::clamp(std::min(in, out), 0.15f, 1.0f);
}

float Weather::aurora() const
{
    if (!auroraOn_) return 0.0f;
    return std::clamp(std::min((t_ - auroraStart_) / 3.0f, (auroraEnd_ - t_) / 3.0f), 0.0f, 1.0f);
}

Vector2 Weather::windDir() const { return {std::cos(windAngle_), std::sin(windAngle_)}; }

Vector2 Weather::blizzardDir() const
{
    float a = windAngle_ + 0.6f * std::sin(t_ * 0.3f);
    return {std::cos(a), std::sin(a)};
}

void Weather::forceBlizzard()
{
    bState_ = B_ACTIVE;
    bTimer_ = cfg::BLIZZARD_DURATION;
    events_.push_back(EV_BLIZZARD_START);
}

void Weather::forceNight(bool withAurora)
{
    // décale l'horloge jusqu'au début de la nuit
    float h = cfg::DAY_START_HOUR + t_ / clockRate_;
    float target = cfg::NIGHT_START + 0.6f;
    while (target < h) target += 24.0f;
    clockShift_ = (target - h) * clockRate_;
    wasNight_ = true;
    events_.push_back(EV_NIGHT);
    if (withAurora) {
        auroraPlanned_ = false;
        auroraOn_ = true;
        auroraStart_ = t_;
        auroraEnd_ = t_ + cfg::AURORA_DURATION;
        events_.push_back(EV_AURORA_START);
    }
}

void Weather::update(float dt, Rectangle view, bool eventsEnabled)
{
    events_.clear();
    t_ += dt;
    windAngle_ += std::sin(t_ * 0.05f) * dt * 0.08f;

    if (eventsEnabled) {
        bTimer_ -= dt;
        if (bState_ == B_IDLE && bTimer_ <= cfg::BLIZZARD_WARNING) {
            bState_ = B_WARN;
            events_.push_back(EV_BLIZZARD_WARN);
        }
        if (bState_ == B_WARN && bTimer_ <= 0.0f) {
            bState_ = B_ACTIVE;
            bTimer_ = cfg::BLIZZARD_DURATION;
            events_.push_back(EV_BLIZZARD_START);
        } else if (bState_ == B_ACTIVE && bTimer_ <= 0.0f) {
            bState_ = B_IDLE;
            bTimer_ = rnd(cfg::BLIZZARD_GAP_MIN, cfg::BLIZZARD_GAP_MAX) / intensity_;
            events_.push_back(EV_BLIZZARD_END);
        }
    } else {
        // tutoriel : l'horloge des événements est gelée
        t_ -= dt;
    }

    bool night = isNight();
    if (night && !wasNight_) {
        events_.push_back(EV_NIGHT);
        if (rnd(0.0f, 1.0f) < cfg::AURORA_CHANCE) {
            auroraPlanned_ = true;
            auroraStart_ = t_ + rnd(6.0f, 30.0f);
            auroraEnd_ = auroraStart_ + cfg::AURORA_DURATION;
        }
    } else if (!night && wasNight_) {
        events_.push_back(EV_DAWN);
    }
    wasNight_ = night;

    if (auroraPlanned_ && t_ >= auroraStart_) {
        auroraPlanned_ = false;
        auroraOn_ = true;
        events_.push_back(EV_AURORA_START);
    }
    if (auroraOn_ && (t_ >= auroraEnd_ || !night)) {
        auroraOn_ = false;
        events_.push_back(EV_AURORA_END);
    }

    for (size_t i = 0; i < parts_.size();) {
        Part& p = parts_[i];
        p.life -= dt;
        p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
        if (p.life <= 0.0f || !CheckCollisionPointRec(p.pos, {view.x - 400, view.y - 400, view.width + 800, view.height + 800})) {
            parts_[i] = parts_.back();
            parts_.pop_back();
        } else {
            i++;
        }
    }
    spawnParticles(view);
}

void Weather::spawnParticles(Rectangle view)
{
    int wind = 0, hail = 0;
    for (auto& p : parts_) (p.kind == PK_WIND ? wind : hail)++;
    float bl = blizzard();
    int windTarget = 14 + (int)(18 * bl);
    for (int k = 0; k < 6 && wind < windTarget; k++) {
        Vector2 p = {rnd(view.x, view.x + view.width), rnd(view.y, view.y + view.height)};
        float m = std::max(mask(WZ_WIND, p), bl);
        if (m < 0.5f) continue;
        Vector2 dir = bl > 0.5f ? blizzardDir() : windDir();
        float life = rnd(1.0f, 1.8f);
        parts_.push_back({p, Vector2Scale(dir, rnd(600.0f, 950.0f) * (1.0f + bl)), life, life, rnd(0.8f, 1.3f), PK_WIND});
        wind++;
    }
    for (int k = 0; k < 10 && hail < 90; k++) {
        Vector2 p = {rnd(view.x, view.x + view.width), rnd(view.y, view.y + view.height)};
        if (mask(WZ_HAIL, p) < 0.5f) continue;
        float life = rnd(0.35f, 0.6f);
        parts_.push_back({p, {0, 0}, life, life, rnd(5.0f, 9.0f), PK_HAIL});
        hail++;
    }
}

WeatherSample Weather::sample(Vector2 p) const
{
    WeatherSample s;
    float mw = mask(WZ_WIND, p);
    float bl = blizzard();
    s.wind = Vector2Scale(windDir(), cfg::WIND_ACCEL * mw * intensity_);
    if (bl > 0.0f) {
        float gust = 0.75f + 0.25f * std::sin(t_ * 1.7f);
        s.wind = Vector2Add(s.wind, Vector2Scale(blizzardDir(), cfg::WIND_ACCEL * cfg::BLIZZARD_WIND_MULT * bl * gust));
    }
    s.hail = mask(WZ_HAIL, p);
    s.fog = std::max(mask(WZ_FOG, p), bl);
    s.inWind = mw > 0.5f || bl > 0.5f;
    s.inHail = s.hail > 0.5f;
    s.inFog = s.fog > 0.5f;
    return s;
}

void Weather::drawGroundZones() const
{
    for (int z : {WZ_WIND, WZ_HAIL}) {
        DrawTexturePro(tex_[z], {0, 0, (float)RES, (float)RES}, {0, 0, cfg::WORLD_SIZE, cfg::WORLD_SIZE}, {0, 0}, 0.0f, WHITE);
    }
}

void Weather::drawFogZones() const
{
    DrawTexturePro(tex_[WZ_FOG], {0, 0, (float)RES, (float)RES}, {0, 0, cfg::WORLD_SIZE, cfg::WORLD_SIZE}, {0, 0}, 0.0f, WHITE);
}

void Weather::drawMinimapZones(Rectangle r) const
{
    for (int z : {WZ_WIND, WZ_HAIL, WZ_FOG}) {
        DrawTexturePro(tex_[z], {0, 0, (float)RES, (float)RES}, r, {0, 0}, 0.0f, WHITE);
    }
}

void Weather::drawParticles(const Assets& a) const
{
    for (auto& p : parts_) {
        float t = 1.0f - p.life / p.maxLife;   // 0 -> 1
        if (p.kind == PK_WIND) {
            float alpha = std::sin(t * PI) * 0.55f;
            float ang = std::atan2(p.vel.y, p.vel.x) * RAD2DEG;
            float w = 300.0f * p.size, h = 95.0f * p.size;
            DrawTexturePro(a.wind, {0, 0, (float)a.wind.width, (float)a.wind.height}, {p.pos.x, p.pos.y, w, h},
                           {w * 0.5f, h * 0.5f}, ang, Fade(WHITE, alpha));
        } else {
            // grêlon qui tombe en diagonale puis rebondit
            float fall = std::max(0.0f, 1.0f - t / 0.7f);
            Vector2 at = {p.pos.x + fall * 60.0f, p.pos.y - fall * 160.0f};
            if (fall > 0.0f) {
                DrawLineEx({at.x + 18, at.y - 46}, at, 3.0f, Fade(WHITE, 0.5f));
                DrawCircleV(at, p.size, Color{235, 240, 255, 230});
            } else {
                float k = (t - 0.7f) / 0.3f;
                DrawCircleV({p.pos.x, p.pos.y + 4}, p.size * 0.8f, Fade(Color{80, 90, 120, 255}, 0.25f * (1 - k)));
                DrawCircleV({p.pos.x, p.pos.y - std::sin(k * PI) * 18.0f}, p.size * 0.8f, Fade(Color{235, 240, 255, 255}, 1 - k));
            }
        }
    }
}

void Weather::drawAurora(float sw, float sh) const
{
    float in = aurora();
    if (in <= 0.0f) return;
    static const Color cols[3] = {{60, 255, 150, 255}, {70, 220, 255, 255}, {190, 100, 255, 255}};
    BeginBlendMode(BLEND_ADDITIVE);
    const int columns = 90;
    for (int k = 0; k < 3; k++) {
        float base = sh * (0.12f + 0.09f * k);
        float height = sh * (0.22f - 0.03f * k);
        for (int i = 0; i < columns; i++) {
            int x0 = (int)(sw * i / columns), x1 = (int)(sw * (i + 1) / columns);
            float x = (float)x0;
            float u = x / sw;
            float y = base + std::sin(u * 6.0f + t_ * 0.5f + k * 1.7f) * sh * 0.05f
                           + std::sin(u * 17.0f - t_ * 1.1f + k) * sh * 0.015f;
            float pulse = 0.55f + 0.45f * std::sin(u * 9.0f + t_ * (0.8f + 0.3f * k) + k * 2.0f);
            float alpha = in * pulse * (k == 2 ? 0.35f : 0.5f);
            Color top = Fade(cols[k], 0.0f);
            Color mid = Fade(cols[k], alpha);
            DrawRectangleGradientV(x0, (int)(y - height), x1 - x0, (int)height, top, mid);
            DrawRectangleGradientV(x0, (int)y, x1 - x0, (int)(height * 0.25f), mid, top);
        }
    }
    EndBlendMode();
}

void Weather::drawBlizzard(const Assets& a, float sw, float sh) const
{
    float bl = blizzard();
    if (bl <= 0.01f) return;
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(Color{235, 240, 250, 255}, 0.3f * bl));
    // flocons déterministes : position = graine + vent * temps, repliée sur l'écran
    Vector2 dir = blizzardDir();
    float unit = sw / 720.0f;
    const int count = 260;
    for (int i = 0; i < count; i++) {
        unsigned h = (unsigned)i * 2654435761u;
        float fx = (h & 0xFFFF) / 65535.0f, fy = ((h >> 16) & 0xFFFF) / 65535.0f;
        float depth = 0.4f + 0.6f * ((i * 37) % 100) / 100.0f;
        float speed = (500.0f + 900.0f * depth) * unit;
        float wob = std::sin(t_ * 3.0f + i) * 12.0f * unit;
        float x = std::fmod(fx * sw + dir.x * t_ * speed - dir.y * wob, sw);
        float y = std::fmod(fy * sh + dir.y * t_ * speed + dir.x * wob, sh);
        if (x < 0) x += sw;
        if (y < 0) y += sh;
        float r = (1.5f + 3.5f * depth) * unit;
        Color c = Fade(WHITE, bl * (0.5f + 0.5f * depth));
        DrawLineEx({x - dir.x * r * 4, y - dir.y * r * 4}, {x, y}, r * 0.9f, Fade(c, 0.5f));
        DrawCircleV({x, y}, r, c);
    }
    (void)a;
}
