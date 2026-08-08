#pragma once

#include "drawvideo/model.hpp"

#include <cstddef>
#include <filesystem>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>

namespace drawvideo {

enum class LegacyReadErrorCode {
    Io,
    InvalidHeader,
    CommandLimitExceeded,
    UnexpectedEndOfFile,
    UnsupportedCommand,
    InvalidFieldCount,
    InvalidInteger,
    TrailingData,
};

class LegacyReadError : public std::runtime_error {
public:
    LegacyReadError(
        LegacyReadErrorCode code,
        std::string source,
        std::optional<std::size_t> line,
        std::string detail);

    LegacyReadErrorCode code() const noexcept;
    const std::string& source() const noexcept;
    std::optional<std::size_t> line() const noexcept;

private:
    LegacyReadErrorCode code_;
    std::string source_;
    std::optional<std::size_t> line_;
};

class LegacyTxtReader {
public:
    static constexpr std::size_t kMaxCommandsPerFrame = 1'000'000;

    static VectorFrame readFile(const std::filesystem::path& path);
    static VectorFrame read(std::istream& input, std::string sourceName);
};

}  // namespace drawvideo
