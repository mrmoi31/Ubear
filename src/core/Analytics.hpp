// Statistiques joueurs anonymes, envoyées par lots à PostHog (format /batch/).
// Aucune donnée personnelle : un identifiant aléatoire par installation.
// Rien n'est collecté avant l'accord du joueur (premier lancement) ; modifiable dans les Options.
#pragma once
#include "Save.hpp"

#include <initializer_list>
#include <string>

namespace analytics {

struct Prop {
    const char* key;
    std::string json;   // valeur déjà encodée en JSON
};
Prop num(const char* key, double v);
Prop str(const char* key, const std::string& v);
Prop flag(const char* key, bool v);

void start(SaveData& save);    // au lancement : identifiant d'installation + "session_start"
void foreground(bool on);      // appli au premier plan / en arrière-plan (sessions, envoi)
void track(const char* event, std::initializer_list<Prop> props = {});
void flush();                  // envoie les événements en attente
void setEnabled(bool on);      // option du joueur ; faux = plus rien n'est collecté

std::string escape(const std::string& s);   // exposé pour l'autotest
}
