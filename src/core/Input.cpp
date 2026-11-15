#include "Input.hpp"

#include <algorithm>

void Input::update()
{
    began_.clear();
    ended_.clear();
    active_.clear();

    // Points de contact courants
    struct Cur { int id; Vector2 pos; };
    std::vector<Cur> cur;
    int n = GetTouchPointCount();
    for (int i = 0; i < n; i++) {
        Vector2 p = GetTouchPosition(i);
        cur.push_back({GetTouchPointId(i), p});
    }
    // Repli souris (émulateur / tests) : un seul "doigt" d'id -1
    if (n == 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        cur.push_back({-1, GetMousePosition()});
    }

    // Nouveaux touchers
    for (auto& c : cur) {
        auto it = std::find_if(live_.begin(), live_.end(), [&](const Live& l) { return l.id == c.id; });
        if (it == live_.end()) {
            live_.push_back({c.id, c.pos, c.pos});
            began_.push_back({c.id, c.pos, c.pos, false});
        } else {
            it->pos = c.pos;
        }
    }
    // Touchers terminés
    for (size_t i = 0; i < live_.size();) {
        auto it = std::find_if(cur.begin(), cur.end(), [&](const Cur& c) { return c.id == live_[i].id; });
        if (it == cur.end()) {
            ended_.push_back({live_[i].id, live_[i].pos, live_[i].start, false});
            live_.erase(live_.begin() + i);
        } else {
            i++;
        }
    }
    for (auto& l : live_) active_.push_back({l.id, l.pos, l.start, false});

    back_ = IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE);
}

bool Input::tapped(Rectangle r)
{
    for (auto& e : ended_) {
        if (!e.consumed && CheckCollisionPointRec(e.start, r) && CheckCollisionPointRec(e.pos, r)) {
            e.consumed = true;
            return true;
        }
    }
    return false;
}

bool Input::pressed(Rectangle r)
{
    for (auto& e : began_) {
        if (!e.consumed && CheckCollisionPointRec(e.pos, r)) {
            e.consumed = true;
            return true;
        }
    }
    return false;
}

bool Input::held(Rectangle r) const
{
    for (auto& e : active_) {
        if (CheckCollisionPointRec(e.start, r)) return true;
    }
    return false;
}

bool Input::dragPos(Rectangle r, Vector2& out) const
{
    for (auto& e : active_) {
        if (CheckCollisionPointRec(e.start, r)) { out = e.pos; return true; }
    }
    return false;
}

void Input::consumeAllBegan()
{
    for (auto& e : began_) e.consumed = true;
}

void Input::consumeAll()
{
    for (auto& e : began_) e.consumed = true;
    for (auto& e : ended_) e.consumed = true;
    back_ = false;
}
