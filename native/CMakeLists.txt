cmake_minimum_required(VERSION 3.20)
project(SmartJumpNative LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(minhook)

add_library(SmartJumpNative SHARED SmartJumpNative.cpp)
target_compile_features(SmartJumpNative PRIVATE cxx_std_20)
target_compile_definitions(SmartJumpNative PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(SmartJumpNative PRIVATE minhook)
set_target_properties(SmartJumpNative PROPERTIES OUTPUT_NAME "SmartJumpNative")

enable_testing()
add_executable(SmartJumpTimingTests TimingPlannerTests.cpp)
target_compile_features(SmartJumpTimingTests PRIVATE cxx_std_20)
add_test(NAME SmartJumpTimingTests COMMAND SmartJumpTimingTests)
