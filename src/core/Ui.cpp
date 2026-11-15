#include "Ui.hpp"
#include "Config.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

Color shade(Color c, float k)
{
    auto f = [k](unsigned char v) { return (unsigned char)std::clamp(v * k, 0.0f, 255.0f); };
    return Color{f(c.r), f(c.g), f(c.b), c.a};
}

Color withAlpha(Color c, float a)
{
    c.a = (unsigned char)std::clamp(c.a * a, 0.0f, 255.0f);
    return c;
}

void Ui::beginFrame()
{
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    scale = std::min(sw / (float)cfg::DESIGN_W, sh / 1100.0f);
    if (scale <= 0.0f) scale = 1.0f;
    W = sw / scale;
    H = sh / scale;
    cam_ = Camera2D{};
    cam_.zoom = scale;
}

void Ui::begin() { BeginMode2D(cam_); }
void Ui::end()   { EndMode2D(); }

Rectangle Ui::toScreen(Rectangle r) const { return {r.x * scale, r.y * scale, r.width * scale, r.height * scale}; }
Vector2   Ui::toScreen(Vector2 p) const   { return {p.x * scale, p.y * scale}; }
Vector2   Ui::toVirtual(Vector2 p) const  { return {p.x / scale, p.y / scale}; }

Vector2 Ui::measure(const char* t, float size) const
{
    return MeasureTextEx(assets_->font, t, size, size * 0.02f);
}

void Ui::text(const char* t, float x, float y, float size, Color c, Align a) const
{
    if (a != Align::Left) {
        float w = measure(t, size).x;
        x -= (a == Align::Center) ? w * 0.5f : w;
    }
    DrawTextEx(assets_->font, t, {std::round(x), std::round(y)}, size, size * 0.02f, c);
}

void Ui::textShadow(const char* t, float x, float y, float size, Color c, Align a) const
{
    text(t, x + 2, y + 3, size, Color{0, 0, 0, (unsigned char)(c.a * 0.55f)}, a);
    text(t, x, y, size, c, a);
}

float Ui::textBox(const char* t, Rectangle box, float size, Color c, Align a, bool draw) const
{
    std::vector<std::string> lines;
    std::string para;
    auto flushPara = [&](const std::string& p) {
        std::string line, word;
        size_t i = 0;
        while (i <= p.size()) {
            if (i == p.size() || p[i] == ' ') {
                if (!word.empty()) {
                    std::string cand = line.empty() ? word : line + " " + word;
                    if (!line.empty() && measure(cand.c_str(), size).x > box.width) {
                        lines.push_back(line);
                        line = word;
                    } else {
                        line = cand;
                    }
                    word.clear();
                }
            } else {
                word += p[i];
            }
            i++;
        }
        lines.push_back(line);
    };
    for (const char* p = t; ; p++) {
        if (*p == '\n' || *p == '\0') { flushPara(para); para.clear(); if (*p == '\0') break; }
        else para += *p;
    }
    float lh = size * 1.15f;
    if (draw) {
        for (size_t i = 0; i < lines.size(); i++) {
            float x = box.x;
            if (a == Align::Center) x = box.x + box.width * 0.5f;
            else if (a == Align::Right) x = box.x + box.width;
            text(lines[i].c_str(), x, box.y + i * lh, size, c, a);
        }
    }
    return lines.size() * lh;
}

void Ui::panel(Rectangle r, Color c, float roundness) const
{
    DrawRectangleRounded({r.x, r.y + 6, r.width, r.height}, roundness, 10, Color{0, 0, 0, (unsigned char)(c.a * 0.25f)});
    DrawRectangleRounded(r, roundness, 10, c);
}

void Ui::bar(Rectangle r, float t, Color fg, Color bg) const
{
    t = std::clamp(t, 0.0f, 1.0f);
    DrawRectangleRounded(r, 1.0f, 8, bg);
    if (t > 0.0f) {
        Rectangle f = r;
        f.width = std::max(r.height, r.width * t);
        DrawRectangleRounded(f, 1.0f, 8, fg);
    }
}

void Ui::coinIcon(Vector2 c, float r) const
{
    DrawCircleV({c.x, c.y + r * 0.12f}, r, Color{176, 120, 10, 255});
    DrawCircleV(c, r, pal::gold);
    DrawCircleV(c, r * 0.72f, Color{255, 214, 92, 255});
    DrawRectangleRounded({c.x - r * 0.14f, c.y - r * 0.45f, r * 0.28f, r * 0.9f}, 0.6f, 4, Color{214, 150, 20, 255});
}

bool Ui::tapped(Rectangle r)  { return input_->tapped(toScreen(r)); }
bool Ui::pressed(Rectangle r) { return input_->pressed(toScreen(r)); }
bool Ui::held(Rectangle r) const { return input_->held(toScreen(r)); }

bool Ui::button(Rectangle r, const char* label, Color bg, float size, bool enabled)
{
    bool down = enabled && held(r);
    Color base = enabled ? bg : shade(bg, 0.55f);
    float lift = down ? 2.0f : 0.0f;
    DrawRectangleRounded({r.x, r.y + 7, r.width, r.height}, 0.35f, 10, shade(base, 0.6f));
    DrawRectangleRounded({r.x, r.y + lift, r.width, r.height}, 0.35f, 10, down ? shade(base, 0.85f) : base);
    Vector2 m = measure(label, size);
    text(label, r.x + r.width * 0.5f, r.y + lift + (r.height - m.y) * 0.5f, size,
         enabled ? WHITE : Color{230, 230, 230, 200}, Align::Center);
    return enabled && tapped(r);
}

bool Ui::slider(Rectangle r, float& v)
{
    Rectangle hit = {r.x - 20, r.y - 30, r.width + 40, r.height + 60};
    Vector2 p;
    bool changed = false;
    if (input_->dragPos(toScreen(hit), p)) {
        Vector2 vp = toVirtual(p);
        float nv = std::clamp((vp.x - r.x) / r.width, 0.0f, 1.0f);
        changed = std::fabs(nv - v) > 0.001f;
        v = nv;
    }
    bar(r, v, pal::blue, Color{210, 220, 235, 255});
    DrawCircleV({r.x + r.width * v, r.y + r.height * 0.5f}, r.height * 1.1f, WHITE);
    DrawCircleLinesV({r.x + r.width * v, r.y + r.height * 0.5f}, r.height * 1.1f, pal::blue);
    return changed;
}
