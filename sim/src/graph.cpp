#include "netwar/graph.hpp"

namespace netwar {

Node* Graph::find(NodeId id) {
    for (auto& node : nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

const Node* Graph::find(NodeId id) const {
    for (const auto& node : nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

} // namespace netwar
