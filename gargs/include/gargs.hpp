#ifndef GARGS_HPP
#define GARGS_HPP

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/**
 * @class gArgs
 * @brief Command-line argument parser with callback-based options.
 *
 * Short flags can be grouped (for example, `-abc`). Use `--` to stop option
 * parsing. Negative decimal tokens are accepted as values and positionals;
 * callbacks are responsible for validating application-specific types/ranges.
 * Value-taking options with `maxValues == 0` consume following non-option
 * arguments without a limit.
 */
class gArgs {
public:
    /** @brief Receives the raw string values collected for a registered option. */
    using OptionCallback = std::function<void(const std::vector<std::string>&)>;

    /** @brief Controls how unregistered option tokens are handled. */
    enum class UnknownOptionPolicy {
        /** Throw an exception when an unregistered option is encountered. */
        Error,
        /** Preserve the unknown token as a positional argument. */
        Positional
    };

private:
    struct OptionHandler {
        std::string longName;
        std::optional<char> shortName;
        OptionCallback callback;
        bool takesValue;
        size_t minValues;
        size_t maxValues;
    };

    std::map<std::string, std::shared_ptr<OptionHandler>> longOptions;
    std::map<char, std::shared_ptr<OptionHandler>> shortOptions;
    std::vector<std::string> positionalArgs;
    std::string programName;
    UnknownOptionPolicy unknownOptionPolicy;
    bool parsed = false;

public:
    /**
     * @brief Create a parser with the selected unknown-option policy.
     * @param policy Defaults to `Error`.
     */
    explicit gArgs(UnknownOptionPolicy policy = UnknownOptionPolicy::Error)
        : unknownOptionPolicy(policy) {}
    ~gArgs() = default;

    template<typename Callable>
    void pushOption(const std::string& longOption, Callable&& callback,
                    bool takesValue = false, size_t minValues = 0,
                    size_t maxValues = 0)
    {
        validateOptionName(longOption);

        auto handler = std::make_shared<OptionHandler>();
        handler->longName = longOption;
        handler->shortName = std::nullopt;
        handler->callback = std::forward<Callable>(callback);
        handler->takesValue = takesValue;
        handler->minValues = minValues;
        handler->maxValues = maxValues;

        longOptions[longOption] = handler;
    }

    template<typename Callable>
    void pushOption(char shortOpt, const std::string& longOpt, Callable&& callback,
                    bool takesValue = false, size_t minValues = 0,
                    size_t maxValues = 0)
    {
        validateOptionName(longOpt);

        if (shortOpt == '-' || shortOpt == '\0') {
            throw std::invalid_argument("Invalid short option character");
        }

        auto handler = std::make_shared<OptionHandler>();
        handler->longName = longOpt;
        handler->shortName = shortOpt;
        handler->callback = std::forward<Callable>(callback);
        handler->takesValue = takesValue;
        handler->minValues = minValues;
        handler->maxValues = maxValues;

        longOptions[longOpt] = handler;
        shortOptions[shortOpt] = handler;
    }

    void parse(int argc, const char* const argv[]);

    [[nodiscard]] const std::vector<std::string>& getPositionalArgs() const {
        return positionalArgs;
    }

    [[nodiscard]] const std::string& getProgramName() const {
        return programName;
    }

    [[nodiscard]] bool isParsed() const {
        return parsed;
    }

private:
    void validateOptionName(const std::string& option);
    void processOption(const std::string& arg, size_t& argIndex, int argc,
                       const char* const argv[]);
};

#endif
