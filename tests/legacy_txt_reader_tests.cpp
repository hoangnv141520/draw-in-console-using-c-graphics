#include "drawvideo/legacy_txt_reader.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <variant>

namespace {

using drawvideo::CubicBezier;
using drawvideo::LegacyReadError;
using drawvideo::LegacyReadErrorCode;
using drawvideo::LegacyTxtReader;
using drawvideo::Line;
using drawvideo::QuadraticBezier;

drawvideo::VectorFrame readText(
    const std::string& text,
    const std::string& source = "fixture.txt") {
    std::istringstream input(text);
    return LegacyTxtReader::read(input, source);
}

void requireError(
    const std::string& text,
    LegacyReadErrorCode expectedCode,
    std::optional<std::size_t> expectedLine,
    const std::string& expectedSource = "fixture.txt") {
    std::istringstream input(text);

    try {
        static_cast<void>(LegacyTxtReader::read(input, expectedSource));
        FAIL("Expected LegacyReadError");
    } catch (const LegacyReadError& error) {
        REQUIRE(error.code() == expectedCode);
        REQUIRE(error.source() == expectedSource);
        REQUIRE(error.line() == expectedLine);
    }
}

}  // namespace

TEST_CASE("Legacy reader maps all supported commands") {
    const auto frame = readText(
        "3\n"
        "Line 10 11 0 0 0 0 12 13\n"
        "QuadraticBezier 20 21 22 23 0 0 24 25\n"
        "CubicBezier 30 31 32 33 34 35 36 37\n");

    REQUIRE(frame.timestampMicroseconds == 0);
    REQUIRE(frame.paths.size() == 3);

    const auto& line = std::get<Line>(frame.paths[0]);
    REQUIRE(line.start.x == 10);
    REQUIRE(line.start.y == 11);
    REQUIRE(line.end.x == 12);
    REQUIRE(line.end.y == 13);

    const auto& quadratic = std::get<QuadraticBezier>(frame.paths[1]);
    REQUIRE(quadratic.start.x == 20);
    REQUIRE(quadratic.start.y == 21);
    REQUIRE(quadratic.control.x == 22);
    REQUIRE(quadratic.control.y == 23);
    REQUIRE(quadratic.end.x == 24);
    REQUIRE(quadratic.end.y == 25);

    const auto& cubic = std::get<CubicBezier>(frame.paths[2]);
    REQUIRE(cubic.start.x == 30);
    REQUIRE(cubic.start.y == 31);
    REQUIRE(cubic.control1.x == 32);
    REQUIRE(cubic.control1.y == 33);
    REQUIRE(cubic.control2.x == 34);
    REQUIRE(cubic.control2.y == 35);
    REQUIRE(cubic.end.x == 36);
    REQUIRE(cubic.end.y == 37);
}

TEST_CASE("Legacy reader imports the repository sample") {
    const auto frame = LegacyTxtReader::readFile("assets/sample-legacy.txt");

    REQUIRE(frame.paths.size() == 3);
    REQUIRE(std::holds_alternative<Line>(frame.paths[0]));
    REQUIRE(std::holds_alternative<QuadraticBezier>(frame.paths[1]));
    REQUIRE(std::holds_alternative<CubicBezier>(frame.paths[2]));
}

TEST_CASE("Legacy reader accepts compatible boundary input") {
    SECTION("zero commands without final newline") {
        const auto frame = readText("0");
        REQUIRE(frame.paths.empty());
    }

    SECTION("signed int32 boundaries and CRLF") {
        const auto frame = readText(
            "1\r\n"
            "CubicBezier -2147483648 2147483647 -1 1 -2 2 -3 3\r\n");
        const auto& cubic = std::get<CubicBezier>(frame.paths[0]);
        REQUIRE(cubic.start.x == INT32_MIN);
        REQUIRE(cubic.start.y == INT32_MAX);
    }

    SECTION("unused placeholders may be non-zero") {
        const auto frame = readText(
            "2\n"
            "Line 1 2 90 91 92 93 3 4\n"
            "QuadraticBezier 5 6 7 8 94 95 9 10\n");
        REQUIRE(std::get<Line>(frame.paths[0]).end.x == 3);
        REQUIRE(std::get<QuadraticBezier>(frame.paths[1]).control.x == 7);
    }

    SECTION("surrounding whitespace and trailing blank lines") {
        const auto frame = readText(
            "  1  \n"
            "\tLine 1 2 0 0 0 0 3 4   \n"
            "   \n"
            "\t\n");
        REQUIRE(frame.paths.size() == 1);
        REQUIRE(std::holds_alternative<Line>(frame.paths[0]));
    }
}

TEST_CASE("Legacy reader rejects invalid headers") {
    SECTION("missing header") {
        requireError("", LegacyReadErrorCode::InvalidHeader, 1);
    }
    SECTION("negative count") {
        requireError("-1\n", LegacyReadErrorCode::InvalidHeader, 1);
    }
    SECTION("malformed count") {
        requireError("abc\n", LegacyReadErrorCode::InvalidHeader, 1);
    }
    SECTION("overflowed count") {
        requireError(
            "18446744073709551616\n", LegacyReadErrorCode::InvalidHeader, 1);
    }
    SECTION("extra header token") {
        requireError("1 extra\n", LegacyReadErrorCode::InvalidHeader, 1);
    }
    SECTION("count exceeds safety limit") {
        requireError(
            "1000001\n", LegacyReadErrorCode::CommandLimitExceeded, 1);
    }
}

TEST_CASE("Legacy reader rejects invalid command lines") {
    SECTION("truncated file") {
        requireError(
            "2\nLine 1 2 0 0 0 0 3 4\n",
            LegacyReadErrorCode::UnexpectedEndOfFile,
            3);
    }
    SECTION("blank declared command") {
        requireError("1\n\n", LegacyReadErrorCode::InvalidFieldCount, 2);
    }
    SECTION("unsupported command") {
        requireError(
            "1\nArc 1 2 3 4 5 6 7 8\n",
            LegacyReadErrorCode::UnsupportedCommand,
            2);
    }
    SECTION("wrong-case command") {
        requireError(
            "1\nline 1 2 3 4 5 6 7 8\n",
            LegacyReadErrorCode::UnsupportedCommand,
            2);
    }
    SECTION("too few fields") {
        requireError(
            "1\nLine 1 2 3 4 5 6 7\n",
            LegacyReadErrorCode::InvalidFieldCount,
            2);
    }
    SECTION("too many fields") {
        requireError(
            "1\nLine 1 2 3 4 5 6 7 8 9\n",
            LegacyReadErrorCode::InvalidFieldCount,
            2);
    }
    SECTION("malformed integer") {
        requireError(
            "1\nLine 1 2 bad 4 5 6 7 8\n",
            LegacyReadErrorCode::InvalidInteger,
            2);
    }
    SECTION("positive int32 overflow") {
        requireError(
            "1\nLine 2147483648 2 3 4 5 6 7 8\n",
            LegacyReadErrorCode::InvalidInteger,
            2);
    }
    SECTION("negative int32 overflow") {
        requireError(
            "1\nLine -2147483649 2 3 4 5 6 7 8\n",
            LegacyReadErrorCode::InvalidInteger,
            2);
    }
}

TEST_CASE("Legacy reader rejects undeclared trailing data") {
    requireError(
        "1\nLine 1 2 0 0 0 0 3 4\nCubicBezier 1 2 3 4 5 6 7 8\n",
        LegacyReadErrorCode::TrailingData,
        3);
}

TEST_CASE("Legacy reader reports file-open failures without a line") {
    const std::filesystem::path missingPath =
        "assets/this-file-must-not-exist.drawvideo-test.txt";

    try {
        static_cast<void>(LegacyTxtReader::readFile(missingPath));
        FAIL("Expected LegacyReadError");
    } catch (const LegacyReadError& error) {
        REQUIRE(error.code() == LegacyReadErrorCode::Io);
        REQUIRE(error.source() == missingPath.string());
        REQUIRE_FALSE(error.line().has_value());
    }
}
