#include "hand_encoder.h"
#include <algorithm>
#include <stdexcept>

static const char* kPointTable[16] = {
    "", "4","6","8","9","10","J","Q","K","A","3","2","5","小鬼","大鬼","7"
};

static uint64_t binom(int n, int k) {
    if (k < 0 || k > n) return 0;
    if (k > n - k) k = n - k;
    uint64_t r = 1;
    for (int i = 1; i <= k; ++i) {
        r = r * (n - k + i) / i;
    }
    return r;
}

int pointToIndex(const std::string& point) {
    for (int i = 1; i < 16; ++i) {
        if (point == kPointTable[i]) return i;
    }
    return 0;
}

std::string indexToPoint(int idx) {
    if (idx < 0 || idx > 15) return "";
    return kPointTable[idx];
}

uint16_t encodeHand(const std::vector<std::string>& points) {
    int a[5] = {0,0,0,0,0};
    int sz = (int)points.size();
    if (sz > 5) throw std::runtime_error("encodeHand: >5 cards");
    for (int i = 0; i < sz; ++i) a[i] = pointToIndex(points[i]);
    for (int i = sz; i < 5; ++i) a[i] = 0;
    std::sort(a, a + 5);
    uint64_t code = 0;
    for (int i = 0; i < 5; ++i) {
        int b = a[i] + i;
        code += binom(b, i + 1);
    }
    return (uint16_t)code;
}

std::vector<std::string> decodeHand(uint16_t code) {
    int b[5] = {0,0,0,0,0};
    int remaining = (int)code;
    for (int i = 4; i >= 0; --i) {
        int lo = i, hi = 20;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if ((int)binom(mid, i + 1) <= remaining) lo = mid + 1;
            else hi = mid;
        }
        b[i] = lo - 1;
        remaining -= (int)binom(b[i], i + 1);
    }
    std::vector<std::string> result;
    for (int i = 0; i < 5; ++i) {
        int a = b[i] - i;
        if (a > 0) result.push_back(kPointTable[a]);
    }
    return result;
}