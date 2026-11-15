// Gestion unifiée du tactile multi-points (avec repli souris pour les tests).
#pragma once
#include <raylib.h>
#include <vector>

struct TouchEvent {
    int     id;
    Vector2 pos;       // position actuelle
    Vector2 start;     // position au début du toucher
    bool    consumed;  // déjà traité par l'UI
};

class Input {
public:
    void update();

    std::vector<TouchEvent>&       began()  { return began_; }
    std::vector<TouchEvent>&       ended()  { return ended_; }
    const std::vector<TouchEvent>& active() const { return active_; }

    // Un toucher a commencé ET terminé dans le rectangle (clic mobile).
    bool tapped(Rectangle r);
    // Un toucher a commencé dans le rectangle (marque l'événement consommé).
    bool pressed(Rectangle r);
    // Un toucher en cours a commencé dans le rectangle (maintien).
    bool held(Rectangle r) const;
    // Position d'un toucher actif ayant commencé dans r (pour les sliders).
    bool dragPos(Rectangle r, Vector2& out) const;

    bool backPressed() const { return back_; }
    void consumeAllBegan();
    void consumeAll();

private:
    struct Live { int id; Vector2 start; Vector2 pos; };
    std::vector<Live>       live_;
    std::vector<TouchEvent> began_, ended_, active_;
    bool back_ = false;
};
