#include "states/VillageInteractionState.h"

#include "core/Game.h"
#include "core/Localization.h"
#include "core/KeyBindings.h"
#include "entities/Player.h"
#include "states/PlayingState.h"
#include "states/VillageInteriorState.h"
#include <array>
#include <memory>
#include <raylib.h>

namespace {

struct Offer {
    core::ItemId item;
    int count;
    int price;
};

constexpr std::array<Offer, 5> kSmithOffers = {{
    { core::items::Dagger, 1, 8 },
    { core::items::Shortsword, 1, 18 },
    { core::items::Longsword, 1, 35 },
    { core::items::Greatsword, 1, 55 },
    { core::items::Bow, 1, 30 }
}};

constexpr std::array<Offer, 3> kInnOffers = {{
    { core::items::Arrow, 10, 3 },
    { core::items::Quiver, 1, 8 },
    { core::items::Dagger, 1, 12 }
}};

bool IsMerchant(VillageNpcRole role) {
    return role == VillageNpcRole::Blacksmith || role == VillageNpcRole::Innkeeper;
}

bool IsQuestGiver(VillageNpcRole role) {
    return role == VillageNpcRole::Mayor || role == VillageNpcRole::Cleric;
}

}

VillageInteractionState::VillageInteractionState(Game* game, VillageNpc npc, int returnBuilding)
    : GameState(game), m_npc(std::move(npc)), m_returnBuilding(returnBuilding) {
    m_type = GameStateType::VillageDialogue;
}

void VillageInteractionState::Enter() {}
void VillageInteractionState::Exit() {}
void VillageInteractionState::Update(float deltaTime) {
    (void)deltaTime;
}

void VillageInteractionState::ReturnToOrigin() {
    if (m_returnBuilding >= 0)
        m_game->SetState(std::make_unique<VillageInteriorState>(m_game, m_returnBuilding));
    else
        m_game->SetState(std::make_unique<PlayingState>(m_game));
}

std::string VillageInteractionState::RoleKey() const {
    switch (m_npc.role) {
        case VillageNpcRole::Blacksmith: return "blacksmith";
        case VillageNpcRole::Mayor: return "mayor";
        case VillageNpcRole::Cleric: return "cleric";
        case VillageNpcRole::Farmer: return "farmer";
        case VillageNpcRole::Innkeeper: return "innkeeper";
    }
    return "villager";
}

std::vector<VillageInteractionState::Choice> VillageInteractionState::Choices() const {
    if (m_page == Page::Talk || m_page == Page::Quest) {
        return { { { lang::Get("settings.back"), "" }, Action::Back } };
    }

    if (m_page == Page::Trade) {
        std::vector<Choice> choices;
        const auto addOffer = [&](const Offer& offer) {
            const core::ItemDef* item = core::GetItem(offer.item);
            if (!item) return;
            const std::string detail = std::to_string(offer.price) + " " + lang::Get("currency.gold");
            choices.push_back({ { lang::ItemName(item->key) + " x" + std::to_string(offer.count), detail },
                                Action::Buy, static_cast<int>(&offer - (m_npc.role == VillageNpcRole::Blacksmith
                                    ? kSmithOffers.data() : kInnOffers.data())) });
        };
        if (m_npc.role == VillageNpcRole::Blacksmith) {
            for (const Offer& offer : kSmithOffers) addOffer(offer);
        } else {
            for (const Offer& offer : kInnOffers) addOffer(offer);
        }
        choices.push_back({ { lang::Get("settings.back"), "" }, Action::Back });
        return choices;
    }

    std::vector<Choice> choices = {
        { { lang::Get("village.action.talk"), "" }, Action::Talk }
    };
    if (IsMerchant(m_npc.role)) choices.push_back({ { lang::Get("village.action.trade"), "" }, Action::Trade });
    if (IsQuestGiver(m_npc.role)) choices.push_back({ { lang::Get("village.action.quest"), "" }, Action::Quest });
    choices.push_back({ { lang::Get("settings.back"), "" }, Action::Back });
    return choices;
}

void VillageInteractionState::HandleQuest() {
    core::QuestLog& quests = m_game->GetQuestLog();
    if (m_npc.role == VillageNpcRole::Mayor) {
        if (quests.villageLetter == core::QuestStatus::NotStarted) {
            quests.villageLetter = core::QuestStatus::Accepted;
            m_messageKey = "quest.accepted";
        } else if (quests.villageLetter == core::QuestStatus::Accepted) {
            m_messageKey = "quest.active";
        } else {
            m_messageKey = "quest.completed";
        }
        return;
    }

    if (quests.villageLetter == core::QuestStatus::Accepted) {
        quests.villageLetter = core::QuestStatus::Completed;
        if (Player* player = m_game->GetPlayer()) player->GetCharacterMutable().gold += 30;
        m_messageKey = "quest.reward";
    } else if (quests.villageLetter == core::QuestStatus::Completed) {
        m_messageKey = "quest.completed";
    } else {
        m_messageKey = "quest.no_letter";
    }
}

void VillageInteractionState::SelectChoice(int index) {
    const std::vector<Choice> choices = Choices();
    if (index < 0 || index >= static_cast<int>(choices.size())) return;
    const Choice choice = choices[static_cast<std::size_t>(index)];
    switch (choice.action) {
        case Action::Talk:
            m_page = Page::Talk;
            m_selected = 0;
            m_lastClicked = -1;
            m_messageKey = "village.talk." + RoleKey();
            break;
        case Action::Trade:
            m_page = Page::Trade;
            m_messageKey.clear();
            m_selected = 0;
            m_lastClicked = -1;
            break;
        case Action::Quest:
            m_page = Page::Quest;
            m_selected = 0;
            m_lastClicked = -1;
            HandleQuest();
            break;
        case Action::Back:
            if (m_page == Page::Main) {
                ReturnToOrigin();
            } else {
                m_page = Page::Main;
                m_selected = 0;
                m_lastClicked = -1;
            }
            break;
        case Action::Buy: {
            const Offer* offer = nullptr;
            if (m_npc.role == VillageNpcRole::Blacksmith && choice.payload >= 0 &&
                choice.payload < static_cast<int>(kSmithOffers.size())) {
                offer = &kSmithOffers[static_cast<std::size_t>(choice.payload)];
            } else if (m_npc.role == VillageNpcRole::Innkeeper && choice.payload >= 0 &&
                       choice.payload < static_cast<int>(kInnOffers.size())) {
                offer = &kInnOffers[static_cast<std::size_t>(choice.payload)];
            }
            Player* player = m_game->GetPlayer();
            if (!offer || !player) break;
            core::Character& character = player->GetCharacterMutable();
            if (character.gold < offer->price) {
                m_messageKey = "trade.not_enough_gold";
            } else {
                character.gold -= offer->price;
                character.AddItem(offer->item, offer->count);
                m_messageKey = "trade.purchase_complete";
            }
            break;
        }
    }
}

void VillageInteractionState::Render() {
    const std::vector<Choice> choices = Choices();
    std::vector<ui::menu::MenuItem> items;
    items.reserve(choices.size());
    for (const Choice& choice : choices) items.push_back(choice.item);

    std::string subtitle = lang::Get("village.role." + RoleKey());
    if (m_page == Page::Talk || m_page == Page::Quest) {
        subtitle = lang::Get(m_messageKey);
    } else if (m_page == Page::Trade) {
        if (const Player* player = m_game->GetPlayer()) {
            subtitle = TextFormat(lang::Get("currency.balance").c_str(), player->GetCharacter().gold);
            if (!m_messageKey.empty()) subtitle += "  |  " + lang::Get(m_messageKey);
        }
    }

    ui::menu::Draw(lang::Get(m_npc.id).c_str(), subtitle.c_str(), items, m_selected);
}

void VillageInteractionState::HandleInput() {
    const std::vector<Choice> choices = Choices();
    const int count = static_cast<int>(choices.size());
    if (count == 0) return;

    // Minecraft-style: first click selects (highlights), second click on same option activates.
    const int clickedOption = IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
        ? ui::menu::OptionAt(GetMousePosition(), count) : -1;

    if (clickedOption >= 0) {
        if (clickedOption == m_lastClicked) {
            // Second click → activate
            m_selected = clickedOption;
            m_lastClicked = -1;
            SelectChoice(m_selected);
            return;
        } else {
            // First click → just highlight
            m_selected = clickedOption;
            m_lastClicked = clickedOption;
            return;
        }
    } else {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) m_lastClicked = -1;
    }

    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Back)) {
        m_lastClicked = -1;
        if (m_page == Page::Main) ReturnToOrigin();
        else { m_page = Page::Main; m_selected = 0; }
        return;
    }
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Up)) {
        m_selected = (m_selected + count - 1) % count;
    } else if (core::KeyBindings::Pressed(core::KeyBindings::Action::Down)) {
        m_selected = (m_selected + 1) % count;
    } else if (core::KeyBindings::Pressed(core::KeyBindings::Action::Confirm)) {
        m_lastClicked = -1;
        SelectChoice(m_selected);
    }
}
