// Engine self-validation. These tests check internal consistency and the
// properties the specification demands (band-limiting, audio-rate Poly Mod,
// self-oscillation, distinct filter models, persistent voices, Vintage,
// calibration, allocation, stability). They do NOT prove fidelity to the
// hardware; that requires the measurement procedures in validation/.
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Synth.h"
#include "dsp/Envelope.h"
#include "midi/MidiController.h"
#include "preset/PresetStore.h"

using namespace p10;

static int g_failed = 0, g_passed = 0;
#define CHECK(cond, ...)                                             \
    do {                                                             \
        if (cond) {                                                  \
            ++g_passed;                                              \
        } else {                                                     \
            ++g_failed;                                              \
            std::printf("  FAIL %s:%d: %s | ", __FILE__, __LINE__, #cond); \
            std::printf(__VA_ARGS__);                                \
            std::printf("\n");                                       \
        }                                                            \
    } while (0)

// ---------------------------------------------------------------- analysis --
static void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2 * kPi / len;
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t j = 0; j < len / 2; ++j) {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Magnitude spectrum in dB (Blackman-Harris window), N = power of two.
static std::vector<double> spectrumDb(const std::vector<double>& x, size_t start, size_t n) {
    std::vector<std::complex<double>> a(n);
    for (size_t i = 0; i < n; ++i) {
        const double w = (double)i / (n - 1);
        const double win = 0.35875 - 0.48829 * std::cos(2 * kPi * w) + 0.14128 * std::cos(4 * kPi * w) -
                           0.01168 * std::cos(6 * kPi * w);
        a[i] = x[start + i] * win;
    }
    fft(a);
    std::vector<double> db(n / 2);
    for (size_t i = 0; i < n / 2; ++i) db[i] = 20 * std::log10(std::abs(a[i]) + 1e-15);
    return db;
}

static double rms(const std::vector<double>& x, size_t a, size_t b) {
    double s = 0;
    for (size_t i = a; i < b; ++i) s += x[i] * x[i];
    return std::sqrt(s / std::max<size_t>(1, b - a));
}

// Highest non-harmonic component (dB relative to the strongest bin) within
// 20 Hz..20 kHz, excluding +-guard bins around multiples of f0.
// The analog model is never exactly on nominal pitch (calibration residual,
// HF tracking), so the true fundamental is measured from the spectrum first.
static double measureF0(const std::vector<double>& db, double nominal, double binHz) {
    const size_t lo = (size_t)(nominal * 0.97 / binHz), hi = (size_t)(nominal * 1.03 / binHz) + 1;
    size_t pk = lo;
    for (size_t i = lo; i <= hi; ++i)
        if (db[i] > db[pk]) pk = i;
    const double a = db[pk - 1], b = db[pk], c = db[pk + 1];
    const double off = 0.5 * (a - c) / (a - 2 * b + c);
    return (pk + off) * binHz;
}

static double worstInharmonicDb(const std::vector<double>& db, double f0nominal, double fs, size_t n, int guard = 6) {
    const double binHz = fs / n;
    const double f0 = measureF0(db, f0nominal, binHz);
    double peak = -1e9;
    for (double v : db) peak = std::max(peak, v);
    double worst = -1e9;
    for (size_t i = (size_t)(20 / binHz); i < db.size() && i * binHz < 20000; ++i) {
        const double f = i * binHz;
        const double h = std::round(f / f0);
        if (h >= 1 && std::fabs(f - h * f0) <= guard * binHz) continue;
        if (h < 1 && f < f0 * 0.5 && f < guard * binHz) continue;
        worst = std::max(worst, db[i] - peak);
    }
    return worst;
}

static std::vector<double> renderNote(Synth& s, int note, double seconds, double vel = 1.0) {
    std::vector<double> warm(2400);
    s.render(warm.data(), (int)warm.size());
    s.noteOn(note, vel);
    std::vector<double> out((size_t)(seconds * s.sampleRate()));
    s.render(out.data(), (int)out.size());
    s.noteOff(note);
    return out;
}

// Removes the intentional analog phase noise (jitter/drift) so that only
// digital artefacts remain in spectral alias measurements.
static void quietAnalog(Synth& s) {
    auto& c = s.calibration();
    c.vJitterCents = {0, 0, 0, 0};
    c.vDriftCents = {0, 0, 0, 0};
    c.vGlobalDriftCents = {0, 0, 0, 0};
}

static void basicDry(Synth& s, int layer = 0) {
    Program p = Program::init();
    s.setProgram(p);
    s.setLayerParam(layer, kAEnvA, 0.0);
    s.setLayerParam(layer, kAEnvS, 1.0);
    s.setLayerParam(layer, kFltCutoff, 1.0);
    s.setLayerParam(layer, kFltResonance, 0.0);
}

// ------------------------------------------------------------------ tests --
static void testAliasingSaw() {
    std::printf("[osc] band-limited sawtooth / pulse / sync / PWM\n");
    const double fs = 48000;
    const size_t N = 32768;
    {
        Synth s(fs, 4, 7);
        quietAnalog(s);
        basicDry(s);
        const int note = 100;  // ~2637 Hz: 7 harmonics below 20 kHz, many above
        auto x = renderNote(s, note, 1.0);
        const double f0 = midiNoteToHz(note);
        const double worst = worstInharmonicDb(spectrumDb(x, 4000, N), f0, fs, N, 8);
        std::printf("  saw @ %.0f Hz worst alias %.1f dB\n", f0, worst);
        CHECK(worst < -85.0, "saw alias %.1f dB", worst);
    }
    {
        Synth s(fs, 4, 7);
        quietAnalog(s);
        basicDry(s);
        s.setLayerParam(0, kOscASaw, 0);
        s.setLayerParam(0, kOscAPulse, 1);
        s.setLayerParam(0, kOscAPw, 0.12);  // narrow pulse
        const int note = 96;
        auto x = renderNote(s, note, 1.0);
        const double f0 = midiNoteToHz(note);
        const double worst = worstInharmonicDb(spectrumDb(x, 4000, N), f0, fs, N, 8);
        std::printf("  narrow pulse @ %.0f Hz worst alias %.1f dB\n", f0, worst);
        CHECK(worst < -85.0, "pulse alias %.1f dB", worst);
    }
    {
        // Hard sync: A at a non-integer ratio above B; all energy must sit on
        // harmonics of B.
        Synth s(fs, 4, 7);
        quietAnalog(s);
        basicDry(s);
        s.setLayerParam(0, kOscASync, 1);
        s.setLayerParam(0, kOscAFreq, 0.5 + 7.0 / 48.0);  // +7 semitones
        s.setLayerParam(0, kOscBFine, 0.5);
        const int note = 84;
        auto x = renderNote(s, note, 1.0);
        const double fB = midiNoteToHz(note);
        const double worst = worstInharmonicDb(spectrumDb(x, 4000, N), fB, fs, N, 8);
        std::printf("  sync A(+7) -> B @ %.0f Hz worst alias %.1f dB\n", fB, worst);
        CHECK(worst < -85.0, "sync alias %.1f dB", worst);
    }
}

static void testSyncPolyModAliasing(int os) {
    std::printf("[osc] sync + audio-rate Poly Mod (Osc B -> Freq A + PW A), all periodic in Osc B\n");
    const double fs = 48000;
    const size_t N = 32768;
    Synth s(fs, os, 11);
    quietAnalog(s);
    basicDry(s);
    s.setLayerParam(0, kOscASync, 1);
    s.setLayerParam(0, kOscAPulse, 1);
    s.setLayerParam(0, kOscAPw, 0.3);
    s.setLayerParam(0, kOscAFreq, 0.5 + 12.0 / 48.0);
    s.setLayerParam(0, kPmOscBAmt, 0.4);
    s.setLayerParam(0, kPmFreqA, 1);
    s.setLayerParam(0, kPmPwA, 1);
    const int note = 72;
    auto x = renderNote(s, note, 1.0);
    const double worst = worstInharmonicDb(spectrumDb(x, 4000, N), midiNoteToHz(note), fs, N, 8);
    std::printf("  %dx oversampling: worst alias %.1f dB\n", os, worst);
    CHECK(worst < (os >= 8 ? -85.0 : -75.0), "sync+polymod alias %.1f dB", worst);
}

static void testAudioRatePolyMod() {
    std::printf("[polymod] audio-rate Osc B -> Freq A produces FM sidebands\n");
    const double fs = 48000;
    const size_t N = 32768;
    auto run = [&](double amount) {
        Synth s(fs, 4, 3);
        basicDry(s);
        s.setLayerParam(0, kPmOscBAmt, amount);
        s.setLayerParam(0, kPmFreqA, 1);
        s.setLayerParam(0, kOscBSaw, 0);
        s.setLayerParam(0, kOscBTri, 1);
        s.setLayerParam(0, kOscBKbd, 0);
        s.setLayerParam(0, kOscBFreq, 0.5 + 19.0 / 48.0);  // B fixed ~ 784 Hz
        s.setLayerParam(0, kOscBFine, 0.5);
        auto x = renderNote(s, 57, 1.0);  // A = 220 Hz
        return spectrumDb(x, 4000, N);
    };
    const auto dry = run(0.0), fm = run(0.35);
    const double binHz = fs / N;
    const double fA = 220.0, fB = midiNoteToHz(60 + 19);
    const size_t sb = (size_t)std::round((fA + fB) / binHz);
    double dryLvl = -1e9, fmLvl = -1e9;
    for (size_t i = sb - 4; i <= sb + 4; ++i) {
        dryLvl = std::max(dryLvl, dry[i]);
        fmLvl = std::max(fmLvl, fm[i]);
    }
    double peakDry = -1e9;
    for (double v : dry) peakDry = std::max(peakDry, v);
    std::printf("  sideband fA+fB: dry %.1f dB, polymod %.1f dB (rel peak)\n", dryLvl - peakDry, fmLvl - peakDry);
    CHECK(fmLvl - dryLvl > 30.0, "sideband rise %.1f dB", fmLvl - dryLvl);
}

static void testSelfOscillation() {
    std::printf("[filter] self-oscillation in both models\n");
    for (int model = 0; model < 2; ++model) {
        Synth s(48000, 4, 5);
        basicDry(s);
        s.setLayerParam(0, kFltRev, model);
        s.setLayerParam(0, kMixOscA, 0.0);
        s.setLayerParam(0, kFltResonance, 1.0);
        s.setLayerParam(0, kFltKbd, 0);
        // Cutoff knob for ~1 kHz.
        const auto& c = s.calibration();
        const double knob = std::log(1000.0 / c.cutoffMinHz) / std::log(c.cutoffMaxHz / c.cutoffMinHz);
        s.setLayerParam(0, kFltCutoff, knob);
        auto x = renderNote(s, 60, 2.0);
        const double r = rms(x, 48000, 96000);
        auto db = spectrumDb(x, 48000, 32768);
        size_t pk = 1;
        for (size_t i = 1; i < db.size(); ++i)
            if (db[i] > db[pk]) pk = i;
        const double f = pk * 48000.0 / 32768;
        std::printf("  %s: rms %.3f, oscillation at %.0f Hz (cutoff 1000 Hz nominal)\n",
                    model ? "CEM3320" : "SSM2040", r, f);
        CHECK(r > 0.02, "no self-oscillation, rms %.4f", r);
        CHECK(f > 700 && f < 1400, "oscillation frequency %.0f", f);
    }
}

static void testFiltersDiffer() {
    std::printf("[filter] the two models are measurably different\n");
    auto fundamentalLoss = [](int model) {
        auto level = [&](double res) {
            Synth s(48000, 4, 5);
            basicDry(s);
            s.setLayerParam(0, kFltRev, model);
            s.setLayerParam(0, kFltKbd, 0);
            s.setLayerParam(0, kFltCutoff, 0.6);
            s.setLayerParam(0, kFltResonance, res);
            auto x = renderNote(s, 36, 1.0);  // 65 Hz, well inside the passband
            auto db = spectrumDb(x, 4000, 32768);
            const size_t b = (size_t)std::round(midiNoteToHz(36) / (48000.0 / 32768));
            double m = -1e9;
            for (size_t i = b - 3; i <= b + 3; ++i) m = std::max(m, db[i]);
            return m;
        };
        return level(0.75) - level(0.0);
    };
    const double ssm = fundamentalLoss(0), cem = fundamentalLoss(1);
    std::printf("  fundamental change at res 0.75: SSM2040 %.1f dB, CEM3320 %.1f dB\n", ssm, cem);
    CHECK(cem < ssm - 2.0, "expected more passband loss in the CEM3320 model (%.1f vs %.1f)", cem, ssm);
}

static void testEnvelope() {
    std::printf("[env] RC envelope timing\n");
    AnalogEnvelope e;
    e.setSampleRate(48000);
    e.setShape(1.3, 4.6);
    EnvTimes t{0.1, 0.5, 0.5, 0.3};
    e.gateOn();
    int n = 0;
    while (e.stage() == AnalogEnvelope::Stage::Attack && n < 48000) {
        e.process(t);
        ++n;
    }
    std::printf("  attack 100 ms -> %.2f ms\n", n / 48.0);
    CHECK(std::fabs(n / 48.0 - 100.0) < 0.5, "attack %.2f ms", n / 48.0);
    // Attack curve is convex-exponential: at half the time it is above 0.5.
    AnalogEnvelope e2;
    e2.setSampleRate(48000);
    e2.setShape(1.3, 4.6);
    e2.gateOn();
    double v = 0;
    for (int i = 0; i < 2400; ++i) v = e2.process(t);
    CHECK(v > 0.55 && v < 0.75, "attack midpoint %.3f", v);
    // Release: from sustain to -40 dB (factor 0.01) in releaseSec.
    for (int i = 0; i < 48000; ++i) e.process(t);
    const double start = e.value();
    e.gateOff();
    for (int i = 0; i < (int)(0.3 * 48000); ++i) e.process(t);
    std::printf("  release 300 ms: %.4f of start level (target 0.0100)\n", e.value() / start);
    CHECK(std::fabs(e.value() / start - 0.01) < 0.002, "release ratio %.4f", e.value() / start);
    // Retrigger continues from the current level.
    e.gateOn();
    const double before = e.value();
    e.process(t);
    CHECK(e.value() >= before, "retrigger reset the envelope");
}

static void testAllocation() {
    std::printf("[alloc] poly rotation, same-note reuse, stealing, unison, split, stack\n");
    Synth s(48000, 2, 1);
    basicDry(s);
    std::vector<double> buf(256);
    std::vector<int> assigned;
    for (int i = 0; i < 5; ++i) {
        s.noteOn(60 + i, 1.0);
        assigned.push_back(s.allocator().lastAssigned(0));
    }
    bool distinct = true;
    for (int i = 1; i < 5; ++i) distinct &= assigned[i] != assigned[i - 1];
    CHECK(distinct, "rotation did not move to a new voice");
    s.noteOff(62);
    s.render(buf.data(), 256);
    s.noteOn(62, 1.0);
    CHECK(s.allocator().lastAssigned(0) == assigned[2], "same note did not reuse its voice");
    for (int i = 5; i < 10; ++i) s.noteOn(60 + i, 1.0);
    int gated = 0;
    for (int v = 0; v < kNumVoices; ++v) gated += s.voice(v).gated();
    CHECK(gated == 10, "expected 10 gated voices, got %d", gated);
    s.noteOn(80, 1.0);
    // Oldest gated note is 60 (61 was started before 62's retrigger).
    CHECK(s.allocator().lastAssigned(0) == assigned[0], "steal did not take the oldest voice");
    s.allNotesOff();

    // Unison with 4 voices.
    basicDry(s);
    s.setLayerParam(0, kUnison, 1);
    s.setLayerParam(0, kUnisonVoices, 4);
    s.noteOn(48, 1.0);
    gated = 0;
    for (int v = 0; v < kNumVoices; ++v) gated += s.voice(v).gated() && s.voice(v).note() == 48;
    CHECK(gated == 4, "unison gated %d voices", gated);
    s.noteOn(55, 1.0);  // last-note priority
    int on55 = 0;
    for (int v = 0; v < kNumVoices; ++v) on55 += s.voice(v).gated() && s.voice(v).note() == 55;
    CHECK(on55 == 4, "unison last-note priority failed (%d)", on55);
    s.noteOff(55);
    int back = 0;
    for (int v = 0; v < kNumVoices; ++v) back += s.voice(v).gated() && s.voice(v).note() == 48;
    CHECK(back == 4, "unison did not return to held key (%d)", back);
    s.allNotesOff();

    // Split: lower -> layer A on voices 0-4, upper -> layer B on voices 5-9.
    Program p = Program::init();
    p.settings[kLayerMode] = 1;
    p.settings[kSplitPoint] = 60;
    s.setProgram(p);
    s.noteOn(40, 1.0);
    const int lo = s.allocator().lastAssigned(0);
    s.noteOn(72, 1.0);
    const int hi = s.allocator().lastAssigned(1);
    CHECK(lo >= 0 && lo < 5 && s.voice(lo).layer() == 0, "split lower voice %d", lo);
    CHECK(hi >= 5 && s.voice(hi).layer() == 1 && s.voice(hi).note() == 72, "split upper voice %d", hi);
    s.allNotesOff();

    // Stack: one key -> one voice in each layer.
    p.settings[kLayerMode] = 2;
    s.setProgram(p);
    s.noteOn(64, 1.0);
    gated = 0;
    for (int v = 0; v < kNumVoices; ++v) gated += s.voice(v).gated();
    CHECK(gated == 2 && s.voice(s.allocator().lastAssigned(0)).layer() == 0 &&
              s.voice(s.allocator().lastAssigned(1)).layer() == 1,
          "stack gated %d", gated);
}

static void testVintageAndTuning() {
    std::printf("[vintage] persistent personalities, spread vs knob, calibration\n");
    Synth s(48000, 2, 42);
    auto& ve = s.vintage();
    auto spread = [&](double vin) {
        double m = 0, m2 = 0;
        for (int v = 0; v < kNumVoices; ++v) {
            const double t = ve.state(v, vin).tuneCentsA;
            m += t;
            m2 += t * t;
        }
        m /= kNumVoices;
        return std::sqrt(m2 / kNumVoices - m * m);
    };
    const double s0 = spread(0.0), s1 = spread(1.0);
    std::printf("  osc A tuning spread: vintage 0 %.2f cents, vintage 1 %.2f cents\n", s0, s1);
    CHECK(s0 < 1.0, "calibrated spread too large %.2f", s0);
    CHECK(s1 > 2.0, "vintage spread too small %.2f", s1);
    // Same voice deviates in the same direction as Vintage rises.
    int consistent = 0;
    for (int v = 0; v < kNumVoices; ++v) {
        const double a = ve.state(v, 0.5).cutoffOffsetSemis - ve.state(v, 0.0).cutoffOffsetSemis;
        const double b = ve.state(v, 1.0).cutoffOffsetSemis - ve.state(v, 0.0).cutoffOffsetSemis;
        consistent += (a * b > 0);
    }
    CHECK(consistent == kNumVoices, "personality not persistent (%d/10)", consistent);
    // Vintage affects far more than pitch.
    const auto st = ve.state(3, 1.0);
    CHECK(std::fabs(st.filterEnvTimeScale - 1.0) > 1e-4 && std::fabs(st.vcaGain - 1.0) > 1e-5 &&
              std::fabs(st.pwOffsetA) > 1e-6 && std::fabs(st.trackError) > 1e-6,
          "vintage did not vary non-pitch components");
    // Aging then tuning.
    auto maxErr = [&]() {
        double m = 0;
        for (int v = 0; v < kNumVoices; ++v)
            for (int o = 0; o < 2; ++o)
                for (double n : {36.0, 60.0, 96.0}) m = std::max(m, std::fabs(ve.measuredPitchErrorCents(v, o, n)));
        return m;
    };
    const double fresh = maxErr();
    ve.age(200.0);
    const double aged = maxErr();
    ve.tune();
    const double retuned = maxErr();
    std::printf("  worst static error: factory %.2f c, after 200 h %.2f c, after tune %.2f c\n", fresh, aged,
                retuned);
    CHECK(aged > fresh && retuned < 0.5, "tune did not restore calibration");
}

static void testStability() {
    std::printf("[stability] extreme modulation, rapid knob movement\n");
    Synth s(48000, 4, 9);
    Program p = Program::init();
    for (int l = 0; l < 2; ++l) {
        auto& L = p.layer[l];
        L[kFltResonance] = 1.0;
        L[kPmOscBAmt] = 1.0;
        L[kPmFilterEnvAmt] = 1.0;
        L[kPmFreqA] = L[kPmPwA] = L[kPmFilter] = 1;
        L[kOscASync] = 1;
        L[kOscAPulse] = 1;
        L[kOscBPulse] = L[kOscBTri] = 1;
        L[kMixOscA] = L[kMixOscB] = L[kMixNoise] = 1.0;
        L[kLfoInitialAmount] = 1.0;
        L[kLfoFreq] = 1.0;
        L[kLfoSaw] = L[kLfoSquare] = 1;
        L[kWmFreqA] = L[kWmFreqB] = L[kWmPwA] = L[kWmPwB] = L[kWmFilter] = 1;
        L[kFltEnvAmt] = 1.0;
        L[kVintage] = 1.0;
    }
    p.settings[kLayerMode] = 2;
    s.setProgram(p);
    Rng r(1);
    std::vector<double> buf(480);
    double peak = 0;
    bool finite = true;
    for (int blk = 0; blk < 400; ++blk) {
        if (blk % 20 == 0) s.noteOn(24 + (int)(r.uniform() * 84), r.uniform());
        if (blk % 37 == 0) s.noteOff(24 + (int)(r.uniform() * 84));
        s.setLayerParam(blk % 2, kFltCutoff, r.uniform());
        s.setLayerParam(blk % 2, kOscAFreq, r.uniform());
        s.setLayerParam(blk % 2, kOscAPw, r.uniform());
        s.setPitchBend(r.bipolar());
        s.setModWheel(r.uniform());
        s.render(buf.data(), (int)buf.size());
        for (double v : buf) {
            finite &= std::isfinite(v);
            peak = std::max(peak, std::fabs(v));
        }
    }
    std::printf("  peak %.3f\n", peak);
    CHECK(finite, "non-finite output");
    CHECK(peak < 8.0, "output blew up %.3f", peak);
}

static void testPerformanceControls() {
    std::printf("[perf] velocity, aftertouch, glide, smoothing\n");
    {
        Synth s(48000, 2, 1);
        basicDry(s);
        s.setLayerParam(0, kAEnvVel, 1);
        auto loud = renderNote(s, 60, 0.3, 1.0);
        Synth s2(48000, 2, 1);
        basicDry(s2);
        s2.setLayerParam(0, kAEnvVel, 1);
        auto soft = renderNote(s2, 60, 0.3, 0.3);
        const double a = rms(loud, 4800, 14400), b = rms(soft, 4800, 14400);
        CHECK(b < 0.8 * a, "velocity had no effect %.3f %.3f", a, b);
    }
    {
        auto centroid = [](const std::vector<double>& x) {
            auto db = spectrumDb(x, 4800, 16384);
            double num = 0, den = 0;
            for (size_t i = 1; i < db.size(); ++i) {
                const double m = std::pow(10.0, db[i] / 20);
                num += m * i;
                den += m;
            }
            return num / den * 48000 / 16384;
        };
        Synth s(48000, 2, 1);
        basicDry(s);
        s.setLayerParam(0, kFltCutoff, 0.45);
        s.setLayerParam(0, kAtFilter, 1);
        auto dull = renderNote(s, 48, 0.5);
        s.setAftertouch(1.0);
        auto bright = renderNote(s, 48, 0.5);
        const double c0 = centroid(dull), c1 = centroid(bright);
        std::printf("  aftertouch -> filter: centroid %.0f -> %.0f Hz\n", c0, c1);
        CHECK(c1 > c0 * 1.3, "aftertouch filter had no effect");
    }
    {
        Synth s(48000, 2, 1);
        basicDry(s);
        s.setLayerParam(0, kGlide, 0.5);
        std::vector<double> buf(4800);
        s.noteOn(48, 1.0);
        s.render(buf.data(), 4800);
        s.noteOff(48);
        s.noteOn(60, 1.0);  // rotates to a new voice: that voice has no history
        s.noteOff(60);
        s.render(buf.data(), 4800);
        s.noteOn(72, 1.0);  // voice 2
        const int v = s.allocator().lastAssigned(0);
        s.render(buf.data(), 480);
        const double p1 = s.voice(v).currentPitch();
        CHECK(p1 == 72.0, "fresh voice should start at its first note (%.2f)", p1);
        // Return to voice 0 (rotation over 10 voices): next notes cycle.
        Synth g(48000, 2, 1);
        basicDry(g);
        g.setLayerParam(0, kGlide, 0.5);
        g.setLayerParam(0, kUnison, 1);
        g.setLayerParam(0, kUnisonVoices, 1);
        g.noteOn(48, 1.0);
        g.render(buf.data(), 4800);
        g.noteOn(60, 1.0);
        std::vector<double> traj;
        for (int i = 0; i < 40; ++i) {
            g.render(buf.data(), 240);
            traj.push_back(g.voice(g.allocator().lastAssigned(0)).currentPitch());
        }
        bool monotone = true;
        for (size_t i = 1; i < traj.size(); ++i) monotone &= traj[i] >= traj[i - 1];
        // Exponential (RC) approach: first half covers more distance than second.
        const double d1 = traj[19] - traj[0], d2 = traj[39] - traj[20];
        std::printf("  glide: %.2f -> %.2f -> %.2f semis (exponential approach)\n", traj[0], traj[19], traj[39]);
        CHECK(monotone && d1 > d2 && traj.back() < 60.0 + 1e-9, "glide trajectory wrong");
    }
    {
        Synth s(48000, 2, 1);
        basicDry(s);
        std::vector<double> buf(1);
        s.setLayerParam(0, kFltCutoff, 0.2);
        s.render(buf.data(), 1);
        const double first = s.lastFrame(0).p[kFltCutoff];
        (void)first;
        std::vector<double> track;
        for (int i = 0; i < 960; ++i) {
            s.render(buf.data(), 1);
            track.push_back(s.lastFrame(0).p[kFltCutoff]);
        }
        // frames_ stores the last block's frames at index (n-1); with n = 1 it
        // is frame 0. Check smooth (no single jump larger than 5% of the move).
        double maxStep = 0;
        for (size_t i = 1; i < track.size(); ++i) maxStep = std::max(maxStep, std::fabs(track[i] - track[i - 1]));
        CHECK(maxStep < 0.05 * 0.8, "knob change not smoothed (max step %.3f)", maxStep);
    }
}

static void testPresetsAndMidi() {
    std::printf("[preset/midi] round trip, NRPN, learn, running status\n");
    Program p = Program::init();
    p.name = "Test Brass";
    p.layer[1][kFltCutoff] = 0.123456;
    p.layer[0][kFltKbd] = 1;
    p.settings[kLayerMode] = 2;
    auto q = programFromText(programToText(p));
    CHECK(q && q->name == p.name && std::fabs(q->layer[1][kFltCutoff] - 0.123456) < 1e-9 &&
              q->layer[0][kFltKbd] == 1 && q->settings[kLayerMode] == 2,
          "program round trip");
    ProgramBank bank("U1");
    bank.at(ProgramBank::indexOf(2, 3)) = p;
    ProgramBank bank2;
    CHECK(bank2.fromText(bank.toText()) && bank2.at(19).name == "Test Brass" && bank2.name() == "U1",
          "bank round trip");

    Synth s(48000, 2, 1);
    MidiController m(&s);
    const uint8_t msgs[] = {0x90, 60, 100, 64, 90, 0x80, 60, 0};  // running status note on x2
    m.feed(msgs, sizeof(msgs));
    int gated = 0;
    for (int v = 0; v < kNumVoices; ++v) gated += s.voice(v).gated();
    CHECK(gated == 1, "running status handling (%d gated)", gated);
    // NRPN: layer B cutoff to max.
    const int num = 256 + kFltCutoff;
    m.message(0xB0, 99, num >> 7);
    m.message(0xB0, 98, num & 127);
    m.message(0xB0, 6, 127);
    m.message(0xB0, 38, 127);
    CHECK(std::fabs(s.layerParam(1, kFltCutoff) - 1.0) < 1e-6, "NRPN set %.3f", s.layerParam(1, kFltCutoff));
    // MIDI learn.
    m.armLearn(0, kFltResonance);
    m.message(0xB0, 52, 64);
    CHECK(std::fabs(s.layerParam(0, kFltResonance) - 64 / 127.0) < 1e-6, "learn %.3f", s.layerParam(0, kFltResonance));
    m.message(0xB0, 74, 0);  // default map: cutoff on layer A
    CHECK(s.layerParam(0, kFltCutoff) == 0.0, "default CC map");
}

static void benchmark() {
    std::printf("[bench] ten voices, 4x oversampling\n");
    Synth s(48000, 4, 1);
    Program p = Program::init();
    p.layer[0][kOscAPulse] = 1;
    p.layer[0][kMixOscB] = 0.7;
    p.layer[0][kPmOscBAmt] = 0.3;
    p.layer[0][kPmFilter] = 1;
    p.layer[0][kFltResonance] = 0.6;
    p.layer[0][kFltCutoff] = 0.5;
    s.setProgram(p);
    for (int i = 0; i < 10; ++i) s.noteOn(40 + i * 3, 1.0);
    std::vector<double> buf(48000);
    const auto t0 = std::chrono::steady_clock::now();
    s.render(buf.data(), 48000);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  1 s of audio in %.3f s (%.1f%% of one core)\n", sec, sec * 100);
}

int main() {
    testEnvelope();
    testAliasingSaw();
    testSyncPolyModAliasing(4);
    testSyncPolyModAliasing(8);
    testAudioRatePolyMod();
    testSelfOscillation();
    testFiltersDiffer();
    testAllocation();
    testVintageAndTuning();
    testPerformanceControls();
    testStability();
    testPresetsAndMidi();
    benchmark();
    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
