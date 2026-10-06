#pragma once
#include <array>
#include "states/GameState.h"
#include "core/Character.h"

// Nuova Partita -> CharacterCreationState -> Conferma -> PlayingState.
// Human / Livello 1 / XP 0 sono fissi. Il giocatore sceglie la classe e il METODO per i punteggi:
//   Tira i dadi     : 4d6 scartando il piu' basso, sei volte. Ogni punteggio si lancia sulla board gotica
//                     (INVIO sulla riga "Lancia i dadi") oppure si tirano tutti i rimanenti in automatico (A).
//   Punteggi fissi  : array standard D&D 5e 15, 14, 13, 12, 10, 8.
// Poi assegna i sei punteggi alle sei caratteristiche.
//   SU/GIU  : seleziona la riga
//   SX/DX   : cambia metodo / classe / punteggio assegnato alla caratteristica
//   INVIO   : su "Lancia i dadi" apre la board; su "Conferma" (se tutto e' assegnato) avvia la partita
//   A       : tira in automatico i punteggi non ancora lanciati
//   ESC     : torna al menu principale (nessuna sessione e' stata creata)
class CharacterCreationState : public GameState {
public:
    enum class Method { Roll, Fixed };

    // Fotografia della creazione a meta': la board dei dadi sostituisce questo stato, poi se ne ricrea uno
    // identico con il nuovo punteggio dentro.
    struct Progress {
        Method method = Method::Roll;
        core::CharacterClass characterClass = core::CharacterClass::Warrior;
        core::AbilityScores pool{};
        std::array<bool, core::kAbilityCount> rolled{};     // quali punteggi del pool sono gia' noti
        std::array<int, core::kAbilityCount> assigned{};    // per caratteristica: indice in pool, -1 = non assegnato
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
    static constexpr int kRowMethod = 0;
    static constexpr int kRowClass = 1;
    static constexpr int kRowRoll = 2;
    static constexpr int kRowFirstAbility = 3;
    static constexpr int kRowConfirm = kRowFirstAbility + core::kAbilityCount;
    static constexpr int kRowCount = kRowConfirm + 1;

    Progress Snapshot() const;
    int RolledCount() const;
    bool RollRowActive() const;           // c'e' ancora qualcosa da lanciare
    bool AllAssigned() const;
    bool IsPoolSlotUsed(int slot) const;
    int NextRow(int dir) const;           // salta la riga "Lancia i dadi" quando non serve
    void SetMethod(Method method);
    void AutoRollRemaining();
    void StartBoardRoll();
    void CycleClass(int dir);
    void CycleAssignment(int ability, int dir);
    core::Character Build() const;

    Method m_method = Method::Roll;
    core::CharacterClass m_class = core::CharacterClass::Warrior;
    core::AbilityScores m_pool{};                          // punteggi: fissi una volta noti
    std::array<bool, core::kAbilityCount> m_rolled{};      // quali punteggi del pool sono noti
    std::array<int, core::kAbilityCount> m_assigned{};     // per caratteristica: indice in m_pool, -1 = non assegnato
    int m_row = kRowRoll;
    bool m_restored = false;                               // ricreato dopo la board: non reinizializzare in Enter()
};