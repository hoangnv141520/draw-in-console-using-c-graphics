#include "drawvideo/legacy_txt_reader.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace drawvideo {
namespace {

constexpr std::array<const char*, 8> kFieldNames{
    "x1",
    "y1",
    "control1.x",
    "control1.y",
    "control2.x",
    "control2.y",
    "x2",
    "y2",
};

std::string formatErrorMessage(
    const std::string& source,
    std::optional<std::size_t> line,
    const std::string& detail) {
    std::ostringstream message;
    message << source;
    if (line.has_value()) {
        message << ':' << *line;
    }
    message << ": " << detail;
    return message.str();
}

[[noreturn]] void throwReadError(
    LegacyReadErrorCode code,
    const std::string& source,
    std::optional<std::size_t> line,
    const std::string& detail) {
    throw LegacyReadError(code, source, line, detail);
}

std::size_t parseCommandCount(
    const std::string& header,
    const std::string& source) {
    std::istringstream tokens(header);
    std::string countToken;
    std::string extraToken;

    if (!(tokens >> countToken) || (tokens >> extraToken)) {
        throwReadError(
            LegacyReadErrorCode::InvalidHeader,
            source,
            1,
            "expected one non-negative command count");
    }

    std::uint64_t count{};
    const auto result = std::from_chars(
        countToken.data(), countToken.data() + countToken.size(), count, 10);
    if (result.ec != std::errc{} ||
        result.ptr != countToken.data() + countToken.size()) {
        throwReadError(
            LegacyReadErrorCode::InvalidHeader,
            source,
            1,
            "invalid command count: \"" + countToken + '"');
    }

    if (count > LegacyTxtReader::kMaxCommandsPerFrame) {
        throwReadError(
            LegacyReadErrorCode::CommandLimitExceeded,
            source,
            1,
            "command count exceeds safety limit");
    }

    return static_cast<std::size_t>(count);
}

std::int32_t parseInteger(
    const std::string& token,
    const char* fieldName,
    const std::string& source,
    std::size_t line) {
    std::int32_t value{};
    const auto result = std::from_chars(
        token.data(), token.data() + token.size(), value, 10);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) {
        throwReadError(
            LegacyReadErrorCode::InvalidInteger,
            source,
            line,
            std::string("invalid integer in ") + fieldName + ": \"" + token +
                '"');
    }
    return value;
}

Path parseCommand(
    const std::string& commandLine,
    const std::string& source,
    std::size_t line) {
    std::istringstream input(commandLine);
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) {
        tokens.push_back(std::move(token));
    }

    if (tokens.size() != 9) {
        throwReadError(
            LegacyReadErrorCode::InvalidFieldCount,
            source,
            line,
            "expected one command and eight integer fields");
    }

    const std::string& command = tokens[0];
    if (command != "Line" && command != "QuadraticBezier" &&
        command != "CubicBezier") {
        throwReadError(
            LegacyReadErrorCode::UnsupportedCommand,
            source,
            line,
            "unsupported command: \"" + command + '"');
    }

    std::array<std::int32_t, 8> values{};
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] =
            parseInteger(tokens[index + 1], kFieldNames[index], source, line);
    }

    const Point start{values[0], values[1]};
    const Point end{values[6], values[7]};
    if (command == "Line") {
        return Line{start, end};
    }
    if (command == "QuadraticBezier") {
        return QuadraticBezier{start, Point{values[2], values[3]}, end};
    }
    return CubicBezier{
        start,
        Point{values[2], values[3]},
        Point{values[4], values[5]},
        end,
    };
}

bool isBlank(const std::string& line) {
    return std::all_of(line.begin(), line.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

}  // namespace

LegacyReadError::LegacyReadError(
    LegacyReadErrorCode code,
    std::string source,
    std::optional<std::size_t> line,
    std::string detail)
    : std::runtime_error(formatErrorMessage(source, line, detail)),
      code_(code),
      source_(std::move(source)),
      line_(line) {}

LegacyReadErrorCode LegacyReadError::code() const noexcept {
    return code_;
}

const std::string& LegacyReadError::source() const noexcept {
    return source_;
}

std::optional<std::size_t> LegacyReadError::line() const noexcept {
    return line_;
}

VectorFrame LegacyTxtReader::readFile(const std::filesystem::path& path) {
    const std::string source = path.string();
    std::ifstream input(path);
    if (!input.is_open()) {
        throwReadError(
            LegacyReadErrorCode::Io,
            source,
            std::nullopt,
            "unable to open file");
    }
    return read(input, source);
}

VectorFrame LegacyTxtReader::read(
    std::istream& input,
    std::string sourceName) {
    std::string line;
    if (!std::getline(input, line)) {
        if (input.bad()) {
            throwReadError(
                LegacyReadErrorCode::Io,
                sourceName,
                1,
                "unable to read header");
        }
        throwReadError(
            LegacyReadErrorCode::InvalidHeader,
            sourceName,
            1,
            "missing command count");
    }

    const std::size_t commandCount = parseCommandCount(line, sourceName);
    VectorFrame frame;
    frame.paths.reserve(commandCount);

    for (std::size_t index = 0; index < commandCount; ++index) {
        const std::size_t lineNumber = index + 2;
        if (!std::getline(input, line)) {
            if (input.bad()) {
                throwReadError(
                    LegacyReadErrorCode::Io,
                    sourceName,
                    lineNumber,
                    "unable to read command");
            }
            throwReadError(
                LegacyReadErrorCode::UnexpectedEndOfFile,
                sourceName,
                lineNumber,
                "expected another command line");
        }

        Path path = parseCommand(line, sourceName, lineNumber);
        frame.paths.push_back(std::move(path));
    }

    std::size_t lineNumber = commandCount + 2;
    while (std::getline(input, line)) {
        if (!isBlank(line)) {
            throwReadError(
                LegacyReadErrorCode::TrailingData,
                sourceName,
                lineNumber,
                "unexpected content after declared commands");
        }
        ++lineNumber;
    }

    if (input.bad()) {
        throwReadError(
            LegacyReadErrorCode::Io,
            sourceName,
            lineNumber,
            "unable to read trailing data");
    }

    return frame;
}

}  // namespace drawvideo
