## Kiến trúc mới được đề xuất

Input video trực tiếp **hoàn toàn khả thi** và nên trở thành workflow chính.

Workflow hiện tại:

```text
Video
  → tự tách thành ảnh
  → chuyển ảnh đen trắng
  → chuyển từng ảnh thành SVG
  → chuyển SVG thành TXT
  → C++ đọc từng TXT
  → render
```

Repo hiện đúng là đang chia pipeline thành các bước rời rạc: `main.py` xử lý ảnh, `export.py` chuyển SVG thành các lệnh Bézier/Line, còn C++ đọc file `bruh.txt` từ đường dẫn cố định.

Workflow mới:

```text
                      ┌── Raster preview
Video ──► Decoder ──► Preprocessor ──► Vectorizer ──► Frame model ──► Renderer
                      │                    │
                      │                    └── Polyline / Bézier
                      │
                      └── Grayscale / Threshold / Canny

                                      └── Automatic vector cache
```

OpenCV có sẵn `VideoCapture` để đọc trực tiếp video và image sequence; module Video I/O hỗ trợ nhiều backend tùy hệ điều hành. Các thông số như FPS hoặc frame count có thể phụ thuộc backend, vì vậy code cần có fallback thay vì tin tuyệt đối vào metadata. ([OpenCV Documentation][1])

## Quyết định kỹ thuật quan trọng

### 1. Bỏ SVG khỏi luồng chạy chính

Không nên tiếp tục tạo hàng nghìn SVG trung gian.

Thay vào đó:

```text
Video frame
  → grayscale
  → resize
  → threshold hoặc Canny
  → findContours
  → approxPolyDP
  → danh sách polyline
```

`findContours` có thể trích đường biên từ ảnh nhị phân, còn `approxPolyDP` giảm số điểm của đường contour theo một sai số cho phép. Đây là cách phù hợp để tạo dữ liệu vector nhẹ hơn mà không phải vòng qua SVG. ([OpenCV Documentation][2])

SVG vẫn có thể được giữ cho:

* import hình vector tĩnh;
* export để debug;
* so sánh kết quả;
* tương thích với workflow cũ.

Nhưng SVG không còn là dependency bắt buộc để phát video.

---

### 2. Chưa fit Bézier ngay ở phiên bản đầu

Phiên bản đầu tiên nên lưu các contour dưới dạng polyline:

```cpp
struct Point {
    std::int16_t x;
    std::int16_t y;
};

struct Polyline {
    bool closed;
    std::vector<Point> points;
};

struct VectorFrame {
    std::int64_t timestampMicroseconds;
    std::vector<Polyline> paths;
};
```

Lý do:

* đơn giản hơn Bézier;
* ít lỗi hơn;
* dễ kiểm tra trực quan;
* render nhanh;
* phù hợp trực tiếp với output của `findContours`;
* dễ thêm Bézier fitting sau này.

Khi pipeline đã ổn định, mới thêm thuật toán:

```text
Contour → simplify → split segments → fit cubic Bézier
```

---

### 3. Dùng SDL3 thay WinBGIm

WinBGIm nên được loại bỏ khỏi renderer chính. SDL3 hiện đã phát hành chính thức, hỗ trợ đa nền tảng và có renderer 2D dành cho point, line, polygon và texture. ([Wiki SDL][3])

Stack đề xuất:

```text
C++17
CMake
vcpkg
OpenCV
SDL3
Catch2 hoặc GoogleTest
```

OpenCV phụ trách:

* đọc video;
* resize;
* grayscale;
* blur;
* threshold;
* Canny;
* morphology;
* contour extraction.

SDL3 phụ trách:

* tạo window;
* render vector;
* event loop;
* pause/play;
* keyboard input;
* scaling;
* fullscreen.

---

## Trải nghiệm chạy cuối cùng

Người dùng chỉ cần:

```bash
drawvideo play input.mp4
```

Chương trình tự động:

1. mở video;
2. đọc thông số;
3. tạo cache nếu chưa có;
4. xử lý từng frame;
5. vector hóa;
6. phát kết quả.

Các lệnh dự kiến:

```bash
# Tự xử lý và phát
drawvideo play video.mp4

# Chuyển trước sang vector cache
drawvideo convert video.mp4 --output video.dvc

# Phát cache đã tạo
drawvideo play video.dvc

# Preview nhanh, không vector hóa
drawvideo play video.mp4 --mode raster

# Chế độ vector
drawvideo play video.mp4 --mode vector

# Giảm FPS để nhẹ hơn
drawvideo play video.mp4 --fps 12

# Tùy chỉnh xử lý ảnh
drawvideo play video.mp4 \
  --width 640 \
  --threshold otsu \
  --blur 3 \
  --epsilon 1.5 \
  --min-path-length 20

# Không sử dụng cache
drawvideo play video.mp4 --cache off
```

## Hai chế độ xử lý

### Raster mode

```text
Video → grayscale/threshold → SDL texture
```

Mục đích:

* preview nhanh;
* kiểm tra threshold;
* xác định video có phù hợp hay không;
* không tạo vector.

### Vector mode

```text
Video → threshold/edge → contours → polyline → SDL lines
```

Mục đích:

* giữ đúng tinh thần project;
* tạo hiệu ứng vẽ bằng đường;
* cache được;
* có thể nâng cấp sang Bézier.

Mặc định nên là:

```text
--mode vector
```

nhưng chương trình nên cho preview bằng raster trước khi chạy conversion dài.

---

# Định dạng cache mới

Không nên tiếp tục tạo một file TXT cho mỗi frame.

Đề xuất một file:

```text
video-name.dvc
```

`DVC` có thể hiểu là **Draw Video Cache**.

Cấu trúc logic:

```text
Header
├── magic: DVC1
├── format version
├── canvas width
├── canvas height
├── source FPS
├── output FPS
├── frame count
├── processing configuration
└── source/config hash

Frames
├── timestamp
├── path count
└── paths
    ├── closed
    ├── point count
    └── points
```

Phiên bản đầu có thể dùng binary không nén. Sau đó mới thêm:

* delta encoding tọa độ;
* frame diff;
* zstd compression;
* key frames;
* metadata JSON.

Nên hỗ trợ thêm JSON debug:

```bash
drawvideo inspect video.dvc --json debug.json
```

Binary dùng để chạy nhanh; JSON chỉ dùng để đọc và debug.

## Cache tự động

Cache key cần phụ thuộc vào:

```text
source video hash
target FPS
target width/height
threshold mode
threshold values
blur size
epsilon
minimum contour length
invert setting
format version
```

Khi đổi bất kỳ tham số nào, cache phải được tạo lại.

```text
.cache/
└── 12ab34cd56ef.dvc
```

Không commit thư mục `.cache` vào Git.

---

# Cấu trúc repo mục tiêu

```text
draw-in-console-using-c-graphics/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── README.md
├── LICENSE
├── AGENTS.md
├── CONTRIBUTING.md
├── .gitignore
│
├── apps/
│   └── drawvideo/
│       └── main.cpp
│
├── include/
│   └── drawvideo/
│       ├── model.hpp
│       ├── video_source.hpp
│       ├── preprocessor.hpp
│       ├── vectorizer.hpp
│       ├── cache.hpp
│       ├── renderer.hpp
│       └── playback.hpp
│
├── src/
│   ├── video_source.cpp
│   ├── preprocessor.cpp
│   ├── vectorizer.cpp
│   ├── cache.cpp
│   ├── sdl_renderer.cpp
│   ├── playback.cpp
│   └── legacy_txt_reader.cpp
│
├── tests/
│   ├── preprocessor_tests.cpp
│   ├── vectorizer_tests.cpp
│   ├── cache_tests.cpp
│   └── fixtures/
│
├── assets/
│   ├── sample.mp4
│   ├── sample.svg
│   └── expected/
│
├── tools/
│   ├── legacy_svg_to_txt.py
│   └── visualize_cache.py
│
├── docs/
│   ├── architecture.md
│   ├── video-pipeline.md
│   ├── cache-format.md
│   ├── legacy-workflow.md
│   ├── decisions/
│   │   ├── 0001-use-opencv.md
│   │   ├── 0002-use-sdl3.md
│   │   ├── 0003-bypass-svg.md
│   │   └── 0004-dvc-cache.md
│   └── tasks/
│       ├── 01-project-foundation.md
│       ├── 02-core-model.md
│       ├── 03-video-input.md
│       ├── 04-vectorization.md
│       ├── 05-cache.md
│       ├── 06-renderer.md
│       └── 07-release-quality.md
│
└── .github/
    └── workflows/
        └── ci.yml
```

`vcpkg.json` cho phép project khai báo dependency theo manifest, trong khi `CMakePresets.json` giúp chia sẻ cấu hình build chung giữa developer, IDE và CI. ([Microsoft Learn][4])

# Kế hoạch triển khai ưu tiên

## Milestone 1 — Project foundation

**Mức ưu tiên:** P0

### Công việc

* Đổi từ Dev-C++ project sang CMake.
* Thêm `vcpkg.json`.
* Thêm SDL3 và OpenCV.
* Tạo cấu trúc `include/`, `src/`, `apps/`, `tests/`.
* Thêm `.gitignore`.
* Thêm README tối thiểu.
* Thêm sample input nhỏ.
* Giữ code cũ trong `legacy/` để tham khảo.
* Xóa toàn bộ absolute path khỏi code mới.
* Tạo một executable mở được SDL window.

### Kết quả chấp nhận

```bashca
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Các lệnh chạy thành công trên máy mới mà không cần Dev-C++.

### Context pack cho coding agent

```text
Repository:
hoangnv141520/draw-in-console-using-c-graphics

Current state:
The repository contains one WinBGIm C++ renderer and two Python scripts.
The current code uses absolute Windows paths, a Dev-C++ .dev file and no
reproducible dependency setup.

Objective:
Modernize only the build and project structure. Create a CMake-based C++17
project using SDL3 and OpenCV through vcpkg manifest mode.

Required:
- Preserve all legacy source under legacy/.
- Create one minimal drawvideo executable.
- The executable must initialize SDL3, open a window and exit cleanly.
- Add CMakePresets.json and vcpkg.json.
- Add build instructions to README.md.
- Add .gitignore for build, cache, IDE and generated files.
- Use no absolute paths.

Do not:
- Implement video conversion yet.
- Rewrite the Bézier renderer yet.
- Delete the old source.
- Introduce global mutable state.

Verification:
- Configure, build and run tests using documented commands.
- Run the executable and verify SDL initializes and shuts down without leaks.
```

---

## Milestone 2 — Core model và legacy compatibility

**Mức ưu tiên:** P0

### Công việc

Tạo model độc lập với OpenCV và SDL:

```cpp
Point
Polyline
VectorFrame
VideoMetadata
ProcessingConfig
VectorVideo
```

### Kết quả chấp nhận

* Parser báo lỗi kèm số dòng.
* Không dùng `string type` rải rác.
* Không push dữ liệu chưa parse thành công.
* Test được Line, Quadratic và Cubic.

### Context pack

```text
Objective:
Create renderer-independent domain models and a safe legacy TXT reader.

Context:
The old format starts with a command count. Each following line contains
a command type and eight integer coordinate fields. The old parser pushes
records even when parsing fails and reuses one mutable Data instance.

Required:
- Define strongly typed Point, Line, QuadraticBezier, CubicBezier and VectorFrame.
- Use std::variant or an equivalent type-safe representation.
- Add a LegacyTxtReader that validates every field.
- Include line number and filename in parse errors.
- Reject unsupported command types.
- Add unit tests for valid, truncated, malformed and unknown input.
- Keep domain headers independent from SDL and OpenCV.

Do not:
- Add rendering calls to the parser.
- Call exit() inside library code.
- Silently skip malformed commands.
```

---

## Milestone 3 — Video input và preprocessing

**Mức ưu tiên:** P0

### Công việc

Tạo:

```text
VideoSource
FrameSampler
FramePreprocessor
```

Pipeline:

```text
decode
→ sample theo target FPS
→ preserve aspect ratio
→ grayscale
→ optional blur
→ threshold/Canny
→ optional morphology
```

OpenCV hỗ trợ threshold thường, adaptive threshold và Otsu; nên expose các chế độ này qua config thay vì hard-code một phương pháp. ([OpenCV Documentation][5])

### Cấu hình ban đầu

```yaml
target_fps: 12
width: 640
height: 480
preprocess:
  mode: otsu
  blur_kernel: 3
  invert: false
  morphology: none
```

### Kết quả chấp nhận

```bash
drawvideo preview sample.mp4
```

Hiển thị được frame gốc và frame sau xử lý.

Không tạo PNG, SVG hoặc TXT trên ổ đĩa.

### Context pack

```text
Objective:
Read a video directly and produce normalized binary frames in memory.

Required:
- Implement VideoSource using cv::VideoCapture.
- Validate that the source opens successfully.
- Read width, height, nominal FPS and frame count when available.
- Do not rely on metadata being non-zero or exact.
- Support a target output FPS.
- Preserve aspect ratio using letterboxing.
- Support grayscale, Gaussian blur, Otsu, adaptive threshold and Canny.
- Keep preprocessing parameters in ProcessingConfig.
- Return structured errors rather than terminating the process.
- Add tests using small generated image fixtures.

Non-goals:
- No contour extraction.
- No SDL vector drawing.
- No cache format.
- No per-frame files.
```

---

## Milestone 4 — Vectorization

**Mức ưu tiên:** P0

### Công việc

Tạo:

```text
Binary frame
→ findContours
→ filter
→ simplify
→ normalize coordinates
→ VectorFrame
```

Các tham số:

```text
contour retrieval mode
minimum path length
minimum area
approximation epsilon
closed/open path handling
maximum paths per frame
maximum points per frame
```

### Cách xử lý noise

* bỏ contour quá ngắn;
* bỏ contour quá nhỏ;
* giới hạn số contour;
* ưu tiên contour dài hơn;
* simplify trước khi lưu;
* có budget điểm cho mỗi frame.

### Kết quả chấp nhận

```bash
drawvideo convert sample.mp4 --output sample.dvc
```

Log ví dụ:

```text
Frame 120/300
Raw contours: 482
Accepted paths: 74
Raw points: 38,241
Simplified points: 3,912
Processing time: 14 ms
```

### Context pack

```text
Objective:
Convert binary OpenCV frames into renderer-independent polylines.

Required:
- Use cv::findContours.
- Simplify accepted contours with cv::approxPolyDP.
- Filter by contour length, area and configured point budget.
- Preserve whether each path is closed.
- Normalize coordinates to the configured canvas.
- Produce deterministic output for the same input and configuration.
- Report per-frame processing statistics.
- Add tests for empty frames, rectangles, circles, noise and dense frames.

Constraints:
- The first implementation stores polylines only.
- Do not fit Bézier curves yet.
- Do not depend on SDL types.
- Do not write SVG or TXT intermediate files.
```

---

## Milestone 5 — DVC cache

**Mức ưu tiên:** P1

### Công việc

* Định nghĩa format version 1.
* Reader và writer.
* Header validation.
* Cache key.
* Atomic write.
* Chống đọc file hỏng.
* JSON inspector.

### Kết quả chấp nhận

* Convert rồi đọc lại tạo model tương đương.
* File bị truncate phải báo lỗi.
* Version không hỗ trợ phải báo rõ.
* Cache cũ không hợp lệ phải được rebuild.
* Không để file cache nửa chừng nếu chương trình crash.

### Context pack

```text
Objective:
Implement a versioned binary cache for preprocessed vector video.

Format requirements:
- Magic bytes: DVC1.
- Explicit format version.
- Fixed-width integer types.
- Canvas metadata and playback timing.
- Processing configuration fingerprint.
- Frame timestamps.
- Polyline counts and point counts.
- Bounds checking before every allocation.

Required:
- Implement DvcReader and DvcWriter.
- Write to a temporary file and rename atomically on success.
- Reject invalid magic, unsupported version, oversized counts and truncated data.
- Add round-trip and corruption tests.
- Document every byte-level field in docs/cache-format.md.

Do not:
- Add compression in version 1.
- Serialize raw C++ structs directly.
- Depend on host endianness without documenting it.
```

---

## Milestone 6 — SDL3 renderer và playback

**Mức ưu tiên:** P1

### Công việc

* render polyline;
* event loop;
* frame timing;
* play/pause;
* restart;
* next/previous frame;
* playback speed;
* fit-to-window;
* fullscreen;
* optional debug overlay.

SDL3 cung cấp window và renderer 2D đa nền tảng; renderer có thể tạo context cho window và trình bày frame qua event loop. ([Wiki SDL][6])

### Phím điều khiển

```text
Space       Play/Pause
Left/Right  Previous/Next frame
R           Restart
F           Fullscreen
+/-         Playback speed
D           Debug overlay
Esc         Exit
```

### Kết quả chấp nhận

* Playback dựa trên timestamp.
* Không dùng `delay(5000000)`.
* Window vẫn phản hồi trong lúc xử lý.
* Video không nhanh/chậm theo tốc độ máy.
* Resize window không làm méo tỷ lệ.

### Context pack

```text
Objective:
Render VectorFrame data with SDL3 and provide timestamp-based playback.

Required:
- Keep SDL calls inside the renderer/application layer.
- Render polylines using SDL line primitives.
- Use frame timestamps instead of one fixed blocking delay.
- Poll events continuously.
- Support pause, resume, seek by frame, restart and playback speed.
- Preserve aspect ratio when the window is resized.
- Handle empty frames.
- Display useful SDL errors.
- Ensure all SDL resources are released.

Concurrency:
Begin with a single-threaded cached playback implementation.
Do not add background conversion in this milestone.

Do not:
- Mix OpenCV Mats into the renderer interface.
- Block the event loop while waiting for a frame.
- Use global SDL pointers.
```

---

## Milestone 7 — CLI, documentation, tests và CI

**Mức ưu tiên:** P1

### Công việc

CLI hoàn chỉnh:

```text
drawvideo play
drawvideo preview
drawvideo convert
drawvideo inspect
drawvideo legacy-import
```

Thêm:

* help text;
* config file;
* progress;
* error messages;
* sample video;
* screenshots/GIF;
* architecture docs;
* GitHub Actions;
* Windows và Linux build;
* formatting;
* static analysis.

GitHub Actions workflow được lưu trong YAML trong repo và có thể tự động build/test khi push hoặc mở pull request. ([GitHub Docs][7])

### Kết quả chấp nhận

Một người mới có thể:

```bash
git clone ...
cd draw-in-console-using-c-graphics
./scripts/bootstrap
cmake --preset default
cmake --build --preset default
drawvideo play assets/sample.mp4
```

README phải đưa họ từ clone đến thấy cửa sổ phát video mà không cần sửa source code.

### Context pack

```text
Objective:
Make the repository understandable and reproducible for a new contributor.

Required:
- Add CLI subcommands: play, preview, convert, inspect and legacy-import.
- Include --help examples for every subcommand.
- Add one small legally redistributable sample.
- Document architecture, processing stages and DVC format.
- Document Windows and Linux setup.
- Add CI for configure, build and test.
- Pin third-party workflow actions to stable versions or commit SHAs.
- Add clang-format configuration.
- Treat compiler warnings as errors in CI for project-owned code.
- Keep third-party warnings out of the project warning policy.

Acceptance scenario:
A clean CI runner can install dependencies, build the project and run all
non-GUI tests without manual intervention.
```

---

# P2 — Các nâng cấp sau khi MVP ổn định

Không nên đưa các phần này vào PR đầu:

### Bézier fitting

```text
Polyline → segment detection → cubic Bézier fitting
```

Cho phép:

* ít điểm hơn;
* đường mượt hơn;
* gần với project ban đầu hơn.

### Background conversion

Ba stage:

```text
Decoder thread
    ↓ bounded queue
Processor/vectorizer thread pool
    ↓ bounded queue
Renderer/main thread
```

Cần queue có giới hạn để tránh ăn toàn bộ RAM.

### Temporal smoothing

Contour giữa hai frame có thể rung. Có thể thêm:

* contour matching;
* exponential smoothing;
* point stabilization;
* path lifetime;
* bỏ contour xuất hiện chỉ một frame.

### Frame-difference cache

Chỉ lưu những path thay đổi giữa các frame.

### Audio synchronization

Giữ audio gốc hoặc đồng bộ playback theo clock audio. Đây là phần phức tạp, nên làm sau khi video-only ổn định.

### GPU processing

Chỉ xem xét khi profiling chứng minh preprocessing CPU là bottleneck.

# Context engineering cấp repository

Ngoài prompt cho từng milestone, repo cần một bộ context cố định để agent không phải đoán lại kiến trúc mỗi lần.

## `AGENTS.md`

Nội dung bắt buộc:

```text
Project mission
- Convert videos and legacy SVG/TXT drawings into line-based vector animation.
- A new user must be able to build and run an example without editing source.

Architectural boundaries
- Domain models do not depend on OpenCV or SDL.
- OpenCV is restricted to video and image-processing adapters.
- SDL is restricted to application and renderer layers.
- Intermediate PNG/SVG/TXT files are not part of the default pipeline.
- Legacy formats are supported through adapters only.

Non-negotiable rules
- No absolute paths.
- No exit() in library code.
- No silent parse failures.
- No unbounded queues or frame accumulation.
- No generated video frames committed to Git.
- All file formats are versioned.
- All configuration affecting output participates in the cache key.

Required validation
- Add or update tests with every behavior change.
- Run configure, build and tests before claiming completion.
- Update documentation when CLI or file formats change.

Scope discipline
- Each PR handles one milestone or one coherent vertical slice.
- Do not combine a renderer migration, cache redesign and Bézier fitting in
  the same PR.
```

## ADR — Architecture Decision Records

Nên tạo ít nhất bốn quyết định:

```text
ADR-0001: Use OpenCV for video decoding and preprocessing
ADR-0002: Use SDL3 instead of WinBGIm
ADR-0003: Remove SVG/TXT from the default runtime pipeline
ADR-0004: Use a versioned DVC cache
```

Mỗi ADR gồm:

```text
Status
Context
Decision
Alternatives considered
Positive consequences
Negative consequences
Migration impact
```

## Context template cho mọi task

Mỗi task giao cho coding agent nên theo mẫu:

```text
Repository and branch:
Relevant milestone:
Current behavior:
Desired behavior:
Relevant files:
Architecture constraints:
Inputs:
Expected outputs:
Explicit non-goals:
Failure cases:
Acceptance criteria:
Verification commands:
Documentation that must be updated:
```

Điểm quan trọng là luôn có **non-goals**. Đây là cách ngăn agent tiện tay refactor toàn bộ repo hoặc triển khai trước những phần chưa cần thiết.

# Thứ tự PR đề xuất

```text
PR 1  chore/project-foundation
      CMake + vcpkg + SDL3/OpenCV + structure

PR 2  refactor/core-model-and-legacy-reader
      Domain model + safe TXT parser

PR 3  feat/video-input-and-preview
      VideoCapture + preprocessing + raster preview

PR 4  feat/contour-vectorizer
      findContours + polyline simplification

PR 5  feat/dvc-cache
      Binary writer/reader + automatic cache

PR 6  feat/sdl-vector-playback
      Vector renderer + timing + controls

PR 7  docs/cli-ci-and-examples
      CLI polish + tests + CI + complete README

PR 8  feat/bezier-fitting
      Optional curve fitting after profiling
```

## Definition of Done cho toàn project

Project được xem là nâng cấp thành công khi:

* input duy nhất có thể là một file video;
* không cần tự tách frame;
* không cần tạo SVG;
* không cần tạo TXT;
* không cần sửa absolute path;
* một command có thể xử lý và phát video;
* lần chạy sau sử dụng cache;
* dữ liệu TXT cũ vẫn import được;
* build không phụ thuộc Dev-C++;
* Windows và Linux đều build qua CI;
* parser và cache có test lỗi;
* README giải thích được toàn bộ pipeline;
* sample chạy được ngay sau khi setup.

Trong lượt này tôi mới chốt kiến trúc, thứ tự triển khai và context cho từng phần; chưa push thay đổi vào repository. Bước triển khai phù hợp tiếp theo là tạo branch `chore/project-foundation` và thực hiện Milestone 1 trước, không trộn video processing vào PR đầu tiên.

[1]: https://docs.opencv.org/4.12.0/d0/da7/videoio_overview.html?utm_source=chatgpt.com "OpenCV: Video I/O with OpenCV Overview"
[2]: https://docs.opencv.org/master/df/d0d/tutorial_find_contours.html?utm_source=chatgpt.com "OpenCV: Finding contours in your image"
[3]: https://wiki.libsdl.org/SDL3/FrontPage?utm_source=chatgpt.com "SDL3/FrontPage - SDL Wiki"
[4]: https://learn.microsoft.com/en-us/vcpkg/concepts/manifest-mode?utm_source=chatgpt.com "Manifest mode | Microsoft Learn"
[5]: https://docs.opencv.org/4.10.0/d7/d4d/tutorial_py_thresholding.html?utm_source=chatgpt.com "OpenCV: Image Thresholding"
[6]: https://wiki.libsdl.org/SDL3/SDL_CreateRenderer?utm_source=chatgpt.com "SDL3/SDL_CreateRenderer - SDL Wiki"
[7]: https://docs.github.com/en/actions/concepts/workflows-and-actions/workflows?learn=getting_started&learnProduct=actions&utm_source=chatgpt.com "Workflows - GitHub Docs"
