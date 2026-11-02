#pragma once
#include <string>
#include <expected>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace gLinguist
{
    /**
     * @struct LocaleMeta
     * @brief Metadata discovered for a locale JSON file.
     * @note `id` is assigned during directory scanning and is not a stable identifier.
     */
    struct LocaleMeta {
        int id;
        std::string code;
        std::string name;
        std::string path;
        bool ltr;
    };

    /**
     * @struct LocaleData
     * @brief Loaded locale strings, flattened into dotted keys.
     */
    struct LocaleData
    {
        bool loaded = false;
        bool ltr = true;
        std::unordered_map<std::string, std::string> mapping;
    };

    /**
     * @class Linguist
     * @brief Loads locale JSON files and resolves translation keys.
     *
     * Call `findAll()` before selecting a fallback or loading a locale. A
     * missing key falls back to the fallback locale, then to the key itself.
     * Locale IDs are valid only for the current scan; directory iteration order
     * is not guaranteed to be stable.
     */
    class Linguist {
    private:
        std::vector<LocaleMeta> availableLocales;
        LocaleData currentLocale;
        LocaleData fallbackLocale;

        int fallbackId = -1;
        int currentId = -1;

        void flattenJson(const json& j, std::string prefix, std::unordered_map<std::string, std::string>& targetMap);
        bool loadFileToData(const std::string& path, LocaleData& dataRef);
        std::expected<std::string, bool> findKey(const std::string& k);

    public:
        void findAll(std::string folderPath);
        std::unordered_map<int, std::string> listAll();
        bool initFallback(std::string codeToFind);
        bool loadLocale(int id);
        std::string getKey(std::string key);
        bool isLtr();
        std::string getCurrentCode();
    };
}
