#pragma once

#include <string>
#include <string_view>

namespace lang {

enum class Language {
    Italian,
    English
};

void Initialize();
void SetLanguage(Language language);
Language CurrentLanguage();
std::string Get(std::string_view key);
std::string ItemName(std::string_view itemKey);
std::string CharacterClassName(std::string_view enumName);
std::string RaceName(std::string_view enumName);

}

// Shorthand: L("key") => lang::Get("key")
#define L(key) ::lang::Get(key)
