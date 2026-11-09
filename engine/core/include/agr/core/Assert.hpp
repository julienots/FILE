#pragma once
#include "agr/core/Platform.hpp"

namespace agr::detail {
/// Logs a failed assertion. Returns true when the debugger should break.
bool reportAssertion(const char* expr, const char* message, const char* file, int line);
[[noreturn]] void fatalError(const char* message, const char* file, int line);
} // namespace agr::detail

// AGR_ASSERT: debug-only invariant check (compiled out in release builds).
// AGR_VERIFY: always evaluated, always checked; aborts on failure.
#if defined(AGR_DEBUG)
    #define AGR_ASSERT(expr, msg)                                                       \
        do {                                                                            \
            if (!(expr) && ::agr::detail::reportAssertion(#expr, msg, __FILE__, __LINE__)) \
                AGR_DEBUG_BREAK();                                                      \
        } while (0)
#else
    #define AGR_ASSERT(expr, msg) ((void)0)
#endif

#define AGR_VERIFY(expr, msg)                                                       \
    do {                                                                            \
        if (!(expr)) ::agr::detail::fatalError("Verification failed: " #expr " - " msg, __FILE__, __LINE__); \
    } while (0)

#define AGR_FATAL(msg) ::agr::detail::fatalError(msg, __FILE__, __LINE__)
