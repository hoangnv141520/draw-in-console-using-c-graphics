# Milestone 2 specification: core model and legacy compatibility

**Status:** Implemented and verified on 2026-08-09.

**Branch:** `refactor/core-model-and-legacy-reader`

**Base:** Milestone 1 commit `d5cd077`

## Objective

Create renderer-independent C++17 domain models and a strict reader for the legacy TXT drawing format. A valid legacy file must become one `drawvideo::VectorFrame`; an invalid file must fail with a structured error and expose no partially parsed frame.

## Current evidence

- The original parser is preserved in `legacy/draw/Test Graphics.cpp`.
- The first physical line contains the declared command count.
- Each command line contains a case-sensitive command name followed by eight decimal integer fields.
- The known commands are `Line`, `QuadraticBezier`, and `CubicBezier`.
- The original parser reuses one mutable `Data` object, does not check extraction success, and pushes that object even after malformed input.
- `assets/sample-legacy.txt` is the valid reference sample.
- The new executable currently owns SDL setup directly and there is no project-owned library target.

## Required outcome

After Milestone 2:

1. Domain headers compile without including SDL or OpenCV.
2. Command type is represented by `std::variant`, not by strings stored in the model.
3. `LegacyTxtReader` can read a file or an input stream and return a complete `VectorFrame`.
4. Every parse failure identifies the source and physical line when a line exists.
5. Unsupported commands, malformed integers, wrong field counts, truncated files, oversized command counts, and undeclared trailing commands are rejected.
6. CTest covers all three valid command types and the failure cases defined below.
7. The existing SDL executable still builds and both Milestone 1 tests still pass.

## Explicit non-goals

- Do not render any model or connect the reader to SDL.
- Do not add a CLI command such as `legacy-import`; that belongs to Milestone 7.
- Do not use OpenCV types in the domain model.
- Do not flatten Bézier curves into polylines yet.
- Do not implement video decoding, preprocessing, contour extraction, caching, or playback.
- Do not modify files under `legacy/`.
- Do not add serialization for the new model.
- Do not add global mutable state or call `exit()` in library code.

## Future performance boundary

Milestone 2 does not implement performance infrastructure, but its models must not force later stages into unbounded memory use:

- One `drawvideo` process may use multiple CPU threads in later milestones; separate operating-system processes are not required.
- Video decoding and conversion must operate frame-by-frame through bounded queues rather than filling `VectorVideo::frames` without a known limit.
- `VectorVideo::frames` remains useful for tests, small clips, and explicitly bounded cached data. It is not the default representation for an actively decoded video.
- A future GPU path may accelerate decode, preprocessing, and batched rendering. Contour extraction and path construction remain behind a CPU-compatible boundary unless profiling justifies a different implementation.
- CUDA, compute shaders, worker queues, and GPU renderer abstractions are deliberately excluded from Milestone 2.
- A future 720p60 profile requires explicit frame-time, path-count, point-count, queue-byte, and memory budgets; those budgets belong in the preprocessing/vectorization/renderer specifications.

## Authoritative domain contract

All types live in namespace `drawvideo`. The authoritative contract is the public C++ interface in `include/drawvideo/model.hpp`; no duplicate JSON or schema artifact is needed because every current consumer shares the same C++ build.

```cpp
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
```

`Path` includes `Polyline` now because the contour vectorizer in Milestone 4 produces polylines, while legacy import must preserve exact Line and Bézier semantics. This avoids introducing a second frame model or prematurely approximating curves.

The following supporting models are also part of Milestone 2 because they are named in the repository plan:

```cpp
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
```

Rules for these models:

- Coordinates use signed 32-bit integers because legacy values may be negative and the future DVC format requires fixed-width values.
- Timestamps use signed 64-bit microseconds and legacy import sets the timestamp to `0`.
- Unavailable FPS and frame count are represented by `std::optional`, not sentinel values such as zero or `-1`.
- `ProcessingConfig` contains only the three settings already needed across milestones. Threshold, blur, morphology, and inversion fields are deferred to Milestone 3.
- `VectorVideo` is a value model only in this milestone. No video decoder may eagerly accumulate frames into it; production bounds and streaming behavior remain responsibilities of later milestones.
- No validation framework or inheritance hierarchy is introduced. Producers validate values at their boundary.

## Legacy reader API

The public interface lives in `include/drawvideo/legacy_txt_reader.hpp`.

```cpp
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
};

class LegacyTxtReader {
public:
    static constexpr std::size_t kMaxCommandsPerFrame = 1'000'000;

    static VectorFrame readFile(const std::filesystem::path& path);
    static VectorFrame read(std::istream& input, std::string sourceName);
};
```

Design decisions:

- A typed exception is used because C++17 has no `std::expected`, and adding a custom result abstraction for one reader would be unnecessary.
- `read()` exists so parsing can be tested deterministically in memory. `readFile()` owns file opening and delegates to it.
- Parse errors have a one-based physical line. I/O errors have no line because no input line was read.
- Tests assert `code`, `source`, and `line`; they do not depend on the complete English `what()` text.
- `readFile()` uses the caller-provided path as the source string. Stream callers are responsible for providing a useful source name.
- The reader has no mutable member state and is safe to invoke independently from multiple callers.

## Exact legacy grammar

```text
file          = header, command-block, trailing-blank-lines, EOF
command-block = (newline, command-line){declared-count}
header        = optional-space, unsigned-decimal, optional-space
command-line  = command, space, integer, space, integer, space, integer,
                space, integer, space, integer, space, integer, space,
                integer, space, integer, optional-space
command       = "Line" | "QuadraticBezier" | "CubicBezier"
integer       = ["-"], digit, {digit}
```

Parsing rules:

- The first physical line is always the header; leading blank lines are not skipped.
- Horizontal leading/trailing whitespace is accepted around tokens.
- The header contains exactly one non-negative integer and no second token.
- A zero command count is valid.
- A zero-command file may end immediately after the header without a final newline.
- The declared count must not exceed `kMaxCommandsPerFrame`.
- Each declared command consumes exactly one physical line. A blank command line is invalid rather than skipped.
- Each command line contains exactly nine tokens: one command and eight integers.
- Integers are parsed in base 10 and must fit `std::int32_t`; suffixes such as `12px` and overflow are invalid.
- Command names are case-sensitive and are not normalized.
- Negative coordinates are valid.
- After the declared commands, blank or whitespace-only lines are accepted; any other content is `TrailingData`.
- Validation order for a command line is field count, supported command name, then integers from left to right. The first failure in that order wins.

## Field mapping

Every command line has these eight numeric slots:

```text
x1 y1 control1X control1Y control2X control2Y x2 y2
```

| Command | Model mapping | Compatibility rule |
|---|---|---|
| `Line` | `start=(x1,y1)`, `end=(x2,y2)` | Both control-point pairs are parsed as valid integers and then ignored. They are not required to be zero. |
| `QuadraticBezier` | `start=(x1,y1)`, `control=(control1X,control1Y)`, `end=(x2,y2)` | The second control-point pair is parsed and ignored. It is not required to be zero. |
| `CubicBezier` | All four points map directly. | No numeric slot is ignored. |

Unused placeholder fields remain permissive because the old renderer ignored them. Requiring zero would reject legacy files that were previously renderable without improving type safety.

## Failure contract

| Condition | Error code | Line |
|---|---|---|
| File cannot be opened | `Io` | none |
| Header missing, negative, non-numeric, overflowed, or has extra tokens | `InvalidHeader` | 1 |
| Header exceeds the command limit | `CommandLimitExceeded` | 1 |
| EOF occurs before a declared command line | `UnexpectedEndOfFile` | next expected physical line |
| Command name is unknown | `UnsupportedCommand` | command line |
| Command has fewer or more than eight numeric fields | `InvalidFieldCount` | command line |
| A numeric field is malformed or outside `int32_t` | `InvalidInteger` | command line |
| Non-blank content exists after the declared commands | `TrailingData` | first trailing content line |

Example diagnostic:

```text
assets/broken.txt:3: invalid integer in control1.x: "abc"
```

The exact prose may change, but the structured fields above are stable within Milestone 2.

## Atomicity and ownership

- Parse the header into a local count.
- Parse each line into local tokens and local strongly typed values.
- Construct one complete `Path` locally.
- Append that path only after the command line has fully validated.
- Build the `VectorFrame` locally and return it by value only after the entire file, including trailing-data validation, succeeds.
- On any exception, no `VectorFrame` or partially populated caller-owned collection is observable.
- Do not reserve from the untrusted header before validating the command limit.

## Intended file changes during implementation

```text
vcpkg.json
CMakeLists.txt
include/drawvideo/model.hpp
include/drawvideo/legacy_txt_reader.hpp
src/legacy_txt_reader.cpp
tests/CMakeLists.txt
tests/legacy_txt_reader_tests.cpp
implementation-notes.md
```

Build organization:

- Add a `drawvideo_core` library containing the reader and public domain headers.
- `drawvideo_core` must not link SDL or OpenCV.
- Keep SDL/OpenCV linkage on the `drawvideo` executable.
- Add Catch2 v3 through vcpkg for the test target only.
- Add one `drawvideo_core_tests` executable and register cases with CTest.
- Do not create a `.cpp` file for aggregate-only models.

## Required test matrix

### Valid input

- One file containing `Line`, `QuadraticBezier`, and `CubicBezier`, asserting every mapped point.
- Zero-command file.
- Negative and boundary `int32_t` coordinates.
- Leading/trailing horizontal whitespace and CRLF input.
- Non-zero unused placeholder values for Line and Quadratic, confirming backward-compatible acceptance.
- Legacy import timestamp equals zero.

### Invalid input

- Missing header.
- Negative, malformed, overflowed, and multi-token header.
- Command count over the hard limit.
- Declared count greater than available command lines.
- Blank line inside the declared command block.
- Unknown or wrong-case command.
- Too few numeric fields.
- Too many numeric fields.
- Malformed integer token.
- Positive and negative `int32_t` overflow.
- Extra non-blank command after the declared count.
- Missing file through `readFile()`.

### Error evidence

- Every parse error test asserts the expected error code.
- Representative failures assert source name and one-based physical line.
- The I/O test asserts that line is absent.
- Existing `drawvideo_version` and `drawvideo_sdl_smoke` tests remain green.

## Implementation order

1. Add Catch2 and a failing valid-format test.
2. Add the domain model and `drawvideo_core` target until the valid test compiles.
3. Implement the smallest parser that passes valid Line, Quadratic, and Cubic cases.
4. Add malformed-input tests one error category at a time, then implement the corresponding validation.
5. Add trailing-data, command-limit, and I/O tests.
6. Run the full configure/build/CTest acceptance sequence.
7. Review includes and link dependencies to prove `drawvideo_core` is independent of SDL/OpenCV.
8. Update `implementation-notes.md` with actual deviations and verification results.

## Verification commands

Run from a Visual Studio 2022 Developer PowerShell or Developer Command Prompt on Windows:

```powershell
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Additional focused check after the test target exists:

```powershell
ctest --preset default -R drawvideo_core --output-on-failure
```

## Definition of done

Milestone 2 is complete only when:

- All public model and reader headers are independent of SDL/OpenCV.
- All legacy commands are represented by typed alternatives in `Path`.
- The reference sample imports into one correct `VectorFrame`.
- Every required invalid-input category fails with structured source/line evidence.
- No failed line is appended and no partial frame escapes.
- No production code uses string command tags after parsing.
- All Milestone 1 and Milestone 2 tests pass through CTest.
- Files under `legacy/` retain their current Git blob hashes.
- The implementation stays within the intended file list unless a deviation is recorded and approved.

## Approved decisions

The following choices were approved before implementation:

1. `VectorFrame::paths` is a variant of exact legacy curves and future polylines, instead of converting curves to polylines during import.
2. `VectorVideo` contains a frame vector as a value model, while actual decoding/queue bounds remain deferred; no video loading is implemented in Milestone 2.

The implementation follows both choices without deviation.
