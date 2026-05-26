#include <iostream>
#include <string_view>
#include "config_loader.hpp"
#include "diff_scanner.hpp"
#include "path_validator.hpp"

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

    if (command == "scan-diff") {
        try {
            DiffScanner scanner;
            auto stats = scanner.scan();
            scanner.print_stats(stats);
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            return 1;
        }
    }

    if (command == "validate-policy") {
        try {
            ConfigLoader config_loader("config/forbidden-paths.json");
            config_loader.load();
            const auto& policies = config_loader.get_forbidden_paths();

            DiffScanner diff_scanner;
            auto diff_stats = diff_scanner.scan();

            PathValidator validator(policies);
            auto validation = validator.validate_diff(diff_stats);
            validator.print_validation_result(validation);

            return validation.is_valid ? 0 : 1;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            return 1;
        }
    }

    std::cerr << "Unknown command: " << command << '\n';
    return 1;
}
