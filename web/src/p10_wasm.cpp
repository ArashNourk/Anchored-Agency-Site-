// C ABI over the engine for the WebAssembly build (web app). One instance per
// module instance: the audio worklet owns one for sound, the UI thread owns a
// second, silent one for parameter metadata and program text conversion.
#include <cstring>
#include <memory>
#include <string>

#include "Synth.h"
#include "midi/MidiController.h"
#include "preset/PresetStore.h"

#define P10_API extern "C" __attribute__((used, visibility("default")))

using namespace p10;

namespace {
std::unique_ptr<Synth> g_synth;
std::unique_ptr<MidiController> g_midi;
float g_out[2][1024];
std::string g_text;
}  // namespace

P10_API int p10_create(double sampleRate, int oversample, int serial) {
    g_synth = std::make_unique<Synth>(sampleRate, oversample, static_cast<uint64_t>(serial));
    g_midi = std::make_unique<MidiController>(g_synth.get());
    return 1;
}

P10_API float* p10_out(int ch) { return g_out[ch & 1]; }

P10_API void p10_render(int n) {
    if (!g_synth) return;
    if (n > 1024) n = 1024;
    g_synth->render(g_out[0], g_out[1], n);
}

P10_API void p10_set_layer_param(int layer, int id, double v) { g_synth->setLayerParam(layer, id, v); }
P10_API double p10_get_layer_param(int layer, int id) { return g_synth->layerParam(layer, id); }
P10_API void p10_set_program_param(int id, double v) { g_synth->setProgramParam(id, v); }
P10_API double p10_get_program_param(int id) { return g_synth->program().settings[id]; }

P10_API void p10_note_on(int note, double vel) { g_synth->noteOn(note, vel); }
P10_API void p10_note_off(int note) { g_synth->noteOff(note); }
P10_API void p10_mod_wheel(double v) { g_synth->setModWheel(v); }
P10_API void p10_pitch_bend(double v) { g_synth->setPitchBend(v); }
P10_API void p10_pressure(double v) { g_synth->setAftertouch(v); }
P10_API void p10_sustain(int on) { g_synth->setSustain(on != 0); }
P10_API void p10_all_notes_off() { g_synth->allNotesOff(); }
P10_API void p10_all_sound_off() { g_synth->allSoundOff(); }
P10_API void p10_midi(int status, int d1, int d2) {
    g_midi->message(static_cast<uint8_t>(status), static_cast<uint8_t>(d1), static_cast<uint8_t>(d2));
}
P10_API void p10_tune() { g_synth->tune(); }
P10_API void p10_age(double hours) { g_synth->age(hours); }

// Metering.
P10_API double p10_voice_level(int v) { return g_synth->voice(v).ampLevel(); }
P10_API int p10_voice_gated(int v) { return g_synth->voice(v).gated() ? 1 : 0; }
P10_API int p10_voice_layer(int v) { return g_synth->voice(v).layer(); }

// Parameter metadata.
P10_API int p10_num_layer_params() { return kNumLayerParams; }
P10_API int p10_num_program_params() { return kNumProgramParams; }
P10_API const char* p10_layer_param_key(int i) { return layerParamInfo(i).key; }
P10_API const char* p10_layer_param_name(int i) { return layerParamInfo(i).name; }
P10_API int p10_layer_param_kind(int i) { return static_cast<int>(layerParamInfo(i).kind); }
P10_API double p10_layer_param_min(int i) { return layerParamInfo(i).minValue; }
P10_API double p10_layer_param_max(int i) { return layerParamInfo(i).maxValue; }
P10_API double p10_layer_param_default(int i) { return layerParamInfo(i).defaultValue; }
P10_API int p10_layer_param_cc(int i) { return layerParamInfo(i).cc; }
P10_API const char* p10_program_param_key(int i) { return programParamInfo(i).key; }

// Program text exchange. Caller writes UTF-8 into the buffer returned by
// p10_text_buffer(len) and calls p10_load_program_text(); p10_program_text()
// returns a NUL-terminated string valid until the next call.
P10_API char* p10_text_buffer(int len) {
    g_text.assign(static_cast<size_t>(len) + 1, '\0');
    return g_text.data();
}
P10_API int p10_load_program_text() {
    auto p = programFromText(std::string(g_text.c_str()));
    if (!p) return 0;
    g_synth->setProgram(*p);
    return 1;
}
P10_API const char* p10_program_text() {
    g_text = programToText(g_synth->program());
    return g_text.c_str();
}
P10_API void p10_set_program_name(const char* name) {
    Program p = g_synth->program();
    p.name = name;
    g_synth->setProgram(p);
}
P10_API void p10_init_program() { g_synth->setProgram(Program::init()); }
