#include "Grid4D.h"

#include <cmath>
#include <algorithm>

namespace kv4d {

static inline int wrap(int v, int n) { return ((v % n) + n) % n; }

void Grid4D::resize(Dims4 d) {
    mDims = d;
    mCells.assign(d.count(), 0.0f);
    mNext.assign(d.count(), 0.0f);
}

void Grid4D::clear() { std::fill(mCells.begin(), mCells.end(), 0.0f); }

int Grid4D::alive() const {
    int n = 0;
    for (float c : mCells) n += (c != 0.0f);
    return n;
}

void Grid4D::step(const Rule4D& rule) {
    const Dims4& d = mDims;
    const bool moore = rule.nhood == Neighborhood::MOORE;

    for (int w = 0; w < d.w; w++)
    for (int z = 0; z < d.z; z++)
    for (int y = 0; y < d.y; y++)
    for (int x = 0; x < d.x; x++) {
        float self = get(x, y, z, w);
        int count = 0;
        float sum = 0.0f;
        int n = 0;

        for (int dw = -1; dw <= 1; dw++)
        for (int dz = -1; dz <= 1; dz++)
        for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int manhattan = std::abs(dx) + std::abs(dy) + std::abs(dz) + std::abs(dw);
            if (manhattan == 0) continue;
            if (!moore && manhattan != 1) continue;
            float s = get(wrap(x + dx, d.x), wrap(y + dy, d.y), wrap(z + dz, d.z), wrap(w + dw, d.w));
            count += (s == 1.0f);
            sum += s;
            n++;
        }

        float next;
        if (rule.type == RuleType::CONTINUOUS) {
            float v = sum / (float)n + rule.add;
            next = v - std::floor(v);
        } else {
            int state = (int)self;
            if (state == 0) {
                next = rule.birth[count] ? 1.0f : 0.0f;
            } else if (state == 1) {
                next = rule.survive[count] ? 1.0f : (rule.numStates > 2 ? 2.0f : 0.0f);
            } else {
                next = (state + 1 < rule.numStates) ? (float)(state + 1) : 0.0f;
            }
        }
        mNext[index(x, y, z, w)] = next;
    }
    mCells.swap(mNext);
}

float Grid4D::randomState(std::mt19937& rng, const Rule4D& rule) {
    if (rule.type == RuleType::CONTINUOUS) {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    }
    return 1.0f;
}

// Cells in the centred box covering `extent` of each axis.
static void boxRange(int n, float extent, int& lo, int& hi) {
    int len = std::max(1, (int)std::round(n * std::clamp(extent, 0.0f, 1.0f)));
    lo = (n - len) / 2;
    hi = lo + len;
}

void Grid4D::seedRandomBox(float extent, float density, const Rule4D& rule, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> coin(0.0f, 1.0f);
    int x0, x1, y0, y1, z0, z1, w0, w1;
    boxRange(mDims.x, extent, x0, x1);
    boxRange(mDims.y, extent, y0, y1);
    boxRange(mDims.z, extent, z0, z1);
    boxRange(mDims.w, extent, w0, w1);
    for (int w = w0; w < w1; w++)
    for (int z = z0; z < z1; z++)
    for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) {
        if (coin(rng) < density) set(x, y, z, w, randomState(rng, rule));
    }
}

void Grid4D::seedSymmetric(float extent, float density, const Rule4D& rule, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> coin(0.0f, 1.0f);
    const Dims4& d = mDims;
    int hx = (int)std::round(d.x * 0.5f * std::clamp(extent, 0.0f, 1.0f));
    int hy = (int)std::round(d.y * 0.5f * std::clamp(extent, 0.0f, 1.0f));
    int hz = (int)std::round(d.z * 0.5f * std::clamp(extent, 0.0f, 1.0f));
    int hw = (int)std::round(d.w * 0.5f * std::clamp(extent, 0.0f, 1.0f));
    // Fill one hyperoctant next to the centre, mirror into the other 15.
    for (int w = 0; w < hw; w++)
    for (int z = 0; z < hz; z++)
    for (int y = 0; y < hy; y++)
    for (int x = 0; x < hx; x++) {
        if (coin(rng) >= density) continue;
        float s = randomState(rng, rule);
        for (int m = 0; m < 16; m++) {
            int xx = (m & 1) ? d.x / 2 + x : (d.x - 1) / 2 - x;
            int yy = (m & 2) ? d.y / 2 + y : (d.y - 1) / 2 - y;
            int zz = (m & 4) ? d.z / 2 + z : (d.z - 1) / 2 - z;
            int ww = (m & 8) ? d.w / 2 + w : (d.w - 1) / 2 - w;
            set(xx, yy, zz, ww, s);
        }
    }
}

void Grid4D::seedWireTesseract(float extent) {
    int x0, x1, y0, y1, z0, z1, w0, w1;
    boxRange(mDims.x, extent, x0, x1);
    boxRange(mDims.y, extent, y0, y1);
    boxRange(mDims.z, extent, z0, z1);
    boxRange(mDims.w, extent, w0, w1);
    // A cell is on an edge of the tesseract when at least 3 of its 4
    // coordinates sit on a face of the box.
    for (int w = w0; w < w1; w++)
    for (int z = z0; z < z1; z++)
    for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) {
        int onFace = (x == x0 || x == x1 - 1) + (y == y0 || y == y1 - 1) +
                     (z == z0 || z == z1 - 1) + (w == w0 || w == w1 - 1);
        if (onFace >= 3) set(x, y, z, w, 1.0f);
    }
}

void Grid4D::seedBall(float cx, float cy, float cz, float cw, float radius, float density, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> coin(0.0f, 1.0f);
    const Dims4& d = mDims;
    float r = radius * (float)std::max(std::max(d.x, d.y), std::max(d.z, d.w));
    int ri = (int)std::ceil(r);
    int px = (int)(cx * d.x), py = (int)(cy * d.y), pz = (int)(cz * d.z), pw = (int)(cw * d.w);
    for (int w = -ri; w <= ri; w++)
    for (int z = -ri; z <= ri; z++)
    for (int y = -ri; y <= ri; y++)
    for (int x = -ri; x <= ri; x++) {
        if ((float)(x * x + y * y + z * z + w * w) > r * r) continue;
        if (coin(rng) >= density) continue;
        set(wrap(px + x, d.x), wrap(py + y, d.y), wrap(pz + z, d.z), wrap(pw + w, d.w), 1.0f);
    }
}

} // namespace kv4d
