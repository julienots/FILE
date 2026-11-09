#include "agr/core/Assert.hpp"

#include <cstdlib>
#include <format>

#include "agr/core/Log.hpp"

namespace agr::detail {

bool reportAssertion(const char* expr, const char* message, const char* file, int line) {
    Log::write(LogLevel::Error, "Assert", std::format("{} ({}) at {}:{}", message, expr, file, line));
    Log::flush();
    // Break only when explicitly requested, so automated test runs never hang.
    return std::getenv("AGR_BREAK_ON_ASSERT") != nullptr;
}

void fatalError(const char* message, const char* file, int line) {
    Log::write(LogLevel::Fatal, "Core", std::format("{} at {}:{}", message, file, line));
    Log::flush();
    std::abort();
}

} // namespace agr::detail
