cmake_minimum_required(VERSION 3.16)
project(p10engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# The DSP engine: no UI, no audio-API, no plugin-SDK dependencies.
add_library(p10engine STATIC
  engine/dsp/Blep.cpp
  engine/dsp/Oscillator.cpp
  engine/dsp/Filters.cpp
  engine/dsp/Decimator.cpp
  engine/model/Calibration.cpp
  engine/model/Params.cpp
  engine/model/Vintage.cpp
  engine/voice/LayerModulator.cpp
  engine/voice/Voice.cpp
  engine/voice/VoiceAllocator.cpp
  engine/midi/MidiController.cpp
  engine/preset/PresetStore.cpp
  engine/io/WavFile.cpp
  engine/Synth.cpp
)
target_include_directories(p10engine PUBLIC engine)

add_executable(p10render apps/p10render.cpp)
target_link_libraries(p10render p10engine)

enable_testing()
add_executable(p10tests tests/test_main.cpp)
target_link_libraries(p10tests p10engine)
add_test(NAME p10tests COMMAND p10tests)
