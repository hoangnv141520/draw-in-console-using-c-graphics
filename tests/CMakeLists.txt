add_executable(drawvideo_core_tests legacy_txt_reader_tests.cpp)

target_link_libraries(drawvideo_core_tests PRIVATE
    drawvideo_core
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(drawvideo_core_tests
    TEST_PREFIX "drawvideo_core."
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
