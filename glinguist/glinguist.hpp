#pragma once
#include <string>
#include <expected>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace gLinguist
{
    struct LocaleMeta {
        int id;
        std::string code;
        std::string name;
        std::string path;
        bool ltr; // left to right
    };

    struct LocaleData
    {
        bool loaded = false;
        bool ltr = true;
        std::unordered_map<std::string, std::string> mapping;
    };

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