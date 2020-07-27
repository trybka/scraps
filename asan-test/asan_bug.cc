#include <memory>
#include <unordered_map>

#include "gmock/gmock.h"

// using ::testing::_;
using ::testing::An;
using ::testing::Pair;

constexpr int kIterations = 100;

int main() {
  std::vector<std::unordered_map<int, int>> garbage;
  auto matcher = ::testing::internal::MakePredicateFormatterFromMatcher(
      Pair(An<int>(), An<int>()));
  for (int i = 0; i < kIterations; ++i) {
    std::unordered_map<int, int> t;
    for (int j = 0; j < kIterations; ++j) {
      t[j];
      for (auto p : t) {
        matcher("p", p);
        matcher("p", p);
        matcher("p", p);
        matcher("p", p);
        matcher("p", p);
        matcher("p", p);
        matcher("p", p);
        matcher("p", p);
      }
    }
    garbage.push_back(std::move(t));
  }
}
