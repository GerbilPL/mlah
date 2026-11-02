#include "gargs.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

int main()
{
    gArgs flagParser;
    bool flagCalled = false;
    flagParser.pushOption("--verbose", [&flagCalled](const std::vector<std::string>& values) {
        flagCalled = true;
        assert(values.empty());
    });

    const char* flagArgv[] = {"app", "--verbose=true"};
    bool rejectedFlagValue = false;
    try {
        flagParser.parse(2, flagArgv);
    } catch (const std::runtime_error&) {
        rejectedFlagValue = true;
    }
    assert(rejectedFlagValue);
    assert(!flagCalled);

    gArgs valueParser;
    std::vector<std::string> receivedValues;
    valueParser.pushOption("--output", [&receivedValues](const std::vector<std::string>& values) {
        receivedValues = values;
    }, true, 1, 1);

    const char* valueArgv[] = {"app", "--output="};
    valueParser.parse(2, valueArgv);
    assert(receivedValues.size() == 1);
    assert(receivedValues.front().empty());

    gArgs positionalParser;
    bool debugEnabled = false;
    positionalParser.pushOption("--debug", [&debugEnabled](const std::vector<std::string>& values) {
        debugEnabled = true;
        assert(values.empty());
    });

    const char* positionalArgv[] = {"app", "--debug", "first.txt", "second.txt"};
    positionalParser.parse(4, positionalArgv);
    assert(debugEnabled);
    assert((positionalParser.getPositionalArgs() == std::vector<std::string>{"first.txt", "second.txt"}));

    gArgs groupedParser;
    int groupedCallbacks = 0;
    groupedParser.pushOption('a', "--alpha", [&groupedCallbacks](const auto&) { ++groupedCallbacks; });
    groupedParser.pushOption('b', "--beta", [&groupedCallbacks](const auto&) { ++groupedCallbacks; });
    groupedParser.pushOption('n', "--number", [](const auto& values) {
        assert(values == std::vector<std::string>{"-5"});
    }, true, 1, 1);

    const char* groupedArgv[] = {"app", "-ab", "--number", "-5", "input.txt"};
    groupedParser.parse(5, groupedArgv);
    assert(groupedCallbacks == 2);
    assert((groupedParser.getPositionalArgs() == std::vector<std::string>{"input.txt"}));

    gArgs largeNumberParser;
    largeNumberParser.pushOption('n', "--number", [](const auto& values) {
        assert(values == std::vector<std::string>{"-1e999"});
    }, true, 1, 1);
    const char* largeNumberArgv[] = {"app", "--number", "-1e999"};
    largeNumberParser.parse(3, largeNumberArgv);

    gArgs passThroughParser(gArgs::UnknownOptionPolicy::Positional);
    bool partialGroupRan = false;
    passThroughParser.pushOption('d', "--debug", [&partialGroupRan](const auto&) {
        partialGroupRan = true;
    });
    const char* passThroughArgv[] = {"app", "--external=yes", "-dX", "--", "-filename"};
    passThroughParser.parse(5, passThroughArgv);
    assert(!partialGroupRan);
    assert((passThroughParser.getPositionalArgs() ==
            std::vector<std::string>{"--external=yes", "-dX", "-filename"}));
}
