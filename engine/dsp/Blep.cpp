#include "Blep.h"

#include <cmath>
#include <map>
#include <memory>
#include <mutex>

#include "DspMath.h"

namespace p10 {

const BlepTables& BlepTables::forOversample(int os, double baseRate) {
    static std::mutex m;
    static std::map<std::pair<int, int>, std::unique_ptr<BlepTables>> cache;
    std::lock_guard<std::mutex> lock(m);
    auto key = std::make_pair(os, static_cast<int>(baseRate));
    auto& slot = cache[key];
    if (!slot) slot.reset(new BlepTables(os, baseRate));
    return *slot;
}

BlepTables::BlepTables(int os, double baseRate) {
    os = std::max(1, os);
    half_ = kBaseHalfWidth * os;
    const int width = 2 * half_;
    const double fsOs = baseRate * os;
    // Cutoff in cycles per oversampled sample; never above 0.45.
    const double fc = std::min(0.45, kCutoffHz / fsOs);
    const int n = width * kPhases + 1;
    const double dt = 1.0 / kPhases;
    std::vector<double> h(n);
    for (int i = 0; i < n; ++i) {
        const double t = -half_ + i * dt;
        const double x = 2.0 * fc * t;
        const double sinc = std::fabs(x) < 1e-12 ? 1.0 : std::sin(kPi * x) / (kPi * x);
        const double w = (t + half_) / width;
        const double win = 0.35875 - 0.48829 * std::cos(kTwoPi * w) + 0.14128 * std::cos(2 * kTwoPi * w) -
                           0.01168 * std::cos(3 * kTwoPi * w);
        h[i] = 2.0 * fc * sinc * win;
    }
    // Integrate impulse -> band-limited step (trapezoidal), normalise to 1.
    std::vector<double> stepTab(n), rampTab(n);
    double acc = 0.0;
    for (int i = 1; i < n; ++i) {
        acc += 0.5 * (h[i - 1] + h[i]) * dt;
        stepTab[i] = acc;
    }
    for (auto& v : stepTab) v /= acc;
    // Integrate step -> band-limited ramp.
    acc = 0.0;
    for (int i = 1; i < n; ++i) {
        acc += 0.5 * (stepTab[i - 1] + stepTab[i]) * dt;
        rampTab[i] = acc;
    }
    // The smooth band-limited curves are tabulated; the naive step/ramp is
    // subtracted analytically at lookup time so interpolation never straddles
    // the discontinuity. The ramp is corrected so its residual is exactly zero
    // at the far edge (removes the small truncation offset of the window).
    const double rampEnd = rampTab[n - 1] - half_;
    for (int i = 0; i < n; ++i) rampTab[i] -= rampEnd * static_cast<double>(i) / (n - 1);
    blep_ = std::move(stepTab);
    blamp_ = std::move(rampTab);
}

double BlepTables::step(double t) const {
    if (t <= -half_ || t >= half_) return 0.0;
    return interp(blep_, t) - (t >= 0.0 ? 1.0 : 0.0);
}

double BlepTables::ramp(double t) const {
    if (t <= -half_ || t >= half_) return 0.0;
    return interp(blamp_, t) - (t > 0.0 ? t : 0.0);
}

double BlepTables::interp(const std::vector<double>& tab, double t) const {
    const double x = (t + half_) * kPhases;
    int i = static_cast<int>(x);
    if (i >= static_cast<int>(tab.size()) - 1) i = static_cast<int>(tab.size()) - 2;
    const double f = x - i;
    return tab[i] + (tab[i + 1] - tab[i]) * f;
}

void BlepBuffer::setTables(const BlepTables* t) {
    t_ = t;
    int size = 1;
    while (size < t->width() + 1) size <<= 1;
    buf_.assign(size, 0.0);
    mask_ = size - 1;
    pos_ = 0;
}

void BlepBuffer::reset() {
    std::fill(buf_.begin(), buf_.end(), 0.0);
    pos_ = 0;
}

}  // namespace p10
