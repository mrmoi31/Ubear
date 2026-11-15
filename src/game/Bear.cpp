#include "Bear.hpp"
#include "Catalog.hpp"
#include "../core/Config.hpp"

#include <raymath.h>
#include <algorithm>
#include <cmath>

// Deux zones : gauche tourne à gauche, droite à droite. Même angle des deux côtés.
static const float TURN[PAW_COUNT] = {-36.0f, 36.0f, -36.0f, 36.0f};

// Repères dans Body.png (676 x 820).
static const Vector2   BODY_CENTER = {338.0f, 410.0f};
static const Vector2   PAW_PX[PAW_COUNT] = {{75, 570}, {600, 572}, {110, 240}, {575, 240}};

void Bear::push(Paw p, float impulse, float turnScale)
{
    heading += TURN[p] * turnScale;
    vel = Vector2Add(vel, Vector2Scale(forward(), impulse));
    pawPulse[p] = 1.0f;
}

Vector2 Bear::forward() const
{
    float a = heading * DEG2RAD;
    return {std::sin(a), -std::cos(a)};
}

float Bear::speed() const { return Vector2Length(vel); }

void Bear::update(float dt, const BearEnv& env)
{
    vel = Vector2Add(vel, Vector2Scale(env.wind, dt));
    float damping = (env.water ? cfg::DAMPING_WATER : cfg::DAMPING_ICE) + env.extraDamping;
    vel = Vector2Scale(vel, std::exp(-damping * dt));
    float sp = Vector2Length(vel);
    if (sp > cfg::MAX_SPEED) vel = Vector2Scale(vel, cfg::MAX_SPEED / sp);
    pos = Vector2Add(pos, Vector2Scale(vel, dt));

    const float m = 150.0f;
    if (pos.x < m) { pos.x = m; vel.x = std::fabs(vel.x) * 0.3f; }
    if (pos.y < m) { pos.y = m; vel.y = std::fabs(vel.y) * 0.3f; }
    if (pos.x > cfg::WORLD_SIZE - m) { pos.x = cfg::WORLD_SIZE - m; vel.x = -std::fabs(vel.x) * 0.3f; }
    if (pos.y > cfg::WORLD_SIZE - m) { pos.y = cfg::WORLD_SIZE - m; vel.y = -std::fabs(vel.y) * 0.3f; }

    angle += (heading - angle) * (1.0f - std::exp(-cfg::TURN_SMOOTH * dt));
    for (float& p : pawPulse) p = std::max(0.0f, p - dt * 4.0f);
}

Vector2 Bear::pawWorld(Paw p) const
{
    Vector2 off = Vector2Scale(Vector2Subtract(PAW_PX[p], BODY_CENTER), cfg::BEAR_SCALE);
    return Vector2Add(pos, Vector2Rotate(off, angle * DEG2RAD));
}

static void drawPart(const Texture2D& t, Vector2 c, float angle, float scale, Vector2 localCenter,
                     float w, float h, Color tint)
{
    Vector2 off = Vector2Scale(Vector2Subtract(localCenter, BODY_CENTER), scale);
    Vector2 r = Vector2Rotate(off, angle * DEG2RAD);
    Rectangle dst = {c.x + r.x, c.y + r.y, w * scale, h * scale};
    DrawTexturePro(t, {0, 0, (float)t.width, (float)t.height}, dst, {dst.width * 0.5f, dst.height * 0.5f}, angle, tint);
}

void drawBear(Assets& a, const SaveData& s, Vector2 pos, float angleDeg, float scale, const float* pulse, bool shadow, bool defaultBag,
              const Texture2D* lookOverride)
{
    (void)s;
    (void)pulse;
    (void)defaultBag;
    const Texture2D& look = (lookOverride && lookOverride->id) ? *lookOverride
        : (a.gameplayLook().id ? a.gameplayLook() : a.body);
    float w = (float)a.body.width, h = (float)a.body.height;
    if (shadow) {
        Vector2 sh = {pos.x + 30.0f * scale, pos.y + 45.0f * scale};
        drawPart(look, sh, angleDeg, scale, BODY_CENTER, w, h, Color{20, 40, 80, 60});
    }
    drawPart(look, pos, angleDeg, scale, BODY_CENTER, w, h, WHITE);
}
