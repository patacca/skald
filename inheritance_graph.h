#pragma once

#include <cstdint>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bitmask.h"

namespace skald {

typedef enum : uint32_t { VIRTUAL = 1, PUBLIC = 2 } EdgeFlag;
DECLARE_BITMASK(EdgeFlag);

typedef uint64_t address_t;
typedef address_t node_identifier_t;  // A node identifier is also an address

// Inheritance edge type. It has a node identifier and EdgeFlag describing the edge type
typedef std::pair<node_identifier_t, EdgeFlag> edge_t;

class Node {
   public:
    bool isLeaf() { return children.empty(); }

    std::vector<edge_t> parents;
    std::vector<edge_t> children;
    std::string name;
    address_t rttiAddress;
    node_identifier_t id;  // For now the address is the same as the id, but it might change
};

class InheritanceGraph {
   public:
    InheritanceGraph();
    void addNode(const std::string &name, address_t rttiAddress,
                 const std::vector<edge_t> &children);
    Node &getNodeByAddr(const address_t &addr);
    Node &getNodeById(const node_identifier_t &id);

    auto getRoots() {
        return std::views::transform(this->roots, [&](const node_identifier_t id) -> Node & {
            return this->graph[this->idMap[id]];
        });
    };

   private:
    std::vector<Node> graph;

    // Map from a node identifier to its index in the graph vector
    std::unordered_map<node_identifier_t, uint32_t> idMap;

    std::unordered_set<node_identifier_t> leaves;
    std::unordered_set<node_identifier_t> roots;
};

}  // namespace skald