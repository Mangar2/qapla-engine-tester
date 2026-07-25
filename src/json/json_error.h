#pragma once

/**
 * @file json_error.h
 * @brief Error model for JSON parsing and access failures.
 */

#include <cstddef>
#include <stdexcept>
#include <string>

namespace mqtt::json {

/**
 * @brief Categorizes JSON processing failures.
 */
enum class JsonError : unsigned char {
    UnexpectedEndOfInput,  ///< Input ended before parser could finish.
    UnexpectedToken,       ///< Encountered an unexpected token.
    InvalidNumber,         ///< Number token is malformed.
    InvalidStringEscape,   ///< String escape sequence is invalid.
    InvalidUnicodeEscape,  ///< Unicode escape sequence is invalid.
    InvalidType,           ///< Operation requested incompatible value type.
    MissingKey,            ///< Requested object key does not exist.
    IndexOutOfRange        ///< Requested array index is outside array bounds.
};

/**
 * @brief Exception type for JSON parse, access, and serialization failures.
 */
class JsonException final : public std::runtime_error {
public:
    /**
     * @brief Constructs a JSON exception.
     * @param errorCode Error category.
     * @param messageText Human-readable error description.
     * @param offsetValue Character offset where the error occurred.
     */
    JsonException(JsonError errorCode, const std::string& messageText, std::size_t offsetValue);

    /**
     * @brief Returns the categorized error code.
     * @return Error category.
     */
    [[nodiscard]] JsonError error() const noexcept;

    /**
     * @brief Returns the source offset where error was detected.
     * @return Character offset in source text.
     */
    [[nodiscard]] std::size_t offset() const noexcept;

private:
    JsonError errorCode_;
    std::size_t offsetValue_;
};

} // namespace mqtt::json
