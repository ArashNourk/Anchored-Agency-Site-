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
        if (attackTarget != attackTarget_) {
            attackTarget_ = attackTarget;
            attackTauPerSec_ = 1.0 / std::log(attackTarget / (attackTarget - 1.0));
        }
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
                v_ += (tgt - v_) * attackCache_.get(t.attackSec * attackTauPerSec_, fs_);
                if (v_ >= 1.0) {
                    v_ = 1.0;
                    stage_ = Stage::Decay;
                }
                return v_;
            }
            case Stage::Decay:
                v_ += (t.sustain - v_) * decayCache_.get(t.decaySec / tauDiv_, fs_);
                return v_;
            case Stage::Release:
                v_ += (0.0 - v_) * releaseCache_.get(t.releaseSec / tauDiv_, fs_);
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
    // One-pole coefficient for a time constant; times only change when a knob
    // moves, so the exp() is cached.
    struct CoefCache {
        double tau = -1.0, fs = 0.0, coef = 1.0;
        double get(double t, double sampleRate) {
            if (t != tau || sampleRate != fs) {
                tau = t;
                fs = sampleRate;
                coef = t <= 1e-9 ? 1.0 : 1.0 - std::exp(-1.0 / (t * sampleRate));
            }
            return coef;
        }
    };
    CoefCache attackCache_, decayCache_, releaseCache_;
    double fs_ = 48000.0;
    double attackTarget_ = 1.3;
    double attackTauPerSec_ = 1.0 / std::log(1.3 / 0.3);
    double tauDiv_ = 4.6;
    double v_ = 0.0;
    Stage stage_ = Stage::Idle;
};

}  // namespace p10
