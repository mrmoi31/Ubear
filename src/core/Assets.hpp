// Chargement centralisé des ressources (textures, sons, police).
// Sur Android, les chemins sont relatifs au dossier assets/ de l'APK.
#pragma once
#include "Save.hpp"

#include <raylib.h>
#include <map>
#include <string>

struct Assets {
    Texture2D body{}, arrow{}, restaurant{}, igloo{}, logo{};
    Texture2D homes[CUSTOMER_COUNT]{};
    Texture2D wind{}, rain{}, fog[3]{}, particle{}, iceTex{};
    Texture2D blizzard{};
    Texture2D light{};      // dégradé radial généré : transparent au centre, opaque au bord (nuit)
    Font      font{};
    Music     music{};
    Sound     coin{}, zip{};
    bool      audioReady = false;

    void load();
    void unload();

    // Texture de cosmétique chargée à la demande (chemin relatif à assets/).
    const Texture2D* cosmetic(const std::string& file);

    // Reconstruit les icônes boutique + la tenue gameplay (hors passe OpenGL).
    void syncLooks(const SaveData& s);

    // Ours de l’aperçu boutique (catégorie courante uniquement, sans sac par défaut).
    void syncShopPreview(const SaveData& s);
    const Texture2D& shopPreview() const { return shopPreview_; }

    // Textures pré-calculées : `fur|…` (pelage), `icon|…` (calque seul).
    const Texture2D* look(const std::string& key) const;
    const Texture2D& gameplayLook() const { return gameplay_; }

    // Igloo de l'habitant, ou l'igloo de passage si l'id est inconnu.
    const Texture2D& homeFor(int customer) const;

    void playSfx(const Sound& s, float volume = 1.0f, float pitch = 1.0f) const;
    float sfxVolume = 1.0f;

private:
    Texture2D compose(const std::string& baseFile, const std::string& skate,
                      const std::string& bag, const std::string& hat);
    const Image* findImage(const std::string& file) const;

    std::map<std::string, Texture2D> cosmetics_;
    std::map<std::string, Image> images_;
    std::map<std::string, Texture2D> looks_;
    Image bodyImage_{};
    Texture2D gameplay_{};
    Texture2D shopPreview_{};
    std::string gameplayKey_;
    std::string shopPreviewKey_;
};
