// Rule4D.h - rule description shared by the GPU simulation and the CPU reference.
//
// Generations semantics (Life is Generations with 2 states):
//   state 0      -> 1 if birth[n] else 0
//   state 1      -> 1 if survive[n] else (numStates > 2 ? 2 : 0)
//   state k >= 2 -> k + 1, wrapping to 0 at numStates
// where n is the number of neighbours in state 1.
//
// Continuous: next = fract(mean(neighbours) + add)

#pragma once

#include <array>
#include <string>
#include <cstdint>

namespace kv4d {

enum class RuleType : int { GENERATIONS = 0, CONTINUOUS = 1 };
enum class Neighborhood : int { MOORE = 0, VON_NEUMANN = 1 };

constexpr int kMaxNeighbors = 80;  // 3^4 - 1

struct Rule4D {
    RuleType type = RuleType::GENERATIONS;
    Neighborhood nhood = Neighborhood::MOORE;
    int numStates = 2;
    std::array<uint8_t, kMaxNeighbors + 1> birth{};
    std::array<uint8_t, kMaxNeighbors + 1> survive{};
    float add = 0.005f;

    int neighborCount() const { return nhood == Neighborhood::MOORE ? 80 : 8; }

    void clearCounts() { birth.fill(0); survive.fill(0); }
    void setBirthRange(int lo, int hi, bool on = true);
    void setSurviveRange(int lo, int hi, bool on = true);

    // Parses "B<list>/S<list>[/C<states>][/N<M|V>]" where <list> is
    // comma-separated counts or ranges, e.g. "B30-34/S26-40,44/C6/NM".
    // Returns false (and leaves the rule untouched) on malformed input.
    bool parse(const std::string& spec);
    std::string toString() const;
};

// Presets found with tools/rulesearch on a 16^4 torus (see README).
struct RulePreset { const char* name; const char* spec; float density; };
const RulePreset* rulePresets(int* count);

} // namespace kv4d
