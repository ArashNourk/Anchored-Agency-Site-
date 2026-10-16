#!/usr/bin/env bash
# Builds the WebAssembly engine and the single-file web app.
#   requires: emsdk activated (em++ on PATH), python3
#   output:   web/dist/p10.wasm, web/dist/index.html (self-contained)
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p web/dist
SRCS="engine/dsp/Blep.cpp engine/dsp/Oscillator.cpp engine/dsp/Filters.cpp engine/dsp/Decimator.cpp
      engine/model/Calibration.cpp engine/model/Params.cpp engine/model/Vintage.cpp
      engine/voice/LayerModulator.cpp engine/voice/Voice.cpp engine/voice/VoiceAllocator.cpp
      engine/midi/MidiController.cpp engine/preset/PresetStore.cpp engine/Synth.cpp web/src/p10_wasm.cpp"
em++ -O3 -std=c++17 -Iengine $SRCS -fno-exceptions \
  -sSTANDALONE_WASM --no-entry -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432 \
  -o web/dist/p10.wasm
python3 web/bundle.py web/src/index.html web/dist/p10.wasm web/dist/index.html
echo "built web/dist/index.html"
