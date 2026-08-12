#pragma once

#include <cstddef>

namespace ota_asset_name {
namespace detail {
constexpr bool isDigit(const char c) { return c >= '0' && c <= '9'; }

constexpr size_t length(const char* value) {
  size_t result = 0;
  while (value != nullptr && value[result] != '\0') ++result;
  return result;
}

constexpr bool equals(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) return false;
  while (*lhs != '\0' && *rhs != '\0') {
    if (*lhs++ != *rhs++) return false;
  }
  return *lhs == *rhs;
}

constexpr bool startsWith(const char* value, const char* prefix) {
  if (value == nullptr || prefix == nullptr) return false;
  while (*prefix != '\0') {
    if (*value == '\0' || *value != *prefix) return false;
    ++value;
    ++prefix;
  }
  return true;
}
}  // namespace detail

constexpr bool matches(const char* assetName, const char* assetStem, const char* unversionedAssetName) {
  if (assetName == nullptr || assetStem == nullptr || unversionedAssetName == nullptr) return false;
  if (detail::equals(assetName, unversionedAssetName)) return true;
  if (!detail::startsWith(assetName, assetStem)) return false;

  const char* version = assetName + detail::length(assetStem);
  if (version[0] != '-' || (version[1] != 'v' && version[1] != 'V')) return false;
  version += 2;

  size_t segmentCount = 0;
  while (segmentCount < 4) {
    if (!detail::isDigit(*version)) return false;
    while (detail::isDigit(*version)) ++version;
    ++segmentCount;

    if (detail::equals(version, ".bin")) return segmentCount >= 3;
    if (*version != '.') return false;
    ++version;
  }

  return false;
}
}  // namespace ota_asset_name
