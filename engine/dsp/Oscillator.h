// CEM3340-style VCO model.
//
// The 3340 core is a linear ramp (capacitor charged by an exponential current
// source) reset by a comparator. Sawtooth, pulse (ramp vs. PW comparator) and
// triangle (rectified ramp) are all derived from that one ramp, so they share
// phase, and the pulse edges and hard-sync resets are tied to the ramp exactly
// as on the chip. All discontinuities (ramp reset, comparator flips, sync
// resets, triangle corners) are inserted with sub-sample accuracy as BLEP /
// BLAMP corrections, including under audio-rate PW and frequency modulation.
//
// Analog deviations modelled here:
//  * ramp curvature (non-ideal current source / capacitor leakage) -> bowed saw
//  * finite reset (retrace) time -> high-frequency flattening, applied by the
//    caller through effectiveFrequency()
//  * shared phase between all waveform outputs, free-running phase (no note-on
//    reset)
#pragma once

#include <algorithm>
#include <cmath>

#include "Blep.h"

namespace p10 {

class Cem3340 {
public:
    struct Out {
        double saw = 0.0, pulse = 0.0, tri = 0.0;
    };

    // Must be called before use: selects the BLEP kernel for the rate.
    void setOversample(int os, double baseRate);
    void reset(double phase);
    void setCurvature(double c) { curv_ = c; }
    void setPulsePolarity(bool highAtRampEnd) { highAtEnd_ = highAtRampEnd; }

    // Advance one sample.
    //   dt    : phase increment in cycles/sample (0 < dt < 0.5)
    //   duty  : pulse duty cycle at the end of this sample (0..1)
    //   syncD : if >= 0, a hard-sync reset happened syncD samples before the
    //           end of this sample (0 <= syncD < 1)
    // Returns the time (samples before end of sample) of this oscillator's own
    // ramp reset, or -1 if it did not reset naturally. Used to sync a slave.
    double process(double dt, double duty, double syncD, Out& out);

    // Free-run without output (used while a voice is silent).
    void advanceSilently(double cycles);

    double phase() const { return phase_; }

    // Frequency actually produced for a nominal frequency, given the ramp
    // retrace time that is not compensated by HF trim (seconds).
    static double effectiveFrequency(double hz, double residualResetSec) {
        return hz / (1.0 + hz * residualResetSec);
    }

private:
    double sawValue(double p) const {
        const double s = 2.0 * p - 1.0;
        return s + curv_ * (s * s - 1.0 / 3.0);
    }
    double sawSlope(double p, double dt) const {  // per sample
        const double s = 2.0 * p - 1.0;
        return (1.0 + 2.0 * curv_ * s) * 2.0 * dt;
    }
    static double triValue(double p) { return 1.0 - 2.0 * std::fabs(2.0 * p - 1.0); }
    double threshold(double duty) const { return highAtEnd_ ? 1.0 - duty : duty; }
    double pulseValue(bool cmp) const {
        const double v = cmp ? 1.0 : -1.0;
        return highAtEnd_ ? v : -v;
    }

    // Move from time ta to tb (fractions of the current sample), emitting
    // events. Phase moves linearly at dt per sample; threshold is interpolated.
    void advance(double ta, double tb, double dt);
    void comparator(double ta, double tb, double pa, double pb);
    double thresholdAt(double t) const { return th0_ + (th1_ - th0_) * t; }
    void emitStep(BlepBuffer& b, double t, double amp) { b.addStep(1.0 - t, amp); }
    void emitRamp(BlepBuffer& b, double t, double amp) { b.addRamp(1.0 - t, amp); }

    double phase_ = 0.0;
    double curv_ = 0.0;
    bool highAtEnd_ = true;
    bool cmp_ = false;  // comparator state: phase >= threshold
    double th0_ = 0.5, th1_ = 0.5;
    double prevThreshold_ = 0.5;
    double ownResetT_ = -1.0;
    BlepBuffer saw_, pulse_, tri_;
    int halfWidth_ = 0;
};

}  // namespace p10
