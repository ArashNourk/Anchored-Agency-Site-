// Core numeric helpers shared by all DSP models.
// All audio-path state is 64-bit floating point.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace p10 {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double clamp(double x, double lo, double hi) { return std::min(std::max(x, lo), hi); }
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

inline double semisToRatio(double semis) { return std::exp2(semis * (1.0 / 12.0)); }
inline double midiNoteToHz(double note, double a4 = 440.0) { return a4 * std::exp2((note - 69.0) / 12.0); }

// Rational tanh approximation (max abs error ~2e-4 over the real line, monotone,
// exactly saturating at +-1). Used inside the per-sample nonlinear loops where
// std::tanh is too costly at oversampled rates for ten voices.
inline double fastTanh(double x) {
    if (x > 4.97) return 1.0;
    if (x < -4.97) return -1.0;
    const double x2 = x * x;
    const double a = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2)));
    const double b = 135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0));
    return std::clamp(a / b, -1.0, 1.0);
}

// tanh(x)/x, the secant gain used to linearise a tanh element around its
// operating point when solving zero-delay-feedback loops.
inline double tanhSecant(double x) {
    if (std::fabs(x) < 1e-6) return 1.0 - x * x / 3.0;
    return fastTanh(x) / x;
}

// Smooth asymmetric saturator: tanh with a small even-order bias term.
// asym = 0 gives a pure odd (tanh) curve.
inline double asymSat(double x, double asym) {
    const double y = fastTanh(x + asym);
    const double y0 = fastTanh(asym);
    return y - y0;
}

// Deterministic, cheap, high-quality PRNG (xoshiro256**-style splitmix seeded).
class Rng {
public:
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) { reseed(seed); }
    void reseed(uint64_t seed) {
        for (auto& s : s_) {
            seed += 0x9E3779B97F4A7C15ull;
            uint64_t z = seed;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            s = z ^ (z >> 31);
        }
        haveGauss_ = false;
    }
    uint64_t next() {
        const uint64_t result = rotl(s_[1] * 5, 7) * 9;
        const uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }
    // Uniform in [0, 1).
    double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    // Uniform in [-1, 1).
    double bipolar() { return uniform() * 2.0 - 1.0; }
    // Standard normal (Box-Muller, cached pair).
    double gauss() {
        if (haveGauss_) {
            haveGauss_ = false;
            return gauss2_;
        }
        double u1 = uniform();
        if (u1 < 1e-300) u1 = 1e-300;
        const double u2 = uniform();
        const double r = std::sqrt(-2.0 * std::log(u1));
        gauss2_ = r * std::sin(kTwoPi * u2);
        haveGauss_ = true;
        return r * std::cos(kTwoPi * u2);
    }

private:
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    uint64_t s_[4]{};
    bool haveGauss_ = false;
    double gauss2_ = 0.0;
};

// One-pole parameter smoother (exponential), used for knob de-zippering.
class Smoother {
public:
    void setTime(double seconds, double sampleRate) {
        coef_ = seconds <= 0.0 ? 1.0 : 1.0 - std::exp(-1.0 / (seconds * sampleRate));
    }
    void reset(double v) { y_ = v; }
    double process(double target) {
        y_ += (target - y_) * coef_;
        return y_;
    }
    double value() const { return y_; }

private:
    double coef_ = 1.0;
    double y_ = 0.0;
};

// First-order DC blocking high-pass (AC coupling capacitor model).
class DcBlocker {
public:
    void setCutoff(double hz, double sampleRate) { r_ = std::exp(-kTwoPi * hz / sampleRate); }
    void reset() { x1_ = y1_ = 0.0; }
    double process(double x) {
        const double y = x - x1_ + r_ * y1_;
        x1_ = x;
        y1_ = y;
        return y;
    }

private:
    double r_ = 0.9999;
    double x1_ = 0.0, y1_ = 0.0;
};

// Ornstein-Uhlenbeck process: mean-reverting random drift with a given
// correlation time and stationary standard deviation. Advanced at control rate.
class OuProcess {
public:
    void configure(double tauSeconds, double sigma, double stepSeconds) {
        tau_ = std::max(tauSeconds, 1e-6);
        sigma_ = sigma;
        a_ = std::exp(-stepSeconds / tau_);
        b_ = sigma_ * std::sqrt(std::max(0.0, 1.0 - a_ * a_));
    }
    void randomise(Rng& rng) { x_ = sigma_ * rng.gauss(); }
    double step(Rng& rng) {
        x_ = a_ * x_ + b_ * rng.gauss();
        return x_;
    }
    double value() const { return x_; }

private:
    double tau_ = 1.0, sigma_ = 0.0, a_ = 1.0, b_ = 0.0, x_ = 0.0;
};

}  // namespace p10
