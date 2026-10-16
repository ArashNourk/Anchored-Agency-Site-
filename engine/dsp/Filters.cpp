#include "Filters.h"

#include <cmath>

#include "DspMath.h"

namespace p10 {

namespace {
// Fixed-point refinements of the secant linearisation per sample.
constexpr int kIterations = 2;
}  // namespace

double tptGain(double hz, double fs) {
    const double w = kPi * std::min(hz, 0.48 * fs) / fs;
    return std::tan(w);
}

double resonanceLoopGain(double knob, const FilterModelCal& cal, double cutoffHz) {
    knob = clamp(knob, 0.0, 1.0);
    const double onset = clamp(cal.resOnsetKnob, 0.05, 0.999);
    double k;
    if (knob < onset)
        k = 4.0 * std::pow(knob / onset, cal.resCurve);
    else
        k = 4.0 + (cal.kMax - 4.0) * (knob - onset) / (1.0 - onset);
    // Loop-gain loss from parasitic poles/slew at high cutoff (absolute Hz,
    // independent of the oversampling factor).
    const double r = cutoffHz / 20000.0;
    return k / (1.0 + cal.hfResDamp * r * r);
}

// ---------------------------------------------------------------------------
// SSM2040 / SSI2140: OTA follower-integrator cascade.
double Ssm2040Filter::process(double x, const FilterControl& c) {
    const double g0 = c.g;
    const double D = c.stageDrive;
    const double Df = c.fbDrive;
    const double k = c.k;
    const double xin = fastTanh(x * c.inputGain) * (1.0 + c.passbandComp * k);

    double g[4];
    for (int i = 0; i < 4; ++i) g[i] = g0 * mis_[i];

    // Operating-point estimates from the previous sample.
    double u[4], y[4];
    for (int i = 0; i < 4; ++i) y[i] = y_[i];
    double y4 = y_[3];
    u[0] = xin - k * y4;
    u[1] = y[0];
    u[2] = y[1];
    u[3] = y[2];

    for (int iter = 0; iter < kIterations; ++iter) {
        double G[4], S[4];
        for (int i = 0; i < 4; ++i) {
            const double a = tanhSecant(D * (u[i] - y[i]));
            const double ga = g[i] * a;
            G[i] = ga / (1.0 + ga);
            S[i] = s_[i] / (1.0 + ga);
        }
        // Algebraic soft limiter in the feedback path: y / sqrt(1 + (Df y)^2).
        const double b = 1.0 / std::sqrt(1.0 + (Df * y4) * (Df * y4));
        const double Gt = G[0] * G[1] * G[2] * G[3];
        const double St = ((S[0] * G[1] + S[1]) * G[2] + S[2]) * G[3] + S[3];
        y4 = (Gt * xin + St) / (1.0 + k * b * Gt);
        u[0] = xin - k * b * y4;
        y[0] = G[0] * u[0] + S[0];
        u[1] = y[0];
        y[1] = G[1] * u[1] + S[1];
        u[2] = y[1];
        y[2] = G[2] * u[2] + S[2];
        u[3] = y[2];
        y[3] = G[3] * u[3] + S[3];
        y4 = y[3];
    }
    for (int i = 0; i < 4; ++i) {
        s_[i] = 2.0 * y[i] - s_[i];
        y_[i] = y[i];
    }
    return y4 * c.outputGain;
}

// ---------------------------------------------------------------------------
// CEM3320: current-mode cells with separate input/state nonlinearities.
double Cem3320Filter::process(double x, const FilterControl& c) {
    const double g0 = c.g;
    const double D = c.stageDrive;
    const double Df = c.fbDrive;
    const double k = c.k;
    const double xin = fastTanh(x * c.inputGain) * (1.0 + c.passbandComp * k);

    double g[4];
    for (int i = 0; i < 4; ++i) g[i] = g0 * mis_[i];

    double u[4], y[4];
    for (int i = 0; i < 4; ++i) y[i] = y_[i];
    double y4 = y_[3];
    u[0] = xin - k * fastTanh(Df * y4) / Df;
    u[1] = y[0];
    u[2] = y[1];
    u[3] = y[2];

    for (int iter = 0; iter < kIterations; ++iter) {
        double G[4], S[4];
        for (int i = 0; i < 4; ++i) {
            const double au = tanhSecant(D * u[i]);
            const double ay = tanhSecant(D * y[i]);
            const double den = 1.0 + g[i] * ay;
            G[i] = g[i] * au / den;
            S[i] = s_[i] / den;
        }
        const double b = tanhSecant(Df * y4);
        const double Gt = G[0] * G[1] * G[2] * G[3];
        const double St = ((S[0] * G[1] + S[1]) * G[2] + S[2]) * G[3] + S[3];
        y4 = (Gt * xin + St) / (1.0 + k * b * Gt);
        u[0] = xin - k * b * y4;
        y[0] = G[0] * u[0] + S[0];
        u[1] = y[0];
        y[1] = G[1] * u[1] + S[1];
        u[2] = y[1];
        y[2] = G[2] * u[2] + S[2];
        u[3] = y[2];
        y[3] = G[3] * u[3] + S[3];
        y4 = y[3];
    }
    for (int i = 0; i < 4; ++i) {
        s_[i] = 2.0 * y[i] - s_[i];
        y_[i] = y[i];
    }
    return y4 * c.outputGain;
}

}  // namespace p10
