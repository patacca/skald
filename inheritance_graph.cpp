
#include "inheritance_graph.h"

#include <fmt/format.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "binaryninjaapi.h"

namespace skald {

InheritanceGraph::InheritanceGraph() {}

void InheritanceGraph::addNode(const std::string &name, address_t rttiAddress,
                               const std::vector<edge_t> &children) {
    // Add the missing children
    for (const auto &[addr, e_flags] : children) {
        // First time adding the children node. Add it as a skeleton node that will be later
        // initialized
        if (!this->idMap.contains(addr)) {
            this->idMap[addr] = this->graph.size();
            this->graph.push_back({{}, {}, "", addr, addr});
        }
        this->roots.erase(addr);  // Children is not a root anymore
        this->graph[this->idMap[addr]].parents.push_back({rttiAddress, e_flags});
    }

    // Add it as a leaf only if it has no children
    if (children.empty()) this->leaves.insert(rttiAddress);

    if (this->idMap.contains(rttiAddress)) {
        // Node already added because it was a children of another class.
        // Initialize its content
        this->graph[this->idMap[rttiAddress]].name = name;
        this->graph[this->idMap[rttiAddress]].children = children;
    } else {  // First time adding it. Create the node
        Node node{{}, children, name, rttiAddress, rttiAddress};
        this->roots.insert(rttiAddress);
        this->idMap[rttiAddress] = this->graph.size();
        this->graph.push_back(std::move(node));
    }
}

Node &InheritanceGraph::getNodeById(const node_identifier_t &id) {
    if (!this->idMap.contains(id))
        throw std::invalid_argument(fmt::format("No node with id %p", id));
    return this->graph[this->idMap[id]];
}

Node &InheritanceGraph::getNodeByAddr(const address_t &addr) {
    // In this case the id is the same as the address but in future it might change
    return this->getNodeById(addr);
}

}  // namespace skald