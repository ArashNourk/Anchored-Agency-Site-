#include "MidiController.h"

#include <cmath>

namespace p10 {

namespace {
int dataBytesFor(uint8_t status) {
    switch (status & 0xF0) {
        case 0xC0:
        case 0xD0:
            return 1;
        default:
            return 2;
    }
}
}  // namespace

void MidiController::feed(const uint8_t* data, int len) {
    for (int i = 0; i < len; ++i) {
        const uint8_t b = data[i];
        if (b >= 0xF8) continue;  // realtime
        if (b >= 0xF0) {          // system common / sysex: cancels running status
            running_ = 0;
            have_ = 0;
            continue;
        }
        if (b & 0x80) {
            running_ = b;
            have_ = 0;
            continue;
        }
        if (!running_) continue;
        buf_[have_++] = b;
        if (have_ == dataBytesFor(running_)) {
            message(running_, buf_[0], have_ > 1 ? buf_[1] : 0);
            have_ = 0;
        }
    }
}

void MidiController::message(uint8_t status, uint8_t d1, uint8_t d2) {
    const int ch = status & 0x0F;
    if (channel_ >= 0 && ch != channel_) return;
    switch (status & 0xF0) {
        case 0x90:
            if (d2 == 0)
                synth_->noteOff(d1);
            else
                synth_->noteOn(d1, d2 / 127.0);
            break;
        case 0x80:
            synth_->noteOff(d1);
            break;
        case 0xB0:
            handleCC(d1, d2);
            break;
        case 0xD0:
            synth_->setAftertouch(d1 / 127.0);
            break;
        case 0xE0: {
            const int v = (d2 << 7) | d1;
            synth_->setPitchBend(std::max(-1.0, (v - 8192) / 8191.0));
            break;
        }
        case 0xC0:
            if (onProgramChange) onProgramChange(d1);
            break;
        default:
            break;
    }
}

void MidiController::armLearn(int layer, int paramId) {
    learnArmed_ = true;
    learnTarget_ = {layer, paramId};
}

void MidiController::applyNormalized(int layer, int param, double norm) {
    const auto& info = layerParamInfo(param);
    double v;
    if (info.kind == ParamKind::Switch)
        v = norm >= 0.5 ? 1.0 : 0.0;
    else
        v = info.minValue + norm * (info.maxValue - info.minValue);
    synth_->setLayerParam(layer, param, v);
}

void MidiController::applyNrpn(int number, int value14) {
    const double norm = value14 / 16383.0;
    if (number < 512) {
        const int layer = number / 256, param = number % 256;
        if (param < kNumLayerParams) applyNormalized(layer, param, norm);
    } else {
        const int id = number - 512;
        if (id < kNumProgramParams) {
            const auto& info = programParamInfo(id);
            synth_->setProgramParam(id, info.minValue + norm * (info.maxValue - info.minValue));
        }
    }
}

void MidiController::handleCC(int cc, int value) {
    // Learn captures any CC except the fixed performance/channel-mode set.
    const bool reserved = cc == 1 || cc == 33 || cc == 64 || cc == 6 || cc == 38 || cc == 98 || cc == 99 ||
                          cc == 100 || cc == 101 || cc >= 120;
    if (learnArmed_ && !reserved) {
        learned_[cc] = learnTarget_;
        learnArmed_ = false;
    }
    switch (cc) {
        case 1:
            modMsb_ = value;
            synth_->setModWheel(value / 127.0);
            return;
        case 33:
            synth_->setModWheel(((modMsb_ << 7) | value) / 16383.0);
            return;
        case 64:
            synth_->setSustain(value >= 64);
            return;
        case 99:
            nrpnMsb_ = value;
            nrpnActive_ = true;
            return;
        case 98:
            nrpnLsb_ = value;
            nrpnActive_ = true;
            return;
        case 101:
        case 100:
            nrpnActive_ = false;  // RPN selected; not used
            return;
        case 6:
            dataMsb_ = value;
            if (nrpnActive_) applyNrpn((nrpnMsb_ << 7) | nrpnLsb_, value << 7);
            return;
        case 38:
            if (nrpnActive_) applyNrpn((nrpnMsb_ << 7) | nrpnLsb_, (dataMsb_ << 7) | value);
            return;
        case 120:
            synth_->allSoundOff();
            return;
        case 121:
            synth_->setModWheel(0);
            synth_->setPitchBend(0);
            synth_->setAftertouch(0);
            synth_->setSustain(false);
            return;
        case 123:
            synth_->allNotesOff();
            return;
        default:
            break;
    }
    const auto it = learned_.find(cc);
    if (it != learned_.end()) {
        applyNormalized(it->second.layer, it->second.param, value / 127.0);
        return;
    }
    for (int i = 0; i < kNumLayerParams; ++i)
        if (layerParamInfo(i).cc == cc) {
            applyNormalized(editLayer_, i, value / 127.0);
            return;
        }
}

}  // namespace p10
