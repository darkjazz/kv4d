// rulesearch - random search for 4D Generations rules with sustained, non-trivial
// dynamics, using the CPU reference simulation.
//
//   c++ -std=c++17 -O3 -I../src tools/rulesearch.cpp src/sim/Rule4D.cpp src/sim/Grid4D.cpp -o build/rulesearch
//   ./build/rulesearch [candidates] [size] [steps] [seed]
//
// A rule passes when, starting from a centred random box, the population after
// `steps` generations is neither extinct nor saturated and is still changing.

#include "sim/Grid4D.h"

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <algorithm>

using namespace kv4d;

struct Result {
    std::string spec;
    float density, finalFill, activity, growth;
};

static bool evaluate(const Rule4D& rule, float density, int size, int steps, uint32_t seed, Result& out) {
    Grid4D g(Dims4{ size, size, size, size });
    g.seedRandomBox(0.5f, density, rule, seed);
    int start = g.alive();
    if (start == 0) return false;

    std::vector<float> prev;
    for (int i = 0; i < steps; i++) {
        if (i == steps - 1) prev = g.cells();
        g.step(rule);
        int a = g.alive();
        if (a == 0) return false;
    }
    int changed = 0;
    for (size_t i = 0; i < prev.size(); i++) changed += (prev[i] != g.cells()[i]);

    float total = (float)g.dims().count();
    out.finalFill = g.alive() / total;
    out.activity = changed / total;
    out.growth = g.alive() / (float)start;
    out.density = density;
    out.spec = rule.toString();
    return out.finalFill > 0.01f && out.finalFill < 0.45f && out.activity > 0.005f;
}

int main(int argc, char** argv) {
    int candidates = argc > 1 ? atoi(argv[1]) : 400;
    int size = argc > 2 ? atoi(argv[2]) : 16;
    int steps = argc > 3 ? atoi(argv[3]) : 60;
    uint32_t seed = argc > 4 ? (uint32_t)atoi(argv[4]) : 1234u;

    std::mutex lock;
    std::vector<Result> passed;
    std::atomic<int> next{ 0 };

    auto worker = [&]() {
        for (;;) {
            int i = next++;
            if (i >= candidates) return;
            std::mt19937 rng(seed + i * 7919u);
            auto ri = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };

            Rule4D r;
            r.clearCounts();
            int bLo = ri(6, 40), bHi = bLo + ri(0, 8);
            int sLo = ri(2, 40), sHi = sLo + ri(0, 30);
            r.setBirthRange(bLo, bHi);
            r.setSurviveRange(sLo, sHi);
            const int states[] = { 2, 2, 3, 4, 6, 8, 12 };
            r.numStates = states[ri(0, 6)];
            float density = std::uniform_real_distribution<float>(0.1f, 0.6f)(rng);

            Result res;
            if (evaluate(r, density, size, steps, seed + i, res)) {
                std::lock_guard<std::mutex> g(lock);
                passed.push_back(res);
                printf("PASS %-28s d=%.2f fill=%.3f act=%.3f growth=%.2f\n",
                       res.spec.c_str(), res.density, res.finalFill, res.activity, res.growth);
                fflush(stdout);
            }
        }
    };

    std::vector<std::thread> threads;
    unsigned n = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned t = 0; t < n; t++) threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    printf("\n%zu / %d rules passed\n", passed.size(), candidates);
    return 0;
}
