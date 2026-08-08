#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace drawvideo {

struct Point {
    std::int32_t x{};
    std::int32_t y{};
};

struct Line {
    Point start;
    Point end;
};

struct QuadraticBezier {
    Point start;
    Point control;
    Point end;
};

struct CubicBezier {
    Point start;
    Point control1;
    Point control2;
    Point end;
};

struct Polyline {
    bool closed{};
    std::vector<Point> points;
};

using Path = std::variant<Line, QuadraticBezier, CubicBezier, Polyline>;

struct VectorFrame {
    std::int64_t timestampMicroseconds{};
    std::vector<Path> paths;
};

struct VideoMetadata {
    std::int32_t width{};
    std::int32_t height{};
    std::optional<double> nominalFps;
    std::optional<std::int64_t> frameCount;
};

struct ProcessingConfig {
    std::int32_t targetWidth{640};
    std::int32_t targetHeight{480};
    double targetFps{12.0};
};

struct VectorVideo {
    VideoMetadata source;
    ProcessingConfig processing;
    std::vector<VectorFrame> frames;
};

}  // namespace drawvideo
