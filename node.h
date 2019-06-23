
#ifndef NODE_H
#define NODE_H

#include <string>
#include <vector>
#include <memory>

#include "imgui_includes.h"

struct NodeValueType_
{
    std::string name;
    vec3 color;
};
typedef std::shared_ptr<NodeValueType_> NodeValueType;


struct NodeConnector_
{
    std::string name, description;
    NodeValueType valType;
    bool multiple;
};
typedef std::shared_ptr<NodeConnector_> NodeConnector;


struct NodeType_
{
    std::string name, description;

    std::vector<NodeConnector> inputs, outputs;

    bool sameOutputsAsInputs, canHaveChildren;

};
typedef std::shared_ptr<NodeType_> NodeType;


class Node_;
typedef std::shared_ptr<Node_> Node;
typedef std::vector<Node> Nodes;


struct Input
{
    Node sourceNode;
    NodeConnector output;
};


struct Node_
{
    const NodeType type;
    vec2 position, size;
    bool collapsed = false;

    std::vector<Input> inputs;
    Nodes children;
};

#endif
