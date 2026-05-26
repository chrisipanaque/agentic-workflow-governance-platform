#pragma once

#include <string>
#include <vector>

class ConfigLoader {
public:
    struct ForbiddenPath {
        std::string path;
        std::string reason;
    };

    explicit ConfigLoader(const std::string& config_path);

    void load();
    const std::vector<ForbiddenPath>& get_forbidden_paths() const;

private:
    std::string config_path_;
    std::vector<ForbiddenPath> forbidden_paths_;
};
