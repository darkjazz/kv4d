#include "Rule4D.h"

#include <sstream>
#include <vector>
#include <cctype>

namespace kv4d {

static void setRange(std::array<uint8_t, kMaxNeighbors + 1>& a, int lo, int hi, bool on) {
    if (lo > hi) std::swap(lo, hi);
    for (int i = std::max(0, lo); i <= std::min(kMaxNeighbors, hi); i++) a[i] = on ? 1 : 0;
}

void Rule4D::setBirthRange(int lo, int hi, bool on) { setRange(birth, lo, hi, on); }
void Rule4D::setSurviveRange(int lo, int hi, bool on) { setRange(survive, lo, hi, on); }

static bool parseList(const std::string& s, std::array<uint8_t, kMaxNeighbors + 1>& out) {
    out.fill(0);
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) continue;
        size_t dash = item.find('-');
        try {
            if (dash == std::string::npos) {
                setRange(out, std::stoi(item), std::stoi(item), true);
            } else {
                setRange(out, std::stoi(item.substr(0, dash)), std::stoi(item.substr(dash + 1)), true);
            }
        } catch (...) {
            return false;
        }
    }
    return true;
}

bool Rule4D::parse(const std::string& spec) {
    Rule4D r = *this;
    r.type = RuleType::GENERATIONS;
    r.numStates = 2;
    r.nhood = Neighborhood::MOORE;
    bool haveB = false, haveS = false;

    std::stringstream ss(spec);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (part.empty()) continue;
        char key = (char)std::toupper((unsigned char)part[0]);
        std::string body = part.substr(1);
        switch (key) {
            case 'B': if (!parseList(body, r.birth)) return false; haveB = true; break;
            case 'S': if (!parseList(body, r.survive)) return false; haveS = true; break;
            case 'C':
                try { r.numStates = std::max(2, std::stoi(body)); } catch (...) { return false; }
                break;
            case 'N':
                if (body.empty()) return false;
                r.nhood = (std::toupper((unsigned char)body[0]) == 'V') ? Neighborhood::VON_NEUMANN : Neighborhood::MOORE;
                break;
            default: return false;
        }
    }
    if (!haveB || !haveS) return false;
    *this = r;
    return true;
}

static std::string listToString(const std::array<uint8_t, kMaxNeighbors + 1>& a) {
    std::string out;
    int i = 0;
    while (i <= kMaxNeighbors) {
        if (!a[i]) { i++; continue; }
        int j = i;
        while (j + 1 <= kMaxNeighbors && a[j + 1]) j++;
        if (!out.empty()) out += ",";
        out += (i == j) ? std::to_string(i) : std::to_string(i) + "-" + std::to_string(j);
        i = j + 1;
    }
    return out;
}

std::string Rule4D::toString() const {
    if (type == RuleType::CONTINUOUS) {
        std::ostringstream o;
        o << "continuous add=" << add << (nhood == Neighborhood::MOORE ? " /NM" : " /NV");
        return o.str();
    }
    return "B" + listToString(birth) + "/S" + listToString(survive) +
           "/C" + std::to_string(numStates) + (nhood == Neighborhood::MOORE ? "/NM" : "/NV");
}

// From tools/rulesearch (16^4, 60 steps, centred box seed of extent 0.5).
// Every one is still active and neither extinct nor saturated at step 60.
static const RulePreset kPresets[] = {
    { "coral",   "B12-17/S11-20/C2/NM", 0.16f },  // fill 0.13
    { "sparse",  "B6-11/S13-25/C2/NM",  0.32f },  // fill 0.11
    { "soup",    "B21-27/S12-29/C2/NM", 0.35f },  // fill 0.30
    { "brain",   "B10-12/S11-12/C3/NM", 0.26f },  // fill 0.31
    { "decay",   "B6-10/S4-14/C4/NM",   0.19f },  // fill 0.23
    { "fronts",  "B8-10/S38-60/C4/NM",  0.33f },  // fill 0.40
    { "lattice", "B9-14/S38-39/C3/NM",  0.14f },  // fill 0.37
    { "pulse",   "B6-7/S31-37/C3/NM",   0.51f },  // fill 0.23
};

const RulePreset* rulePresets(int* count) {
    *count = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
    return kPresets;
}

} // namespace kv4d
