// UI en mode immédiat, dans un espace virtuel d'au moins 720 x 1100 unités.
// Les rectangles sont exprimés en unités virtuelles ; la conversion vers les pixels
// écran (et donc vers les points de contact) est faite ici.
#pragma once
#include "Assets.hpp"
#include "Input.hpp"

#include <raylib.h>

enum class Align { Left, Center, Right };

// Noms en minuscules : raylib définit déjà RED, BLUE, GOLD... comme macros.
namespace pal {
constexpr Color bg     {22, 58, 104, 255};
constexpr Color panel  {250, 252, 255, 240};
constexpr Color ink    {30, 42, 64, 255};
constexpr Color soft   {96, 110, 134, 255};
constexpr Color blue   {58, 128, 214, 255};
constexpr Color red    {226, 72, 72, 255};
constexpr Color green  {66, 176, 104, 255};
constexpr Color gold   {245, 186, 40, 255};
constexpr Color purple {140, 92, 220, 255};
constexpr Color grey   {150, 160, 175, 255};
constexpr Color dim    {10, 18, 34, 170};
}

Color shade(Color c, float k);          // k < 1 assombrit, k > 1 éclaircit
Color withAlpha(Color c, float a);      // a dans [0, 1]

class Ui {
public:
    float W = 720.0f, H = 1280.0f, scale = 1.0f;

    void attach(Assets* a, Input* in) { assets_ = a; input_ = in; }
    void beginFrame();                  // recalcule l'échelle selon l'écran
    void begin();                       // passe en espace virtuel
    void end();

    Rectangle toScreen(Rectangle r) const;
    Vector2   toScreen(Vector2 p) const;
    Vector2   toVirtual(Vector2 p) const;

    Vector2 measure(const char* t, float size) const;
    void    text(const char* t, float x, float y, float size, Color c, Align a = Align::Left) const;
    void    textShadow(const char* t, float x, float y, float size, Color c, Align a = Align::Left) const;
    // Retour à la ligne automatique ; renvoie la hauteur utilisée.
    float   textBox(const char* t, Rectangle box, float size, Color c, Align a = Align::Left, bool draw = true) const;

    void panel(Rectangle r, Color c, float roundness = 0.18f) const;
    void bar(Rectangle r, float t, Color fg, Color bg) const;
    void coinIcon(Vector2 c, float r) const;

    bool button(Rectangle r, const char* label, Color bg, float size = 40.0f, bool enabled = true);
    bool slider(Rectangle r, float& v);
    bool tapped(Rectangle r);
    bool pressed(Rectangle r);
    bool held(Rectangle r) const;

    const Font& font() const { return assets_->font; }
    Input&      input() { return *input_; }

private:
    Assets*  assets_ = nullptr;
    Input*   input_ = nullptr;
    Camera2D cam_{};
};
