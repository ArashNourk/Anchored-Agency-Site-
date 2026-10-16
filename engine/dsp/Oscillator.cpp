#include "Oscillator.h"

#include <cmath>

namespace p10 {

void Cem3340::setOversample(int os, double baseRate) {
    const BlepTables* t = &BlepTables::forOversample(os, baseRate);
    saw_.setTables(t);
    pulse_.setTables(t);
    tri_.setTables(t);
    halfWidth_ = t->halfWidth();
}

void Cem3340::reset(double phase) {
    phase_ = phase - std::floor(phase);
    saw_.reset();
    pulse_.reset();
    tri_.reset();
    cmp_ = phase_ >= prevThreshold_;
    // Pre-fill the linear-phase delay so the first outputs are continuous.
    for (int i = 0; i < halfWidth_; ++i) {
        saw_.push(sawValue(phase_));
        pulse_.push(pulseValue(cmp_));
        tri_.push(triValue(phase_));
    }
}

void Cem3340::advanceSilently(double cycles) { reset(phase_ + cycles); }

double Cem3340::process(double dt, double duty, double syncD, Out& out) {
    th0_ = prevThreshold_;
    th1_ = threshold(std::clamp(duty, 0.0, 1.0));
    ownResetT_ = -1.0;

    if (syncD >= 0.0 && syncD < 1.0) {
        const double ts = 1.0 - syncD;
        advance(0.0, ts, dt);
        // Hard sync: the master's reset pulse discharges this ramp.
        const double p = phase_;
        const double s0 = sawValue(0.0), s1 = sawValue(p);
        emitStep(saw_, ts, s0 - s1);
        emitRamp(saw_, ts, sawSlope(0.0, dt) - sawSlope(p, dt));
        emitStep(tri_, ts, triValue(0.0) - triValue(p));
        const double triSlopeBefore = (p < 0.5 ? 4.0 : -4.0) * dt;
        emitRamp(tri_, ts, 4.0 * dt - triSlopeBefore);
        phase_ = 0.0;
        const bool c = 0.0 >= thresholdAt(ts);
        if (c != cmp_) {
            emitStep(pulse_, ts, pulseValue(c) - pulseValue(cmp_));
            cmp_ = c;
        }
        advance(ts, 1.0, dt);
    } else {
        advance(0.0, 1.0, dt);
    }
    prevThreshold_ = th1_;

    out.saw = saw_.push(sawValue(phase_));
    out.pulse = pulse_.push(pulseValue(cmp_));
    out.tri = tri_.push(triValue(phase_));
    return ownResetT_ >= 0.0 ? 1.0 - ownResetT_ : -1.0;
}

void Cem3340::advance(double ta, double tb, double dt) {
    double t = ta;
    int guard = 0;
    while (t < tb && guard++ < 4) {
        const double pa = phase_;
        const double pEnd = pa + dt * (tb - t);
        bool wrap = false;
        double segEnd = tb;
        double pb = pEnd;
        if (pEnd >= 1.0) {
            wrap = true;
            segEnd = std::min(tb, t + (1.0 - pa) / dt);
            pb = 1.0;
        }
        // Triangle corner at mid-ramp.
        if (pa < 0.5 && pb >= 0.5) emitRamp(tri_, t + (0.5 - pa) / dt, -8.0 * dt);
        comparator(t, segEnd, pa, pb);
        if (!wrap) {
            phase_ = pEnd;
            t = tb;
            break;
        }
        // Natural ramp reset (the comparator on the chip's timing capacitor).
        const double sEnd = sawValue(1.0), s0 = sawValue(0.0);
        emitStep(saw_, segEnd, s0 - sEnd);
        emitRamp(saw_, segEnd, sawSlope(0.0, dt) - sawSlope(1.0, dt));
        emitRamp(tri_, segEnd, 8.0 * dt);
        const bool c = 0.0 >= thresholdAt(segEnd);
        if (c != cmp_) {
            emitStep(pulse_, segEnd, pulseValue(c) - pulseValue(cmp_));
            cmp_ = c;
        }
        phase_ = 0.0;
        ownResetT_ = segEnd;
        t = segEnd;
    }
}

void Cem3340::comparator(double ta, double tb, double pa, double pb) {
    const double ea = pa - thresholdAt(ta);
    const double eb = pb - thresholdAt(tb);
    const bool ca = ea >= 0.0;
    if (ca != cmp_) {
        emitStep(pulse_, ta, pulseValue(ca) - pulseValue(cmp_));
        cmp_ = ca;
    }
    const bool cb = eb >= 0.0;
    if (cb != cmp_) {
        const double denom = ea - eb;
        double tc = denom != 0.0 ? ta + (tb - ta) * (ea / denom) : tb;
        tc = std::clamp(tc, ta, tb);
        emitStep(pulse_, tc, pulseValue(cb) - pulseValue(cmp_));
        cmp_ = cb;
    }
}

}  // namespace p10
