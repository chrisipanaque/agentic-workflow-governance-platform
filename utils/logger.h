// logger.h
// Tiny logging helpers for the CLI workflow. Print simple stage-prefixed logs.

#pragma once

#include <string>
#include <iostream>

namespace Logger {
    inline void info(const std::string& s) { std::cout << s << std::endl; }
    inline void error(const std::string& s) { std::cerr << s << std::endl; }
}
