#include <iostream>
#include <string_view>
#include "config_loader.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ai-control-plane <command>\n";
        return 1;
    }

    std::string_view command{argv[1]};

    if (command == "healthcheck") {
        std::cout << "AI control plane operational\n";
        return 0;
    }

    if (command == "show-policies") {
        try {
            ConfigLoader loader("config/forbidden-paths.json");
            loader.load();

            const auto& paths = loader.get_forbidden_paths();
            std::cout << "Forbidden paths: " << paths.size() << '\n';
            for (const auto& policy : paths) {
                std::cout << "  - " << policy.path << " (" << policy.reason << ")\n";
            }
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            return 1;
        }
    }

    std::cerr << "Unknown command: " << command << '\n';
    return 1;
}
