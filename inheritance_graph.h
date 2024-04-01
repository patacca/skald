#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace skald {

typedef uint64_t address_t;
typedef address_t node_identifier_t;  // A node identifier is also an address

class Node {
   public:
    bool isLeaf() { return children.empty(); }

    std::vector<node_identifier_t> parents;
    std::vector<node_identifier_t> children;
    std::string name;
    address_t rttiAddress;
};

class InheritanceGraph {
   public:
    InheritanceGraph();
    void addNode(const std::string &name, address_t rttiAddress,
                 const std::vector<address_t> &children);
    Node &getNode(address_t addr);

   private:
    std::vector<Node> graph;
    // Map from node identifier to index in the graph vector
    std::unordered_map<node_identifier_t, uint32_t> idMap;
    std::unordered_set<node_identifier_t> leaves;
    std::unordered_set<node_identifier_t> roots;
};

}  // namespace skald