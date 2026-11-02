#include "gargs.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, const char* argv[])
{
    gArgs args;
    std::string number;
    bool debug = false;

    args.pushOption('d', "--debug", [&debug](const std::vector<std::string>&) {
        debug = true;
    });
    args.pushOption('n', "--number", [&number](const std::vector<std::string>& values) {
        number = values.front();
    }, true, 1, 1);
    args.pushOption('h', "--help", [](const std::vector<std::string>&) {
        std::cout << "Usage: gargs_example [-dh] [-n NUMBER] [FILES...]\n";
    });

    try {
        args.parse(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    std::cout << "debug: " << (debug ? "on" : "off") << '\n';
    if (!number.empty()) {
        std::cout << "number: " << number << '\n';
    }
    for (const auto& path : args.getPositionalArgs()) {
        std::cout << "file: " << path << '\n';
    }
}
