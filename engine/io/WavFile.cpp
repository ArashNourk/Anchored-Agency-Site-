#include "WavFile.h"

#include <cstdint>
#include <cstring>
#include <fstream>

namespace p10 {

namespace {
void put32(std::ofstream& f, uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); }
void put16(std::ofstream& f, uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); }
}  // namespace

bool writeWavFloat(const std::string& path, const std::vector<float>& data, int channels, int sampleRate) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const uint32_t bytes = static_cast<uint32_t>(data.size() * sizeof(float));
    f.write("RIFF", 4);
    put32(f, 36 + bytes);
    f.write("WAVEfmt ", 8);
    put32(f, 16);
    put16(f, 3);  // IEEE float
    put16(f, static_cast<uint16_t>(channels));
    put32(f, static_cast<uint32_t>(sampleRate));
    put32(f, static_cast<uint32_t>(sampleRate * channels * 4));
    put16(f, static_cast<uint16_t>(channels * 4));
    put16(f, 32);
    f.write("data", 4);
    put32(f, bytes);
    f.write(reinterpret_cast<const char*>(data.data()), bytes);
    return static_cast<bool>(f);
}

bool readWav(const std::string& path, std::vector<float>& out, int& channels, int& sampleRate) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char id[4];
    uint32_t size;
    f.read(id, 4);
    f.read(reinterpret_cast<char*>(&size), 4);
    f.read(id, 4);
    if (std::memcmp(id, "WAVE", 4) != 0) return false;
    uint16_t format = 0, bits = 0, ch = 0;
    uint32_t rate = 0;
    while (f.read(id, 4)) {
        f.read(reinterpret_cast<char*>(&size), 4);
        if (std::memcmp(id, "fmt ", 4) == 0) {
            std::vector<char> b(size);
            f.read(b.data(), size);
            std::memcpy(&format, &b[0], 2);
            std::memcpy(&ch, &b[2], 2);
            std::memcpy(&rate, &b[4], 4);
            std::memcpy(&bits, &b[14], 2);
            if (format == 0xFFFE && size >= 26) std::memcpy(&format, &b[24], 2);
        } else if (std::memcmp(id, "data", 4) == 0) {
            std::vector<uint8_t> b(size);
            f.read(reinterpret_cast<char*>(b.data()), size);
            const int bps = bits / 8;
            if (bps == 0 || ch == 0) return false;
            const size_t n = size / bps;
            out.resize(n);
            for (size_t i = 0; i < n; ++i) {
                const uint8_t* p = &b[i * bps];
                if (format == 3 && bits == 32) {
                    std::memcpy(&out[i], p, 4);
                } else if (bits == 16) {
                    int16_t v;
                    std::memcpy(&v, p, 2);
                    out[i] = v / 32768.0f;
                } else if (bits == 24) {
                    int32_t v = (p[0] << 8) | (p[1] << 16) | (p[2] << 24);
                    out[i] = (v >> 8) / 8388608.0f;
                } else if (bits == 32) {
                    int32_t v;
                    std::memcpy(&v, p, 4);
                    out[i] = static_cast<float>(v / 2147483648.0);
                } else {
                    return false;
                }
            }
            channels = ch;
            sampleRate = static_cast<int>(rate);
            return true;
        } else {
            f.seekg(size + (size & 1), std::ios::cur);
        }
    }
    return false;
}

}  // namespace p10
