#include "VoiceAllocator.h"

#include <algorithm>

namespace p10 {

void VoiceAllocator::configure(std::array<Voice, kNumVoices>* voices, const CalibrationModel* cal) {
    voices_ = voices;
    cal_ = cal;
}

void VoiceAllocator::setLayerConfig(int layer, const LayerPatch& p) {
    LayerCfg& c = cfg_[layer];
    const bool wasUnison = c.unison;
    c.unison = p[kUnison] > 0.5;
    c.unisonVoices = std::clamp(static_cast<int>(p[kUnisonVoices] + 0.5), 1, kNumVoices);
    c.unisonDetune = p[kUnisonDetune];
    c.keyPriority = std::clamp(static_cast<int>(p[kKeyPriority] + 0.5), 0, 2);
    c.retrigger = p[kRetrigger] > 0.5;
    if (wasUnison != c.unison) {
        for (int v : pool(layer)) {
            auto& vo = (*voices_)[v];
            if (vo.layer() == layer && vo.gated()) vo.release(++stamp_);
            vo.setDetuneCents(0.0);
        }
        monoNote_[layer] = -1;
    }
}

void VoiceAllocator::setLayerMode(int mode, int splitPoint, bool upperIsB) {
    if (mode != mode_) allNotesOff();
    mode_ = mode;
    splitPoint_ = splitPoint;
    upperIsB_ = upperIsB;
}

std::vector<int> VoiceAllocator::pool(int layer) const {
    std::vector<int> p;
    bool fixed = false;
    if (mode_ == 0) {
        if (layer == 1) return p;
        fixed = false;
    } else if (mode_ == 1) {
        fixed = cal_->allocSplitFixedHalves > 0.5;
    } else {
        fixed = cal_->allocStackFixedHalves > 0.5;
    }
    if (mode_ == 0 || !fixed) {
        for (int v = 0; v < kNumVoices; ++v) p.push_back(v);
    } else {
        const int half = kNumVoices / 2;
        for (int v = 0; v < half; ++v) p.push_back(layer * half + v);
    }
    return p;
}

std::vector<int> VoiceAllocator::layersForNote(int note) const {
    if (mode_ == 0) return {0};
    if (mode_ == 2) return {0, 1};
    const int upper = upperIsB_ ? 1 : 0;
    return {note >= splitPoint_ ? upper : 1 - upper};
}

void VoiceAllocator::noteOn(int note, double velocity) {
    for (int layer : layersForNote(note)) layerNoteOn(layer, note, velocity);
}

void VoiceAllocator::noteOff(int note) {
    // Release on every layer that may hold the note (split point could have
    // moved while the key was down).
    for (int layer = 0; layer < 2; ++layer) layerNoteOff(layer, note);
}

void VoiceAllocator::layerNoteOn(int layer, int note, double velocity) {
    auto& h = held_[layer];
    h.erase(std::remove_if(h.begin(), h.end(), [&](const Held& x) { return x.note == note; }), h.end());
    h.push_back({note, velocity, ++stamp_});
    auto& s = sustained_[layer];
    s.erase(std::remove(s.begin(), s.end(), note), s.end());
    if (cfg_[layer].unison)
        monoUpdate(layer, true, velocity);
    else
        polyNoteOn(layer, note, velocity);
}

void VoiceAllocator::layerNoteOff(int layer, int note) {
    auto& h = held_[layer];
    const auto it = std::find_if(h.begin(), h.end(), [&](const Held& x) { return x.note == note; });
    if (it == h.end()) return;
    h.erase(it);
    if (cfg_[layer].unison) {
        if (sustain_) {
            if (h.empty()) return;  // pedal keeps the current unison note
        }
        monoUpdate(layer, false, 0.0);
        return;
    }
    for (int v : pool(layer)) {
        auto& vo = (*voices_)[v];
        if (vo.layer() == layer && vo.note() == note && vo.gated()) {
            if (sustain_)
                sustained_[layer].push_back(note);
            else
                vo.release(++stamp_);
        }
    }
}

void VoiceAllocator::polyNoteOn(int layer, int note, double velocity) {
    const auto p = pool(layer);
    if (p.empty()) return;
    auto& vs = *voices_;
    int chosen = -1;

    // 1. The voice that last played this key gets it again (whether it is
    //    still releasing or has gone quiet); the most recent one wins.
    if (cal_->allocReuseSameNote > 0.5) {
        uint64_t newest = 0;
        for (int v : p)
            if (vs[v].layer() == layer && vs[v].note() == note && vs[v].startStamp() >= newest) {
                newest = vs[v].startStamp();
                chosen = v;
            }
    }
    // 2. Free voice by assignment policy.
    if (chosen < 0) {
        const int mode = static_cast<int>(cal_->allocMode + 0.5);
        if (mode == 0) {
            int start = 0;
            for (size_t i = 0; i < p.size(); ++i)
                if (p[i] == rotor_[layer]) start = static_cast<int>(i) + 1;
            for (size_t k = 0; k < p.size(); ++k) {
                const int v = p[(start + k) % p.size()];
                if (!vs[v].gated()) {
                    chosen = v;
                    break;
                }
            }
        } else {
            uint64_t best = UINT64_MAX;
            for (int v : p) {
                if (vs[v].gated()) continue;
                const uint64_t key = mode == 1 ? vs[v].releaseStamp() : static_cast<uint64_t>(v);
                if (key < best) {
                    best = key;
                    chosen = v;
                }
            }
        }
    }
    // 3. Steal.
    if (chosen < 0) {
        if (cal_->allocStealQuietest > 0.5) {
            double best = 1e30;
            for (int v : p)
                if (vs[v].ampLevel() < best) {
                    best = vs[v].ampLevel();
                    chosen = v;
                }
        } else {
            uint64_t best = UINT64_MAX;
            for (int v : p)
                if (vs[v].startStamp() < best) {
                    best = vs[v].startStamp();
                    chosen = v;
                }
        }
    }
    if (chosen < 0) return;
    vs[chosen].setDetuneCents(0.0);
    vs[chosen].start(note, velocity, layer, ++stamp_, true);
    lastAssigned_[layer] = chosen;
    rotor_[layer] = chosen;
}

int VoiceAllocator::pickMonoKey(int layer) const {
    const auto& h = held_[layer];
    if (h.empty()) return -1;
    switch (cfg_[layer].keyPriority) {
        case 0: return std::min_element(h.begin(), h.end(), [](auto& a, auto& b) { return a.note < b.note; })->note;
        case 1: return std::max_element(h.begin(), h.end(), [](auto& a, auto& b) { return a.note < b.note; })->note;
        default: return std::max_element(h.begin(), h.end(), [](auto& a, auto& b) { return a.order < b.order; })->note;
    }
}

void VoiceAllocator::monoUpdate(int layer, bool newKey, double velocity) {
    const auto p = pool(layer);
    const LayerCfg& c = cfg_[layer];
    const int n = std::min<int>(c.unisonVoices, static_cast<int>(p.size()));
    auto& vs = *voices_;
    const int key = pickMonoKey(layer);
    if (key < 0) {
        for (int i = 0; i < n; ++i)
            if (vs[p[i]].layer() == layer && vs[p[i]].gated()) vs[p[i]].release(++stamp_);
        monoNote_[layer] = -1;
        return;
    }
    if (velocity <= 0.0) {
        for (const auto& h : held_[layer])
            if (h.note == key) velocity = h.velocity;
    }
    const bool fromSilence = monoNote_[layer] < 0;
    const bool keyChanged = key != monoNote_[layer];
    if (!keyChanged && !newKey) return;
    if (!keyChanged && newKey) return;  // new key did not win priority
    const bool retrig = fromSilence || (c.retrigger && newKey);
    for (int i = 0; i < n; ++i) {
        Voice& v = vs[p[i]];
        const double spread = n > 1 ? (2.0 * i / (n - 1) - 1.0) : 0.0;
        v.setDetuneCents(spread * 0.5 * c.unisonDetune * cal_->unisonDetuneMaxCents);
        if (retrig || !v.gated() || v.layer() != layer)
            v.start(key, velocity, layer, ++stamp_, true);
        else
            v.legatoTo(key);
    }
    monoNote_[layer] = key;
    lastAssigned_[layer] = p[0];
}

void VoiceAllocator::setSustain(bool on) {
    sustain_ = on;
    if (on) return;
    for (int layer = 0; layer < 2; ++layer) {
        if (cfg_[layer].unison) {
            if (held_[layer].empty()) monoUpdate(layer, false, 0.0);
            continue;
        }
        for (int note : sustained_[layer]) {
            const bool stillHeld = std::any_of(held_[layer].begin(), held_[layer].end(),
                                               [&](const Held& h) { return h.note == note; });
            if (stillHeld) continue;
            for (int v : pool(layer)) {
                auto& vo = (*voices_)[v];
                if (vo.layer() == layer && vo.note() == note && vo.gated()) vo.release(++stamp_);
            }
        }
        sustained_[layer].clear();
    }
}

void VoiceAllocator::allNotesOff() {
    for (auto& v : *voices_)
        if (v.gated()) v.release(++stamp_);
    for (int l = 0; l < 2; ++l) {
        held_[l].clear();
        sustained_[l].clear();
        monoNote_[l] = -1;
    }
}

}  // namespace p10
