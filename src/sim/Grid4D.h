// Grid4D.h - CPU-side 4D grid, stored in the same 2D atlas layout the GPU uses:
//   u = x + X*z,  v = y + Y*w,  index = v * (X*Z) + u
// It is used for seeding (uploaded straight to the GPU texture) and as the
// reference implementation the GPU step is tested against.

#pragma once

#include "Rule4D.h"

#include <vector>
#include <random>
#include <cstdint>

namespace kv4d {

struct Dims4 {
    int x = 16, y = 16, z = 16, w = 16;
    int count() const { return x * y * z * w; }
    int atlasW() const { return x * z; }
    int atlasH() const { return y * w; }
};

class Grid4D {
public:
    explicit Grid4D(Dims4 d = Dims4()) { resize(d); }

    void resize(Dims4 d);
    const Dims4& dims() const { return mDims; }

    int index(int x, int y, int z, int w) const {
        return (y + mDims.y * w) * mDims.atlasW() + (x + mDims.x * z);
    }
    float get(int x, int y, int z, int w) const { return mCells[index(x, y, z, w)]; }
    void set(int x, int y, int z, int w, float v) { mCells[index(x, y, z, w)] = v; }

    std::vector<float>& cells() { return mCells; }
    const std::vector<float>& cells() const { return mCells; }

    int alive() const;  // cells with state != 0

    // One generation with periodic boundaries.
    void step(const Rule4D& rule);

    // Seeds. Coordinates and extents are fractions of the grid (0..1).
    void clear();
    void seedRandomBox(float extent, float density, const Rule4D& rule, uint32_t seed);
    void seedSymmetric(float extent, float density, const Rule4D& rule, uint32_t seed);  // 16-way hyperoctant mirror
    void seedWireTesseract(float extent);
    void seedBall(float cx, float cy, float cz, float cw, float radius, float density, uint32_t seed);

private:
    Dims4 mDims;
    std::vector<float> mCells;
    std::vector<float> mNext;

    float randomState(std::mt19937& rng, const Rule4D& rule);
};

} // namespace kv4d
