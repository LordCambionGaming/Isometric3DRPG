#pragma once
#include <array>
#include "core/GameState.h"
#include "core/Character.h"

// Nuova Partita -> CharacterCreationState -> Conferma -> PlayingState.
// Human / Livello 1 / XP 0 sono fissi. Il giocatore sceglie la classe e assegna alle sei
// caratteristiche i sei punteggi (4d6 scartando il piu' basso) tirati UNA volta in Enter().
//   SU/GIU  : seleziona la riga (Classe, STR..CHA, Conferma)
//   SX/DX   : cambia classe / cambia il punteggio assegnato alla caratteristica
//   INVIO   : sulla riga Conferma (solo se tutti i punteggi sono assegnati) avvia la partita
//   ESC     : torna al menu principale (nessuna sessione e' stata creata)
class CharacterCreationState : public GameState {
public:
    explicit CharacterCreationState(Game* game);

    void Enter() override;
    void Exit() override {}
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    static constexpr int kRowClass = 0;
    static constexpr int kRowFirstAbility = 1;
    static constexpr int kRowConfirm = kRowFirstAbility + core::kAbilityCount;
    static constexpr int kRowCount = kRowConfirm + 1;

    bool AllAssigned() const;
    bool IsPoolSlotUsed(int slot) const;
    void CycleClass(int dir);
    void CycleAssignment(int ability, int dir);
    core::Character Build() const;

    core::CharacterClass m_class = core::CharacterClass::Warrior;
    core::AbilityScores m_pool{};       // i sei punteggi tirati: fissi dopo Enter()
    std::array<int, core::kAbilityCount> m_assigned{};   // per caratteristica: indice in m_pool, -1 = non assegnato
    int m_row = kRowClass;
};