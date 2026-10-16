#include "Calibration.h"

#include <fstream>
#include <sstream>

namespace p10 {

double VintageSigma::at(double v) const {
    // Piecewise-linear across the four anchors at 0, 1/3, 2/3, 1.
    if (v <= 0.0) return rev4;
    if (v >= 1.0) return rev1;
    const double x = v * 3.0;
    const int seg = static_cast<int>(x);
    const double f = x - seg;
    const double pts[4] = {rev4, rev3, rev2, rev1};
    return pts[seg] + (pts[seg + 1] - pts[seg]) * f;
}

namespace {
void addSigma(std::vector<CalibrationModel::Entry>& e, const std::string& key, VintageSigma& s,
              const std::string& id) {
    e.push_back({key + ".rev4", &s.rev4, id});
    e.push_back({key + ".rev3", &s.rev3, id});
    e.push_back({key + ".rev2", &s.rev2, id});
    e.push_back({key + ".rev1", &s.rev1, id});
}
void addFilter(std::vector<CalibrationModel::Entry>& e, const std::string& p, FilterModelCal& f,
               const std::string& id) {
    e.push_back({p + ".inputGain", &f.inputGain, id});
    e.push_back({p + ".outputGain", &f.outputGain, id});
    e.push_back({p + ".resOnsetKnob", &f.resOnsetKnob, id});
    e.push_back({p + ".resCurve", &f.resCurve, id});
    e.push_back({p + ".kMax", &f.kMax, id});
    e.push_back({p + ".passbandComp", &f.passbandComp, id});
    e.push_back({p + ".fbDrive", &f.fbDrive, id});
    e.push_back({p + ".stageDrive", &f.stageDrive, id});
    e.push_back({p + ".hfResDamp", &f.hfResDamp, id});
    e.push_back({p + ".cutoffScale", &f.cutoffScale, id});
}
}  // namespace

std::vector<CalibrationModel::Entry> CalibrationModel::entries() {
    std::vector<Entry> e;
    auto add = [&](const char* k, double& v, const char* id) { e.push_back({k, &v, id}); };
    add("osc.a.freqRangeSemis", oscAFreqRangeSemis, "A-OSC-01");
    add("osc.a.freqQuantize", oscAFreqQuantize, "A-OSC-02");
    add("osc.b.freqRangeSemis", oscBFreqRangeSemis, "A-OSC-01");
    add("osc.b.freqQuantize", oscBFreqQuantize, "A-OSC-02");
    add("osc.b.fineRangeSemis", oscBFineRangeSemis, "A-OSC-03");
    add("osc.b.loFreqShiftSemis", oscBLoFreqShiftSemis, "A-OSC-04");
    add("osc.b.kbdOffRefNote", oscBKbdOffRefNote, "A-OSC-05");
    add("osc.sawLevel", sawLevel, "A-OSC-06");
    add("osc.pulseLevel", pulseLevel, "A-OSC-06");
    add("osc.triLevel", triLevel, "A-OSC-06");
    add("osc.waveCombineMax", waveCombineMax, "A-OSC-07");
    add("osc.pwMinDuty", pwMinDuty, "A-OSC-08");
    add("osc.pwMaxDuty", pwMaxDuty, "A-OSC-08");
    add("osc.resetTimeSec", oscResetTimeSec, "A-OSC-09");
    add("osc.sawCurvature", sawCurvature, "A-OSC-10");
    add("osc.outputBandwidthHz", oscOutputBandwidthHz, "A-OSC-12");
    add("osc.pulseHighAtRampEnd", pulseHighAtRampEnd, "A-OSC-13");
    add("mix.levelTaper", mixLevelTaper, "A-MIX-01");
    add("mix.noiseGain", noiseGain, "A-MIX-02");
    add("mix.drive", mixDrive, "A-MIX-03");
    add("mix.asym", mixAsym, "A-MIX-04");
    add("mix.dcBlockHz", mixDcBlockHz, "A-MIX-05");
    add("filter.cutoffMinHz", cutoffMinHz, "A-FLT-01");
    add("filter.cutoffMaxHz", cutoffMaxHz, "A-FLT-01");
    add("filter.envRangeSemis", filterEnvRangeSemis, "A-FLT-02");
    add("filter.kbdRefNote", filterKbdRefNote, "A-FLT-03");
    add("filter.kbdHalf", filterKbdHalf, "A-FLT-04");
    add("filter.noiseFloor", filterNoiseFloor, "A-FLT-05");
    addFilter(e, "filter.ssm", ssm, "A-FLT-1x");
    addFilter(e, "filter.cem", cem, "A-FLT-2x");
    add("env.attackMinSec", envAttackMinSec, "A-ENV-01");
    add("env.attackMaxSec", envAttackMaxSec, "A-ENV-01");
    add("env.decayMinSec", envDecayMinSec, "A-ENV-02");
    add("env.decayMaxSec", envDecayMaxSec, "A-ENV-02");
    add("env.releaseMinSec", envReleaseMinSec, "A-ENV-03");
    add("env.releaseMaxSec", envReleaseMaxSec, "A-ENV-03");
    add("env.timeTaper", envTimeTaper, "A-ENV-04");
    add("env.attackTarget", envAttackTarget, "A-ENV-05");
    add("env.decayTauDivisor", envDecayTauDivisor, "A-ENV-06");
    add("vel.filterDepth", velFilterDepth, "A-ENV-07");
    add("vel.ampDepth", velAmpDepth, "A-ENV-08");
    add("vel.curve", velCurve, "A-ENV-09");
    add("vca.feedthrough", vcaFeedthrough, "A-VCA-01");
    add("vca.sat", vcaSat, "A-VCA-02");
    add("pm.freqARangeSemis", pmFreqARangeSemis, "A-PM-01");
    add("pm.pwARange", pmPwARange, "A-PM-02");
    add("pm.filterRangeSemis", pmFilterRangeSemis, "A-PM-03");
    add("pm.envBipolar", pmEnvBipolar, "A-PM-04");
    add("pm.oscBSourceGain", pmOscBSourceGain, "A-PM-05");
    add("pm.amountTaper", pmAmountTaper, "A-PM-06");
    add("lfo.minHz", lfoMinHz, "A-LFO-01");
    add("lfo.maxHz", lfoMaxHz, "A-LFO-01");
    add("lfo.multiWaveGain", lfoMultiWaveGain, "A-LFO-02");
    add("lfo.squareSlewSec", lfoSquareSlewSec, "A-LFO-03");
    add("wheel.noiseLpHz", wheelNoiseLpHz, "A-LFO-04");
    add("wheel.combineMax", wheelCombineMax, "A-LFO-05");
    add("mod.freqRangeSemis", modFreqRangeSemis, "A-LFO-06");
    add("mod.pwRange", modPwRange, "A-LFO-06");
    add("mod.filterRangeSemis", modFilterRangeSemis, "A-LFO-06");
    add("mod.amountTaper", modAmountTaper, "A-LFO-07");
    add("at.filterRangeSemis", atFilterRangeSemis, "A-AT-01");
    add("at.lfoDepth", atLfoDepth, "A-AT-02");
    add("glide.minSec", glideMinSec, "A-GLD-01");
    add("glide.maxSec", glideMaxSec, "A-GLD-01");
    add("glide.taper", glideTaper, "A-GLD-02");
    add("unison.detuneMaxCents", unisonDetuneMaxCents, "A-UNI-01");
    addSigma(e, "vintage.oscTuneCents", vOscTuneCents, "A-VIN-01");
    addSigma(e, "vintage.oscScaleCentsPerOct", vOscScaleCentsPerOct, "A-VIN-02");
    addSigma(e, "vintage.oscHfResidual", vOscHfResidual, "A-VIN-03");
    addSigma(e, "vintage.pwOffset", vPwOffset, "A-VIN-04");
    addSigma(e, "vintage.sawCurvature", vSawCurvature, "A-VIN-05");
    addSigma(e, "vintage.cutoffSemis", vCutoffSemis, "A-VIN-06");
    addSigma(e, "vintage.filterTrack", vFilterTrack, "A-VIN-07");
    addSigma(e, "vintage.resScale", vResScale, "A-VIN-08");
    addSigma(e, "vintage.filterEnvTime", vFilterEnvTime, "A-VIN-09");
    addSigma(e, "vintage.ampEnvTime", vAmpEnvTime, "A-VIN-09");
    addSigma(e, "vintage.envSustain", vEnvSustain, "A-VIN-10");
    addSigma(e, "vintage.vcaGainDb", vVcaGainDb, "A-VIN-11");
    addSigma(e, "vintage.vcaFeedthrough", vVcaFeedthrough, "A-VIN-12");
    addSigma(e, "vintage.stageMismatch", vStageMismatch, "A-VIN-13");
    addSigma(e, "vintage.driftCents", vDriftCents, "A-VIN-14");
    addSigma(e, "vintage.globalDriftCents", vGlobalDriftCents, "A-VIN-15");
    addSigma(e, "vintage.jitterCents", vJitterCents, "A-VIN-16");
    addSigma(e, "vintage.filterDriftSemis", vFilterDriftSemis, "A-VIN-17");
    add("vintage.driftTauSec", vDriftTauSec, "A-VIN-14");
    add("vintage.globalDriftTauSec", vGlobalDriftTauSec, "A-VIN-15");
    add("vintage.jitterBandwidthHz", vJitterBandwidthHz, "A-VIN-16");
    add("cal.rawTuneCents", calRawTuneCents, "A-CAL-01");
    add("cal.rawScaleCentsPerOct", calRawScaleCentsPerOct, "A-CAL-01");
    add("cal.rawCutoffSemis", calRawCutoffSemis, "A-CAL-01");
    add("cal.rawCutoffScale", calRawCutoffScale, "A-CAL-01");
    add("cal.dacStepCents", calDacStepCents, "A-CAL-02");
    add("cal.measureNoiseCents", calMeasureNoiseCents, "A-CAL-03");
    add("cal.agingCentsPerHour", calAgingCentsPerHour, "A-CAL-04");
    add("cal.hfTrimEfficiency", calHfTrimEfficiency, "A-CAL-05");
    add("alloc.mode", allocMode, "A-VAL-01");
    add("alloc.reuseSameNote", allocReuseSameNote, "A-VAL-02");
    add("alloc.stealQuietest", allocStealQuietest, "A-VAL-03");
    add("alloc.stackFixedHalves", allocStackFixedHalves, "A-VAL-04");
    add("alloc.splitFixedHalves", allocSplitFixedHalves, "A-VAL-05");
    add("out.gain", outputGain, "A-OUT-01");
    add("out.dcHz", outputDcHz, "A-OUT-02");
    return e;
}

bool CalibrationModel::loadFromText(const std::string& text, std::vector<std::string>* errors) {
    auto reg = entries();
    std::istringstream in(text);
    std::string line;
    bool ok = true;
    while (std::getline(in, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto trim = [](std::string s) {
            const auto b = s.find_first_not_of(" \t\r");
            const auto e = s.find_last_not_of(" \t\r");
            return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
        };
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));
        bool found = false;
        for (auto& en : reg) {
            if (en.key == key) {
                try {
                    *en.value = std::stod(val);
                } catch (...) {
                    ok = false;
                    if (errors) errors->push_back("bad value for " + key);
                }
                found = true;
                break;
            }
        }
        if (!found) {
            ok = false;
            if (errors) errors->push_back("unknown calibration key " + key);
        }
    }
    return ok;
}

bool CalibrationModel::loadFromFile(const std::string& path, std::vector<std::string>* errors) {
    std::ifstream f(path);
    if (!f) {
        if (errors) errors->push_back("cannot open " + path);
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return loadFromText(ss.str(), errors);
}

std::string CalibrationModel::toText() {
    std::ostringstream o;
    o.precision(10);
    o << "# Prophet-10 Rev4 model calibration. See docs/ASSUMPTIONS.md for each ID.\n";
    for (auto& en : entries()) o << en.key << " = " << *en.value << "    # " << en.assumption << "\n";
    return o.str();
}

}  // namespace p10
