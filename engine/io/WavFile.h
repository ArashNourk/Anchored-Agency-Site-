// Minimal WAV (IEEE float 32) writer and reader for offline rendering and the
// validation tools.
#pragma once

#include <string>
#include <vector>

namespace p10 {

bool writeWavFloat(const std::string& path, const std::vector<float>& interleaved, int channels, int sampleRate);
// Reads 16/24/32-bit PCM or 32-bit float. Returns channel-interleaved floats.
bool readWav(const std::string& path, std::vector<float>& interleaved, int& channels, int& sampleRate);

}  // namespace p10
