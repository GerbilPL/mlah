#include "glogger.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "mlah-glogger-test";
    const auto firstDir = root / "first";
    const auto secondDir = root / "second";
    std::filesystem::remove_all(root);

    {
        gLogger::Logger logger(false);
        assert(logger.init(firstDir.string()));
        logger.log(gLogger::LogLevel::INFO, "first destination");

        assert(logger.init(secondDir.string()));
        logger.log(gLogger::LogLevel::INFO, "second destination");
    }

    std::ifstream firstFile(firstDir / "latest.log");
    std::ifstream secondFile(secondDir / "latest.log");
    const std::string firstContent((std::istreambuf_iterator<char>(firstFile)), {});
    const std::string secondContent((std::istreambuf_iterator<char>(secondFile)), {});

    assert(firstContent.find("first destination") != std::string::npos);
    assert(firstContent.find("second destination") == std::string::npos);
    assert(secondContent.find("second destination") != std::string::npos);

    std::filesystem::remove_all(root);
}
