// MIDI / performance layer. Turns a raw MIDI byte stream into Synth calls:
// notes + velocity, pitch bend (14-bit), mod wheel (14-bit with CC 33),
// sustain, channel aftertouch, program change, the parameter CC map,
// NRPN (14-bit, every parameter of both layers and the program settings) and
// MIDI learn. MIDI learn is an addition on top of the fixed map; it never
// changes how the synth itself responds to its panel.
//
// NRPN numbering: layer A params 0..255, layer B params 256..511,
// program settings 512.. (index = enum value in model/Params.h).
#pragma once

#include <cstdint>
#include <functional>
#include <map>

#include "../Synth.h"

namespace p10 {

class MidiController {
public:
    explicit MidiController(Synth* synth) : synth_(synth) {}

    void setChannel(int ch) { channel_ = ch; }  // 0..15, or -1 for omni
    void setEditLayer(int layer) { editLayer_ = layer; }  // layer CCs address

    // Feed raw bytes (running status supported, realtime bytes ignored).
    void feed(const uint8_t* data, int len);
    // Feed one complete short message.
    void message(uint8_t status, uint8_t d1, uint8_t d2);

    // MIDI learn: the next CC received is bound to this parameter.
    void armLearn(int layer, int paramId);
    void cancelLearn() { learnArmed_ = false; }
    void clearLearned() { learned_.clear(); }

    std::function<void(int program)> onProgramChange;

    struct Binding {
        int layer;
        int param;
    };

private:
    void handleCC(int cc, int value);
    void applyNormalized(int layer, int param, double norm);
    void applyNrpn(int number, int value14);

    Synth* synth_;
    int channel_ = -1;
    int editLayer_ = 0;
    uint8_t running_ = 0;
    uint8_t buf_[2]{};
    int have_ = 0;
    int modMsb_ = 0;
    int nrpnMsb_ = 0, nrpnLsb_ = 0, dataMsb_ = 0;
    bool nrpnActive_ = false;
    bool learnArmed_ = false;
    Binding learnTarget_{0, 0};
    std::map<int, Binding> learned_;
};

}  // namespace p10
