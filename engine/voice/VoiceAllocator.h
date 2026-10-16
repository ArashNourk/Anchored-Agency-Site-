// Voice assignment over the ten physical voices.
//
// Poly assignment (per layer, over that layer's voice pool):
//   1. a key goes back to the voice that last played it, so repeated notes
//      meet that voice's envelopes where they are               [A-VAL-02]
//   2. otherwise a free (un-gated) voice is chosen by the assignment policy:
//      rotate to the next free voice after the last one assigned (default),
//      oldest-released, or lowest index                          [A-VAL-01]
//   3. otherwise the oldest gated voice (or quietest) is stolen  [A-VAL-03]
// Unison (mono, per layer): N voices of the pool play the held key chosen by
//   key priority (low / high / last); with retrigger off, changing key while
//   others are held glides/legatos without restarting envelopes.
// Layer pools: single = all voices to layer A; stack/split = fixed halves
//   (0-4 layer A, 5-9 layer B) by default, or one shared dynamic pool
//                                                       [A-VAL-04, A-VAL-05]
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "../model/Calibration.h"
#include "../model/Params.h"
#include "Voice.h"

namespace p10 {

class VoiceAllocator {
public:
    void configure(std::array<Voice, kNumVoices>* voices, const CalibrationModel* cal);

    // Layer settings snapshot (unison, key priority etc.) taken from patches.
    void setLayerConfig(int layer, const LayerPatch& patch);
    void setLayerMode(int mode, int splitPoint, bool upperIsB);

    void noteOn(int note, double velocity);
    void noteOff(int note);
    void setSustain(bool on);
    void allNotesOff();

    // For tests / metering: which voice played the most recent note on layer.
    int lastAssigned(int layer) const { return lastAssigned_[layer]; }
    std::vector<int> pool(int layer) const;

private:
    struct LayerCfg {
        bool unison = false;
        int unisonVoices = 10;
        double unisonDetune = 0.0;
        int keyPriority = 2;
        bool retrigger = true;
    };
    void layerNoteOn(int layer, int note, double velocity);
    void layerNoteOff(int layer, int note);
    void polyNoteOn(int layer, int note, double velocity);
    void monoUpdate(int layer, bool newKey, double velocity);
    int pickMonoKey(int layer) const;
    std::vector<int> layersForNote(int note) const;

    std::array<Voice, kNumVoices>* voices_ = nullptr;
    const CalibrationModel* cal_ = nullptr;
    LayerCfg cfg_[2];
    int mode_ = 0, splitPoint_ = 60;
    bool upperIsB_ = true;
    bool sustain_ = false;
    uint64_t stamp_ = 1;
    int lastAssigned_[2] = {-1, -1};
    int rotor_[2] = {-1, -1};

    struct Held {
        int note;
        double velocity;
        uint64_t order;
    };
    std::vector<Held> held_[2];       // physically held keys per layer
    std::vector<int> sustained_[2];   // released keys kept by the pedal
    int monoNote_[2] = {-1, -1};
};

}  // namespace p10
