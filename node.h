
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

static NodeValueType createNodeValueType(NodeValueType_ x) { return std::make_shared<NodeValueType_>(x); }


struct NodeConnector_
{
    std::string name, description;
    NodeValueType valType;
};
typedef std::shared_ptr<NodeConnector_> NodeConnector;

static NodeConnector createNodeConnector(NodeConnector_ x) { return std::make_shared<NodeConnector_>(x); }


struct NodeType_
{
    std::string name, description;

    std::vector<NodeConnector> inputs, outputs;

    bool canHaveChildren;
};
typedef std::shared_ptr<NodeType_> NodeType;

static NodeType createNodeType(NodeType_ x) { return std::make_shared<NodeType_>(x); }


class Node_;
typedef std::shared_ptr<Node_> Node;
typedef std::vector<Node> Nodes;

struct Connection
{
    Node srcNode, dstNode;
    NodeConnector output, input;
};


struct Node_
{
    const NodeType type;
    vec2 position, size;
    bool collapsed = false;

    std::vector<NodeConnector> additionalInputs, additionalOutputs;

    std::vector<Connection> connections;
    Nodes children;

    ~Node_()
    {
        std::cout << "Node destroyed\n";
    }
};

static Node createNode(Node_ x) { return std::make_shared<Node_>(x); }

#endif
