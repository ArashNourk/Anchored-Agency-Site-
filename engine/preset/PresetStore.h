// Program storage. A program stores every panel position of both layers and
// the bi-timbral settings, by parameter key (not by index), so files survive
// parameter-list growth. Banks follow a group/program addressing scheme
// (N banks x 40 programs = 5 groups x 8 programs).
//
// No factory content ships with the engine. External data (e.g. a legally
// obtained SysEx dump) is brought in through the PatchTranslator interface,
// which is deliberately left unimplemented: the hardware dump format and the
// mapping of its value ranges to this engine's panel positions must be
// established from documentation/measurement before a translator is written.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../model/Params.h"

namespace p10 {

std::string programToText(const Program& p);
// Parses one [program] ... [end] block (or a bare key=value list). Missing
// keys keep their basic-patch values; unknown keys are reported in warnings.
std::optional<Program> programFromText(const std::string& text, std::vector<std::string>* warnings = nullptr);

class ProgramBank {
public:
    static constexpr int kProgramsPerBank = 40;  // 5 groups x 8
    explicit ProgramBank(std::string name = "User");

    const std::string& name() const { return name_; }
    Program& at(int index) { return programs_.at(index); }
    const Program& at(int index) const { return programs_.at(index); }
    static int indexOf(int group, int number) { return group * 8 + number; }  // 0-based

    std::string toText() const;
    bool fromText(const std::string& text, std::vector<std::string>* warnings = nullptr);
    bool save(const std::string& path) const;
    bool load(const std::string& path, std::vector<std::string>* warnings = nullptr);

private:
    std::string name_;
    std::vector<Program> programs_;
};

class PatchTranslator {
public:
    virtual ~PatchTranslator() = default;
    virtual std::optional<Program> fromSysEx(const std::vector<uint8_t>& message) = 0;
    virtual std::vector<uint8_t> toSysEx(const Program& p) = 0;
};

}  // namespace p10
