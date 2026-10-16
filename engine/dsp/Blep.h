// Band-limited step (BLEP) and ramp (BLAMP) residual tables plus a correction
// accumulator. The oscillator models generate their naive (trivial) waveform and
// insert residual corrections at the exact sub-sample time of every
// discontinuity (reset, comparator flip, sync). The kernel is a Blackman-Harris
// windowed sinc defined in *absolute* frequency: flat to ~20 kHz, cut off at
// kCutoffHz, and essentially empty above kCutoffHz + kTransitionHz. Keeping
// the oscillator output free of energy far above the audio band matters as much
// as the BLEP itself: the downstream nonlinear stages (mixer, filter cells,
// VCA) would otherwise fold that energy back at the oversampled rate.
// The output is delayed by halfWidth() samples (linear phase).
#pragma once

#include <vector>

namespace p10 {

class BlepTables {
public:
    static constexpr double kCutoffHz = 38000.0;
    static constexpr int kBaseHalfWidth = 6;  // kernel half width in output-rate samples
    static constexpr int kPhases = 256;       // table resolution per (oversampled) sample

    // Tables for a given oversampling factor (built once, cached).
    static const BlepTables& forOversample(int os, double baseRate = 48000.0);

    int halfWidth() const { return half_; }
    int width() const { return 2 * half_; }
    // Residual of a unit step at t = 0, evaluated at t (samples).
    double step(double t) const;
    // Residual of a unit slope change (ramp) at t = 0.
    double ramp(double t) const;

private:
    BlepTables(int os, double baseRate);
    double interp(const std::vector<double>& tab, double t) const;
    int half_;
    std::vector<double> blep_, blamp_;
};

// Accumulates naive samples plus band-limiting corrections, emits the result
// halfWidth() samples later.
class BlepBuffer {
public:
    void setTables(const BlepTables* t);
    void reset();

    // Insert a step of height `amp` that happened `d` samples before the
    // current sample instant (0 <= d <= 1).
    void addStep(double d, double amp) {
        const int h = t_->halfWidth(), w = t_->width();
        for (int k = 0; k < w; ++k) buf_[(pos_ + k) & mask_] += amp * t_->step(k - h + d);
    }
    // Insert a slope change of `amp` (units per sample) `d` samples ago.
    void addRamp(double d, double amp) {
        const int h = t_->halfWidth(), w = t_->width();
        for (int k = 0; k < w; ++k) buf_[(pos_ + k) & mask_] += amp * t_->ramp(k - h + d);
    }
    // Push the naive value for the current sample and pop the delayed output.
    double push(double naive) {
        buf_[(pos_ + t_->halfWidth()) & mask_] += naive;
        const double out = buf_[pos_];
        buf_[pos_] = 0.0;
        pos_ = (pos_ + 1) & mask_;
        return out;
    }

private:
    const BlepTables* t_ = nullptr;
    std::vector<double> buf_;
    int mask_ = 0;
    int pos_ = 0;
};

}  // namespace p10
