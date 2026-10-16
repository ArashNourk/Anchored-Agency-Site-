// Offline renderer: plays a scenario script through the engine and writes a
// 32-bit float WAV. Used for listening tests and for producing the software
// side of every hardware comparison in validation/.
//
// Scenario syntax (one command per line, '#' comments, times in seconds):
//   samplerate 48000 | oversample 4 | serial 1 | length 3.0
//   calibration <file>             load calibration overrides
//   program <file>                 load a program (preset text)
//   set <A|B|P>.<key> <value>      immediate (before t=0)
//   at <t> set <A|B|P>.<key> <v>   parameter change
//   at <t> on <note> <vel 1-127>   note on       at <t> off <note>
//   at <t> cc <n> <v>              MIDI CC       at <t> bend <-1..1>
//   at <t> pressure <0..1>         aftertouch    at <t> tune
//   ramp <t0> <t1> <A|B|P>.<key> <v0> <v1>     linear sweep (1 ms steps)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../engine/Synth.h"
#include "../engine/io/WavFile.h"
#include "../engine/midi/MidiController.h"
#include "../engine/preset/PresetStore.h"

using namespace p10;

namespace {
struct Event {
    double t;
    int order;
    std::vector<std::string> args;
};

bool applySet(Synth& s, const std::string& target, double v) {
    if (target.size() < 3 || target[1] != '.') return false;
    const std::string key = target.substr(2);
    if (target[0] == 'P') {
        const int id = findProgramParam(key);
        if (id < 0) return false;
        s.setProgramParam(id, v);
        return true;
    }
    const int id = findLayerParam(key);
    if (id < 0) return false;
    s.setLayerParam(target[0] == 'B' ? 1 : 0, id, v);
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--dump-calibration") {
        CalibrationModel c;
        std::cout << c.toText();
        return 0;
    }
    if (argc < 3) {
        std::cerr << "usage: p10render <scenario.txt> <out.wav>\n       p10render --dump-calibration\n";
        return 1;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open " << argv[1] << "\n";
        return 1;
    }
    double fs = 48000, length = 2.0;
    int os = 4;
    unsigned long long serial = 1;
    std::string calFile, programFile;
    std::vector<std::pair<std::string, double>> presets;
    std::vector<Event> events;
    std::string line;
    int order = 0;
    while (std::getline(in, line)) {
        const auto h = line.find('#');
        if (h != std::string::npos) line.resize(h);
        std::istringstream ls(line);
        std::vector<std::string> a;
        for (std::string w; ls >> w;) a.push_back(w);
        if (a.empty()) continue;
        try {
            if (a[0] == "samplerate") fs = std::stod(a.at(1));
            else if (a[0] == "oversample") os = std::stoi(a.at(1));
            else if (a[0] == "serial") serial = std::stoull(a.at(1));
            else if (a[0] == "length") length = std::stod(a.at(1));
            else if (a[0] == "calibration") calFile = a.at(1);
            else if (a[0] == "program") programFile = a.at(1);
            else if (a[0] == "set") presets.push_back({a.at(1), std::stod(a.at(2))});
            else if (a[0] == "at") events.push_back({std::stod(a.at(1)), order++, {a.begin() + 2, a.end()}});
            else if (a[0] == "ramp") {
                const double t0 = std::stod(a.at(1)), t1 = std::stod(a.at(2));
                const double v0 = std::stod(a.at(4)), v1 = std::stod(a.at(5));
                const int steps = std::max(1, static_cast<int>((t1 - t0) * 1000.0));
                for (int i = 0; i <= steps; ++i) {
                    const double f = static_cast<double>(i) / steps;
                    events.push_back({t0 + (t1 - t0) * f, order++, {"set", a.at(3), std::to_string(v0 + (v1 - v0) * f)}});
                }
            } else {
                std::cerr << "unknown command: " << line << "\n";
                return 1;
            }
        } catch (...) {
            std::cerr << "bad line: " << line << "\n";
            return 1;
        }
    }

    auto synth = std::make_unique<Synth>(fs, os, serial);
    if (!calFile.empty()) {
        std::vector<std::string> errs;
        synth->calibration().loadFromFile(calFile, &errs);
        for (auto& e : errs) std::cerr << "calibration: " << e << "\n";
        synth->calibrationChanged();
        synth->vintage().build(serial);
    }
    if (!programFile.empty()) {
        std::ifstream pf(programFile);
        std::stringstream ss;
        ss << pf.rdbuf();
        std::vector<std::string> warn;
        auto p = programFromText(ss.str(), &warn);
        for (auto& w : warn) std::cerr << "program: " << w << "\n";
        if (p) synth->setProgram(*p);
    }
    for (auto& [k, v] : presets)
        if (!applySet(*synth, k, v)) std::cerr << "unknown parameter " << k << "\n";
    // Let smoothed knobs settle on their initial values before t = 0.
    {
        std::vector<double> warm(static_cast<size_t>(fs * 0.05));
        synth->render(warm.data(), static_cast<int>(warm.size()));
    }
    MidiController midi(synth.get());

    std::stable_sort(events.begin(), events.end(),
                     [](const Event& a, const Event& b) { return a.t < b.t || (a.t == b.t && a.order < b.order); });
    const long total = static_cast<long>(length * fs);
    std::vector<float> out(total * 2);
    long pos = 0;
    size_t ei = 0;
    while (pos < total) {
        while (ei < events.size() && static_cast<long>(events[ei].t * fs) <= pos) {
            const auto& a = events[ei].args;
            try {
                if (a.at(0) == "set") {
                    if (!applySet(*synth, a.at(1), std::stod(a.at(2)))) std::cerr << "unknown parameter " << a[1] << "\n";
                } else if (a[0] == "on") synth->noteOn(std::stoi(a.at(1)), std::stod(a.at(2)) / 127.0);
                else if (a[0] == "off") synth->noteOff(std::stoi(a.at(1)));
                else if (a[0] == "cc") midi.message(0xB0, std::stoi(a.at(1)), std::stoi(a.at(2)));
                else if (a[0] == "bend") synth->setPitchBend(std::stod(a.at(1)));
                else if (a[0] == "pressure") synth->setAftertouch(std::stod(a.at(1)));
                else if (a[0] == "tune") synth->tune();
                else std::cerr << "unknown event " << a[0] << "\n";
            } catch (...) {
                std::cerr << "bad event at " << events[ei].t << "\n";
            }
            ++ei;
        }
        long next = total;
        if (ei < events.size()) next = std::min(next, std::max(pos + 1, static_cast<long>(events[ei].t * fs)));
        const int n = static_cast<int>(next - pos);
        std::vector<float> l(n), r(n);
        synth->render(l.data(), r.data(), n);
        for (int i = 0; i < n; ++i) {
            out[2 * (pos + i)] = l[i];
            out[2 * (pos + i) + 1] = r[i];
        }
        pos = next;
    }
    if (!writeWavFloat(argv[2], out, 2, static_cast<int>(fs))) {
        std::cerr << "cannot write " << argv[2] << "\n";
        return 1;
    }
    float peak = 0;
    for (float v : out) peak = std::max(peak, std::fabs(v));
    std::printf("wrote %s (%.2f s, peak %.3f, latency %.1f samples)\n", argv[2], length, peak, synth->latencySamples());
    return 0;
}
