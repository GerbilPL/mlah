#include "glinguist.hpp"

namespace gLinguist
{
    void Linguist::flattenJson(const json& j, std::string prefix, std::unordered_map<std::string, std::string>& targetMap) {
        for (auto& [key, value] : j.items()) {
            std::string fullKey = prefix.empty() ? key : prefix + "." + key;
            if (value.is_object()) {
                flattenJson(value, fullKey, targetMap);
            }
            else if (value.is_string()) {
                targetMap[fullKey] = value.get<std::string>();
            }
            else {
                // Preserve non-string JSON leaves as their serialized JSON representation.
                targetMap[fullKey] = value.dump();
            }
        }
    }

    bool Linguist::loadFileToData(const std::string& path, LocaleData& dataRef) {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        try {
            json j = json::parse(file);
            dataRef.mapping.clear();

            dataRef.ltr = j.value("ltr", true);

            if (j.contains("dict") && j["dict"].is_object()) {
                flattenJson(j["dict"], "", dataRef.mapping);
            } else {
                flattenJson(j, "", dataRef.mapping);
            }
            dataRef.loaded = true;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "gLinguist: Error loading file " << path << ": " << e.what() << std::endl;
            return false;
        }
    }

    std::expected<std::string, bool> Linguist::findKey(const std::string& k) {
        if (currentLocale.loaded) {
            auto it = currentLocale.mapping.find(k);
            if (it != currentLocale.mapping.end()) {
                return it->second;
            }
        }
        if (fallbackLocale.loaded) {
            auto it = fallbackLocale.mapping.find(k);
            if (it != fallbackLocale.mapping.end()) {
                return it->second;
            }
        }
        return std::unexpected(false);
    }

    void Linguist::findAll(std::string folderPath) {
        availableLocales.clear();
        if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
            std::cerr << "gLinguist: Directory does not exist: " << folderPath << std::endl;
            return;
        }

        int index = 0;
        for (const auto& entry : fs::directory_iterator(folderPath)) {
            if (entry.path().extension() == ".json") {
                try {
                    std::ifstream f(entry.path());
                    json j = json::parse(f);

                    LocaleMeta meta;
                    meta.id = index;
                    meta.path = entry.path().string();
                    meta.code = j.value("id", "unknown");
                    meta.name = j.value("name", entry.path().stem().string());
                    meta.ltr = j.value("ltr", true);

                    availableLocales.push_back(meta);
                    index++;
                } catch (const std::exception& e) {
                    std::cerr << "gLinguist: Error scanning file " << entry.path() << ": " << e.what() << std::endl;
                }
            }
        }
    }

    std::unordered_map<int, std::string> Linguist::listAll() {
        std::unordered_map<int, std::string> list;
        for (const auto& locale : availableLocales) {
            list[locale.id] = locale.name;
        }
        return list;
    }

    bool Linguist::initFallback(std::string codeToFind) {
        for (const auto& loc : availableLocales) {
            if (loc.code == codeToFind) {
                fallbackId = loc.id;
                return loadFileToData(loc.path, fallbackLocale);
            }
        }
        std::cerr << "gLinguist: Fallback locale not found: " << codeToFind << std::endl;
        return false;
    }

    bool Linguist::loadLocale(int id) {
        if (id < 0 || id >= static_cast<int>(availableLocales.size())) return false;

        currentId = id;
        if (id == fallbackId) {
            currentLocale.mapping.clear();
            currentLocale.loaded = false;
            currentLocale.ltr = availableLocales[id].ltr;
            std::cout << "gLinguist: Default language selected. Using fallback instance." << std::endl;
            return true;
        }

        std::cout << "gLinguist: Loading language " << availableLocales[id].name << std::endl;
        return loadFileToData(availableLocales[id].path, currentLocale);
    }

    std::string Linguist::getKey(std::string key) {
        auto result = findKey(key);
        return result.has_value() ? result.value() : key;
    }

    bool Linguist::isLtr() {
        if (this->currentId != -1 && currentId < static_cast<int>(availableLocales.size())) {
            return availableLocales[currentId].ltr;
        }
        return true;
    }

    std::string Linguist::getCurrentCode() {
        if (this->currentId != -1 && currentId < static_cast<int>(availableLocales.size())) {
            return availableLocales[currentId].code;
        }
        return "none";
    }
}
