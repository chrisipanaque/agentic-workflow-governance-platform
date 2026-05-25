// markdown_generator.h
// Helpers to validate and wrap markdown planner artifacts.

#pragma once

#include <string>

namespace Output {
    // Ensure the LLM output is treated as markdown and apply minimal sanitization.
    std::string sanitize_markdown(const std::string& raw);
}
