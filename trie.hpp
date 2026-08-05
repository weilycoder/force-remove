#ifndef FRM_TRIE_HPP
#define FRM_TRIE_HPP

#include <array>
#include <cstdint>
#include <generator>
#include <stack>
#include <utility>
#include <vector>

class Trie {
private:
  std::vector<bool> isEnd;
  std::vector<std::array<std::size_t, 256>> nodes;

  std::size_t new_node() {
    isEnd.emplace_back(false);
    nodes.emplace_back(std::array<std::size_t, 256>{});
    return nodes.size() - 1;
  }

public:
  explicit Trie() {
    new_node(); // Create the root node
  }

  void insert(const void *data, std::size_t length) {
    std::size_t nodeIndex = 0;
    const uint8_t *byteData = reinterpret_cast<const uint8_t *>(data);
    for (std::size_t i = 0; i < length; ++i) {
      uint8_t byte = byteData[i];
      if (nodes[nodeIndex][byte] == 0) nodes[nodeIndex][byte] = new_node();
      nodeIndex = nodes[nodeIndex][byte];
    }
    isEnd[nodeIndex] = true;
  }

  bool exists(const void *data, std::size_t length) const {
    std::size_t nodeIndex = 0;
    const uint8_t *byteData = reinterpret_cast<const uint8_t *>(data);
    for (std::size_t i = 0; i < length; ++i) {
      uint8_t byte = byteData[i];
      if (nodes[nodeIndex][byte] == 0) return false;
      nodeIndex = nodes[nodeIndex][byte];
    }
    return isEnd[nodeIndex];
  }

  std::generator<const std::vector<std::uint8_t> &> get_all() const {
    std::vector<std::uint8_t> current;
    std::stack<std::pair<std::size_t, std::size_t>> stack;

    stack.emplace(0, 0); // Start with the root node and index 0
    while (!stack.empty()) {
      auto &[nodeIndex, byteIndex] = stack.top();
      while (byteIndex < 256 && nodes[nodeIndex][byteIndex] == 0) ++byteIndex;
      if (byteIndex == 256) {
        if (isEnd[nodeIndex]) co_yield current;
        if (!current.empty()) current.pop_back();
        stack.pop();
      } else {
        current.emplace_back(static_cast<std::uint8_t>(byteIndex));
        stack.emplace(nodes[nodeIndex][byteIndex++], 0);
      }
    }
  }
};

#endif // FRM_TRIE_HPP