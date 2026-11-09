#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "agr/core/Log.hpp"

int main(int argc, char** argv) {
    // Keep test output readable: only warnings and errors from the engine.
    agr::Log::setLevel(agr::LogLevel::Warning);
    doctest::Context context(argc, argv);
    return context.run();
}
