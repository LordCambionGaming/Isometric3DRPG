#pragma once
#include <array>
#include <string>
#include <vector>
#include <raylib.h>
#include "states/GameState.h"
#include "core/Character.h"

// Nuova Partita -> CharacterCreationState (multi-pagina, gotico, responsive) -> Conferma -> PlayingState.
// Pagina 0: Aspetto & Identita' (Nome, Sesso, Capelli, Colore Pelle, Colore Capelli)
// Pagina 1: Statistiche & Classe (Classe, Metodo punteggi: Tira Dadi / Fissi, Assegnazione punteggi, Conferma)
class CharacterCreationState : public GameState {
public:
    enum class Method { Roll, Fixed };

    struct Progress {
        Method method = Method::Roll;
        core::CharacterClass characterClass = core::CharacterClass::Warrior;
        core::CharacterAppearance appearance{};
        core::AbilityScores pool{};
        std::array<bool, core::kAbilityCount> rolled{};
        std::array<int, core::kAbilityCount> assigned{};
        int page = 1; // se torniamo dai dadi, torniamo alla pagina 1 (statistiche)
        int row = 0;
    };

    explicit CharacterCreationState(Game* game);
    CharacterCreationState(Game* game, const Progress& progress);

    void Enter() override;
    void Exit() override {}
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    // Pagina 0: Aspetto
    static constexpr int kPageAppearance = 0;
    static constexpr int kRowAppName = 0;
    static constexpr int kRowAppGender = 1;
    static constexpr int kRowAppHairStyle = 2;
    static constexpr int kRowAppSkinColor = 3;
    static constexpr int kRowAppHairColor = 4;
    static constexpr int kRowAppNext = 5;
    static constexpr int kRowAppCount = 6;

    // Pagina 1: Statistiche & Classe
    static constexpr int kPageAbilities = 1;
    static constexpr int kRowMethod = 0;
    static constexpr int kRowClass = 1;
    static constexpr int kRowRoll = 2;
    static constexpr int kRowFirstAbility = 3;
    static constexpr int kRowConfirm = kRowFirstAbility + core::kAbilityCount; // 9
    static constexpr int kRowPrevPage = kRowConfirm + 1; // 10
    static constexpr int kRowAbiCount = kRowPrevPage + 1; // 11

    Progress Snapshot() const;
    int RolledCount() const;
    bool RollRowActive() const;
    bool AllAssigned() const;
    bool IsPoolSlotUsed(int slot) const;
    int NextRow(int dir) const;
    void SetMethod(Method method);
    void AutoRollRemaining();
    void StartBoardRoll();
    void CycleClass(int dir);
    void CycleAssignment(int ability, int dir);

    // Appearance cycling
    void CycleGender(int dir);
    void CycleHairStyle(int dir);
    void CycleSkinColor(int dir);
    void CycleHairColor(int dir);

    void UpdatePreviewVisuals();
    void RenderPreview3D(const Rectangle& viewport);

    core::Character Build() const;

    int m_page = kPageAppearance;
    int m_row = 0;

    // Aspetto
    core::CharacterAppearance m_appearance;
    bool m_editingName = false;

    // 3D Preview
    Camera3D m_previewCamera{};
    Model* m_previewBodyModel = nullptr;
    Model* m_previewHairModel = nullptr;
    std::array<Model*, 3> m_previewEquipModels = { nullptr, nullptr, nullptr };
    ModelAnimation* m_previewAnims = nullptr;
    int m_previewAnimCount = 0;
    int m_previewIdleIndex = -1;
    float m_previewAnimTime = 0.0f;
    float m_previewRotation = 0.0f;
    core::Gender m_lastLoadedGender = static_cast<core::Gender>(-1);
    std::string m_lastLoadedHair = "";

    // Statistiche & Classe
    Method m_method = Method::Roll;
    core::CharacterClass m_class = core::CharacterClass::Warrior;
    core::AbilityScores m_pool{};
    std::array<bool, core::kAbilityCount> m_rolled{};
    std::array<int, core::kAbilityCount> m_assigned{};
    bool m_restored = false;
};
