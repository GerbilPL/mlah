#include "gargs.hpp"

#include <cctype>

namespace {
bool isNegativeNumber(const std::string& value)
{
    // Classify token syntax only; option callbacks remain responsible for type and range checks.
    if (value.size() < 2 || value.front() != '-') {
        return false;
    }

    size_t index = 1;
    size_t mantissaDigits = 0;
    while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index]))) {
        ++index;
        ++mantissaDigits;
    }
    if (index < value.size() && value[index] == '.') {
        ++index;
        while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index]))) {
            ++index;
            ++mantissaDigits;
        }
    }
    if (mantissaDigits == 0) {
        return false;
    }

    if (index < value.size() && (value[index] == 'e' || value[index] == 'E')) {
        ++index;
        if (index < value.size() && (value[index] == '+' || value[index] == '-')) {
            ++index;
        }
        const size_t exponentStart = index;
        while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index]))) {
            ++index;
        }
        if (index == exponentStart) {
            return false;
        }
    }

    return index == value.size();
}

bool startsOption(const char* value)
{
    return value[0] == '-' && value[1] != '\0';
}
}

void gArgs::validateOptionName(const std::string& option)
{
    if (option.empty()) {
        throw std::invalid_argument("Option name cannot be empty");
    }
    if (option.rfind("--", 0) != 0) {
        throw std::invalid_argument("Long option must start with '--'");
    }
    if (option.size() < 3) {
        throw std::invalid_argument("Long option must have at least 1 character after '--'");
    }

    for (size_t index = 2; index < option.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(option[index]);
        if (!std::isalnum(character) && character != '-' && character != '_') {
            throw std::invalid_argument(std::string("Invalid character in option: ") + option[index]);
        }
    }
}

void gArgs::processOption(const std::string& arg, size_t& argIndex, int argc,
                          const char* const argv[])
{
    auto invoke = [&](const std::shared_ptr<OptionHandler>& handler,
                      std::vector<std::string> values) {
        if (!handler->takesValue && !values.empty()) {
            throw std::runtime_error("Option " + handler->longName + " does not take a value");
        }

        if (handler->takesValue) {
            const size_t maxValues = handler->maxValues > 0
                ? handler->maxValues
                : static_cast<size_t>(-1);

            while (values.size() < maxValues && static_cast<int>(argIndex) + 1 < argc) {
                const char* nextArg = argv[argIndex + 1];
                if (nextArg == nullptr) {
                    throw std::runtime_error("Null argument encountered");
                }
                if (std::string(nextArg) == "--" ||
                    (startsOption(nextArg) && !isNegativeNumber(nextArg))) {
                    break;
                }

                ++argIndex;
                values.emplace_back(nextArg);
            }

            if (values.size() < handler->minValues) {
                throw std::runtime_error(
                    "Option requires at least " + std::to_string(handler->minValues) + " value(s)");
            }
        }

        handler->callback(values);
    };

    if (arg.rfind("--", 0) == 0) {
        std::string optionName = arg;
        std::vector<std::string> values;
        const size_t equals = optionName.find('=');
        if (equals != std::string::npos) {
            values.push_back(optionName.substr(equals + 1));
            optionName.resize(equals);
        }

        const auto found = longOptions.find(optionName);
        if (found == longOptions.end()) {
            if (unknownOptionPolicy == UnknownOptionPolicy::Positional) {
                positionalArgs.push_back(arg);
                return;
            }
            throw std::runtime_error("Unknown option: " + optionName);
        }
        invoke(found->second, std::move(values));
        return;
    }

    const std::string shortOptionsInArg = arg.substr(1);

    // Validate the group before running callbacks, so an unknown character cannot
    // leave the parser with only part of the group applied.
    size_t position = 0;
    while (position < shortOptionsInArg.size()) {
        const auto found = shortOptions.find(shortOptionsInArg[position]);
        if (found == shortOptions.end()) {
            if (unknownOptionPolicy == UnknownOptionPolicy::Positional) {
                positionalArgs.push_back(arg);
                return;
            }
            throw std::runtime_error(std::string("Unknown option: -") + shortOptionsInArg[position]);
        }
        if (found->second->takesValue) {
            break;
        }
        ++position;
    }

    position = 0;
    while (position < shortOptionsInArg.size()) {
        const auto handler = shortOptions.at(shortOptionsInArg[position]);
        if (handler->takesValue) {
            std::vector<std::string> values;
            if (position + 1 < shortOptionsInArg.size()) {
                std::string attachedValue = shortOptionsInArg.substr(position + 1);
                if (!attachedValue.empty() && attachedValue.front() == '=') {
                    attachedValue.erase(0, 1);
                }
                values.push_back(std::move(attachedValue));
            }
            invoke(handler, std::move(values));
            return;
        }

        invoke(handler, {});
        ++position;
    }
}

void gArgs::parse(int argc, const char* const argv[])
{
    if (argc < 1) {
        throw std::invalid_argument("argc must be at least 1");
    }
    if (argv == nullptr || argv[0] == nullptr) {
        throw std::invalid_argument("argv and argv[0] must not be null");
    }

    programName = argv[0];
    bool positionalOnly = false;

    for (int index = 1; index < argc; ++index) {
        const char* arg = argv[index];
        if (arg == nullptr) {
            throw std::runtime_error("Null argument encountered");
        }

        const std::string argString(arg);
        if (positionalOnly) {
            positionalArgs.push_back(argString);
            continue;
        }
        if (argString == "--") {
            positionalOnly = true;
            continue;
        }

        const bool registeredNumericShortOption = argString.size() == 2 &&
            argString.front() == '-' && shortOptions.contains(argString[1]);
        if (isNegativeNumber(argString) && !registeredNumericShortOption) {
            positionalArgs.push_back(argString);
            continue;
        }

        if (startsOption(arg)) {
            size_t argIndex = static_cast<size_t>(index);
            processOption(argString, argIndex, argc, argv);
            index = static_cast<int>(argIndex);
        } else {
            positionalArgs.push_back(argString);
        }
    }

    parsed = true;
}
