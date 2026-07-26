/**
 * @file src/platform/common.cpp
 * @brief Shared platform helper implementations.
 */

#include "common.h"

#include <algorithm>
#include <cctype>

namespace {
  /**
   * @brief Compare ASCII protocol identifiers without case sensitivity.
   * @param left First identifier.
   * @param right Second identifier.
   * @return True when both identifiers are equal ignoring ASCII case.
   */
  bool equals_case_insensitive(std::string_view left, std::string_view right) {
    return left.size() == right.size() && std::ranges::equal(left, right, [](char a, char b) {
             return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
           });
  }

  /**
   * @brief Extract the primary language subtag from a BCP-47-like identifier.
   * @param language Language identifier to inspect.
   * @return The portion before the first region or variant separator.
   */
  std::string_view primary_language(std::string_view language) {
    const auto separator = language.find_first_of("-_");
    return language.substr(0, separator);
  }
}  // namespace

namespace platf {
  int keyboard_layout_match_score(const keyboard_layout_t &requested, const keyboard_layout_t &candidate) {
    if (requested.platform == candidate.platform && !requested.id.empty() && equals_case_insensitive(requested.id, candidate.id)) {
      return 1000;
    }
    if (requested.language.empty() || candidate.language.empty()) {
      return -1;
    }
    if (equals_case_insensitive(requested.language, candidate.language)) {
      return 500;
    }
    if (equals_case_insensitive(primary_language(requested.language), primary_language(candidate.language))) {
      return 250;
    }
    return -1;
  }
}  // namespace platf
