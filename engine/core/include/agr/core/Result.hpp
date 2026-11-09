#pragma once
#include <string>
#include <utility>
#include <variant>

#include "agr/core/Assert.hpp"

namespace agr {

/// Error carried by Result<T>. `code` is a stable machine-readable category.
struct Error {
    enum class Code { Unknown, NotFound, InvalidArgument, IOError, ParseError, Unsupported, AlreadyExists, VersionMismatch };
    Code code = Code::Unknown;
    std::string message;
};

inline Error makeError(Error::Code code, std::string message) { return Error{code, std::move(message)}; }

/// Minimal C++20 replacement for std::expected<T, Error>.
template <typename T>
class [[nodiscard]] Result {
public:
    Result(T value) : m_data(std::in_place_index<0>, std::move(value)) {}
    Result(Error error) : m_data(std::in_place_index<1>, std::move(error)) {}

    bool ok() const { return m_data.index() == 0; }
    explicit operator bool() const { return ok(); }

    T& value() & { AGR_VERIFY(ok(), "Result::value() on error"); return std::get<0>(m_data); }
    const T& value() const& { AGR_VERIFY(ok(), "Result::value() on error"); return std::get<0>(m_data); }
    T&& value() && { AGR_VERIFY(ok(), "Result::value() on error"); return std::get<0>(std::move(m_data)); }
    const Error& error() const { AGR_VERIFY(!ok(), "Result::error() on success"); return std::get<1>(m_data); }

    T valueOr(T fallback) const { return ok() ? std::get<0>(m_data) : std::move(fallback); }

private:
    std::variant<T, Error> m_data;
};

template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(Error error) : m_error(std::move(error)), m_ok(false) {}
    bool ok() const { return m_ok; }
    explicit operator bool() const { return m_ok; }
    const Error& error() const { AGR_VERIFY(!m_ok, "Result::error() on success"); return m_error; }

private:
    Error m_error;
    bool m_ok = true;
};

using Status = Result<void>;

} // namespace agr
