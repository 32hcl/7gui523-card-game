#pragma once

#include <vector>
#include <string>
#include <cstdint>

constexpr int kHandEncodedMax = 15504;
constexpr int kSymbolCount   = 16;

uint16_t encodeHand(const std::vector<std::string>& points);

std::vector<std::string> decodeHand(uint16_t code);

int pointToIndex(const std::string& point);

std::string indexToPoint(int idx);