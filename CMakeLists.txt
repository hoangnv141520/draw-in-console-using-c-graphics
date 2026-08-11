cmake_minimum_required(VERSION 3.21)

project(drawvideo VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

find_package(OpenCV CONFIG REQUIRED COMPONENTS core imgproc videoio)
find_package(SDL3 CONFIG REQUIRED)

add_executable(drawvideo apps/drawvideo/main.cpp)

target_compile_features(drawvideo PRIVATE cxx_std_17)
target_link_libraries(drawvideo PRIVATE SDL3::SDL3 opencv_core opencv_imgproc opencv_videoio)

if(BUILD_TESTING)
    add_test(NAME drawvideo_version COMMAND drawvideo --version)
    add_test(NAME drawvideo_sdl_smoke COMMAND drawvideo --smoke-test)
    set_tests_properties(drawvideo_sdl_smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    )
endif()
