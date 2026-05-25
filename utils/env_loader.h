// env_loader.h
// Load environment variables from a .env file into the process environment.

#pragma once

#include <string>

namespace Env {
    bool load_dotenv(const std::string& path);
}
