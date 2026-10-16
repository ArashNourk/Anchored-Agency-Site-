#include "PresetStore.h"

#include <fstream>
#include <sstream>

namespace p10 {

namespace {
std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    const auto e = s.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}
}  // namespace

std::string programToText(const Program& p) {
    std::ostringstream o;
    o.precision(10);
    o << "[program]\nname=" << p.name << "\n";
    for (int l = 0; l < 2; ++l)
        for (int i = 0; i < kNumLayerParams; ++i)
            o << (l == 0 ? "A." : "B.") << layerParamInfo(i).key << "=" << p.layer[l][i] << "\n";
    for (int i = 0; i < kNumProgramParams; ++i) o << "P." << programParamInfo(i).key << "=" << p.settings[i] << "\n";
    o << "[end]\n";
    return o.str();
}

std::optional<Program> programFromText(const std::string& text, std::vector<std::string>* warnings) {
    Program p = Program::init();
    std::istringstream in(text);
    std::string line;
    bool any = false;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line == "[program]") continue;
        if (line == "[end]") break;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));
        any = true;
        if (key == "name") {
            p.name = val;
            continue;
        }
        double v;
        try {
            v = std::stod(val);
        } catch (...) {
            if (warnings) warnings->push_back("bad value: " + line);
            continue;
        }
        if (key.size() > 2 && (key[0] == 'A' || key[0] == 'B') && key[1] == '.') {
            const int id = findLayerParam(key.substr(2));
            if (id >= 0) {
                const auto& info = layerParamInfo(id);
                p.layer[key[0] == 'A' ? 0 : 1][id] = std::min(std::max(v, info.minValue), info.maxValue);
                continue;
            }
        } else if (key.size() > 2 && key[0] == 'P' && key[1] == '.') {
            const int id = findProgramParam(key.substr(2));
            if (id >= 0) {
                const auto& info = programParamInfo(id);
                p.settings[id] = std::min(std::max(v, info.minValue), info.maxValue);
                continue;
            }
        }
        if (warnings) warnings->push_back("unknown key: " + key);
    }
    if (!any) return std::nullopt;
    return p;
}

ProgramBank::ProgramBank(std::string name) : name_(std::move(name)), programs_(kProgramsPerBank, Program::init()) {}

std::string ProgramBank::toText() const {
    std::ostringstream o;
    o << "# p10 program bank\nbank=" << name_ << "\n";
    for (const auto& p : programs_) o << programToText(p);
    return o.str();
}

bool ProgramBank::fromText(const std::string& text, std::vector<std::string>* warnings) {
    std::istringstream in(text);
    std::string line, block;
    int index = 0;
    bool inBlock = false;
    while (std::getline(in, line)) {
        const std::string t = trim(line);
        if (!inBlock && t.rfind("bank=", 0) == 0) name_ = t.substr(5);
        if (t == "[program]") {
            inBlock = true;
            block.clear();
            continue;
        }
        if (t == "[end]" && inBlock) {
            inBlock = false;
            if (index < kProgramsPerBank) {
                auto p = programFromText(block, warnings);
                if (p) programs_[index] = *p;
            }
            ++index;
            continue;
        }
        if (inBlock) block += line + "\n";
    }
    return index > 0;
}

bool ProgramBank::save(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << toText();
    return static_cast<bool>(f);
}

bool ProgramBank::load(const std::string& path, std::vector<std::string>* warnings) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    return fromText(ss.str(), warnings);
}

}  // namespace p10
