#pragma once

#include <raylib.h>
#include "core/Character.h"
class Player;

class HUD {
public:
    HUD() = default;
    ~HUD() = default;

    // HUD is rendered after the 3D pass, so every primitive here is screen-space.
    void Render(const Player& player, const Camera3D& camera);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }


  //barra vita sopra un'entità nel mondo 3D
    // Chiamala da Game::Render DOPO il render del mondo, PRIMA di HUD::Render.
    // Proietta worldPos in screen-space e disegna una mini-barra.
     void DrawWorldHealthBar(Vector3 worldPos, float hpFrac,
                            const Camera3D& camera, float barWidthPx = 60.0f) const;

private:
        // Indicatore player a schermo (chevron quando è in vista, freccia ai bordi)
    void DrawPlayerIndicator(const Vector3& playerPosition, const Vector3& headAnchor, const Camera3D& camera) const;
        // Barra vita base (usata dal pannello)
    void DrawHealthBar(const core::Character& c,
                       float x, float y, float w, float h) const;
        // Barra risorsa generica (HP, MP, stamina...)
    void DrawResourceBar(const char* label, int current, int max,
                         Color fillColor,
                         float x, float y, float w, float h) const;
        // Pannello completo del personaggio (in basso a sinistra)
    void DrawCharacterPanel(const core::Character& c) const;
    bool m_enabled = true;


    // Stato visivo interno all'HUD (mutable perché aggiornato in Render const)
    mutable float m_displayedHP = 1.0f;   // valore ANIMATO (0..1)
    mutable int   m_lastHP = -1;          // per rilevare il cambio (flash)
    mutable float m_damageFlash = 0.0f;   // 0..1, timer del flash rosso
    mutable float m_panelAlpha = 1.0f;    // fade del pannello (futuro)
};