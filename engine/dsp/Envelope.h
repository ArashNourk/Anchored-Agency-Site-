// Analog-style ADSR modelled as an RC control circuit:
//  * Attack charges a capacitor toward a target above the trip level
//    (CalibrationModel::envAttackTarget). The stage ends at the trip level, so
//    the attack is a slightly convex exponential segment, not a line.
//  * Decay and release discharge exponentially toward sustain / zero.
//  * Every stage starts from the capacitor's current voltage, so retriggering
//    a sounding voice or stealing it continues from where it is (no reset to
//    zero), which is what produces the hardware's repeated-note feel.
// Times are panel knob positions mapped through the calibratable taper; the
// per-voice Vintage time scale multiplies them.
#pragma once

#include <cmath>

namespace p10 {

struct EnvTimes {
    double attackSec, decaySec, sustain, releaseSec;
};

class AnalogEnvelope {
public:
    enum class Stage { Idle, Attack, Decay, Release };

    void setSampleRate(double fs) { fs_ = fs; }
    void setShape(double attackTarget, double decayTauDivisor) {
        attackTarget_ = attackTarget;
        tauDiv_ = decayTauDivisor;
    }

    void gateOn() { stage_ = Stage::Attack; }
    void gateOff() {
        if (stage_ != Stage::Idle) stage_ = Stage::Release;
    }
    void hardReset() {
        stage_ = Stage::Idle;
        v_ = 0.0;
    }

    // Times may change every sample (smoothed knobs); coefficients are cheap.
    double process(const EnvTimes& t) {
        switch (stage_) {
            case Stage::Idle:
                return v_;
            case Stage::Attack: {
                // Time from 0 to trip level equals attackSec.
                const double tgt = attackTarget_;
                const double tau = t.attackSec / std::log(tgt / (tgt - 1.0));
                v_ += (tgt - v_) * coef(tau);
                if (v_ >= 1.0) {
                    v_ = 1.0;
                    stage_ = Stage::Decay;
                }
                return v_;
            }
            case Stage::Decay:
                v_ += (t.sustain - v_) * coef(t.decaySec / tauDiv_);
                return v_;
            case Stage::Release:
                v_ += (0.0 - v_) * coef(t.releaseSec / tauDiv_);
                if (v_ < 1e-7) {
                    v_ = 0.0;
                    stage_ = Stage::Idle;
                }
                return v_;
        }
        return v_;
    }

    double value() const { return v_; }
    Stage stage() const { return stage_; }
    bool active() const { return stage_ != Stage::Idle; }

private:
    double coef(double tau) const {
        if (tau <= 1e-9) return 1.0;
        return 1.0 - std::exp(-1.0 / (tau * fs_));
    }
    double fs_ = 48000.0;
    double attackTarget_ = 1.3;
    double tauDiv_ = 4.6;
    double v_ = 0.0;
    Stage stage_ = Stage::Idle;
};

}  // namespace p10
