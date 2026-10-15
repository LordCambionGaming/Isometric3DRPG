#pragma once

#include "states/GameState.h"
#include "world/Village.h"
#include "ui/MedievalMenu.h"
#include <string>
#include <vector>

class VillageInteractionState : public GameState {
public:
    VillageInteractionState(Game* game, VillageNpc npc, int returnBuilding = -1);

    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    enum class Page { Main, Talk, Trade, Quest };
    enum class Action { Talk, Trade, Quest, Back, Buy };

    struct Choice {
        ui::menu::MenuItem item;
        Action action;
        int payload = 0;
    };

    VillageNpc m_npc;
    int m_returnBuilding = -1;
    Page m_page = Page::Main;
    int m_selected = 0;
    int m_lastClicked = -1; // Minecraft-style: first click selects, second click activates
    std::string m_messageKey;

    std::vector<Choice> Choices() const;
    void SelectChoice(int index);
    void HandleQuest();
    void ReturnToOrigin();
    std::string RoleKey() const;
};
