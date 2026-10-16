// The two switchable Prophet filter models. They are topologically different,
// not one ladder with two EQ curves:
//
// Ssm2040Filter (Rev1/Rev2 character, SSM2040 / Rossum SSI2140):
//   Four OTA integrator cells. Each cell's transconductor sees the
//   *difference* between its input and its own output (unity-gain follower
//   integrator), so the nonlinearity is tanh(drive*(u - y)). The resonance
//   loop is closed through a soft (algebraic) limiter, and part of the
//   passband loss caused by resonance is compensated at the input, as the
//   Rev1/2 input/feedback resistor network does. Result: rounder, fuller bass
//   at high resonance, gradual, "squelchy" onset of oscillation.
//
// Cem3320Filter (Rev3 character, CEM3320):
//   Four current-mode cells where the input and the state are each passed
//   through their own differential-pair nonlinearity: dy = g(tanh(u) - tanh(y))
//   (transistor-ladder style). Resonance feedback goes through an
//   uncompensated VCA with a harder tanh limiter. Result: brighter, more
//   pronounced passband loss as resonance rises, tighter, harder-edged
//   self-oscillation.
//
// Both are solved with zero-delay feedback (trapezoidal integration): the
// nonlinear elements are linearised with their secant gain around the
// current operating point and the resulting linear loop is solved exactly;
// this is iterated (two refinement passes) every sample. They are intended to
// run at the voice's oversampled rate.
#pragma once

#include "../model/Calibration.h"

namespace p10 {

struct FilterControl {
    double g;            // prewarped integrator gain tan(pi fc / fs), all modulation included
    double k;            // loop gain (4.0 ~ linear self-oscillation threshold)
    double inputGain;
    double outputGain;
    double passbandComp;
    double fbDrive;
    double stageDrive;
};

class Ssm2040Filter {
public:
    void setSampleRate(double fs) { fs_ = fs; }
    void setStageMismatch(const double m[4]) {
        for (int i = 0; i < 4; ++i) mis_[i] = 1.0 + m[i];
    }
    void reset() {
        for (double& v : s_) v = 0.0;
        for (double& v : y_) v = 0.0;
    }
    double process(double x, const FilterControl& c);

private:
    double fs_ = 192000.0;
    double s_[4]{}, y_[4]{};
    double mis_[4]{1, 1, 1, 1};
};

class Cem3320Filter {
public:
    void setSampleRate(double fs) { fs_ = fs; }
    void setStageMismatch(const double m[4]) {
        for (int i = 0; i < 4; ++i) mis_[i] = 1.0 + m[i];
    }
    void reset() {
        for (double& v : s_) v = 0.0;
        for (double& v : y_) v = 0.0;
    }
    double process(double x, const FilterControl& c);

private:
    double fs_ = 192000.0;
    double s_[4]{}, y_[4]{};
    double mis_[4]{1, 1, 1, 1};
};

// Prewarped integrator gain for a TPT one-pole at cutoff hz.
double tptGain(double hz, double fs);

// Resonance knob -> loop gain. Progressive below the oscillation onset point,
// then rising linearly to kMax.
double resonanceLoopGain(double knob, const FilterModelCal& cal, double cutoffHz);

}  // namespace p10
