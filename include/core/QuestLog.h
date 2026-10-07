#pragma once

#include <cstdint>

namespace core {

enum class QuestStatus : std::uint8_t {
    NotStarted,
    Accepted,
    Completed
};

struct QuestLog {
    QuestStatus villageLetter = QuestStatus::NotStarted;
};

}
