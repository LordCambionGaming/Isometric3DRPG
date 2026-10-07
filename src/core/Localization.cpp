#include "core/Localization.h"

#include <fstream>
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace lang {
namespace {

Language g_language = Language::Italian;
std::unordered_map<std::string, std::string> g_strings;
constexpr const char* kLanguageConfigPath = "language.cfg";

const char* LanguageCode(Language language) {
    return language == Language::English ? "en" : "it";
}

void LoadStrings(Language language) {
    g_strings.clear();
    std::ifstream file(std::string("lang/") + LanguageCode(language) + ".json");
    if (!file) return;

    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        if (!root.is_object()) return;
        for (auto it = root.begin(); it != root.end(); ++it) {
            if (it.value().is_string()) g_strings.emplace(it.key(), it.value().get<std::string>());
        }
    } catch (const nlohmann::json::exception&) {
        g_strings.clear();
    }
}

void SaveLanguage() {
    std::ofstream file(kLanguageConfigPath, std::ios::trunc);
    if (file) file << LanguageCode(g_language) << '\n';
}

}

void Initialize() {
    Language language = Language::Italian;
    std::ifstream file(kLanguageConfigPath);
    std::string code;
    if (file >> code && code == "en") language = Language::English;
    g_language = language;
    LoadStrings(g_language);
    if (g_strings.empty()) {
        g_language = Language::Italian;
        LoadStrings(g_language);
    }
}

void SetLanguage(Language language) {
    if (g_language == language && !g_strings.empty()) return;
    g_language = language;
    LoadStrings(g_language);
    if (g_strings.empty() && language != Language::Italian) {
        g_language = Language::Italian;
        LoadStrings(g_language);
    }
    SaveLanguage();
}

Language CurrentLanguage() {
    return g_language;
}

std::string Get(std::string_view key) {
    const auto found = g_strings.find(std::string(key));
    return found == g_strings.end() ? std::string(key) : found->second;
}

std::string ItemName(std::string_view itemKey) {
    return Get(std::string("item.") + std::string(itemKey));
}

std::string CharacterClassName(std::string_view enumName) {
    std::string normalized(enumName);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return Get("character.class." + normalized);
}

std::string RaceName(std::string_view enumName) {
    std::string normalized(enumName);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return Get("character.race." + normalized);
}

}
