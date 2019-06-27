#ifndef NODE_EDITOR_H
#define NODE_EDITOR_H

#include "node.h"
#include "imgui_includes.h"

class NodeEditor
{
  public:
    static json copiedNodes;

    const std::string id;

    Nodes nodes;
    std::vector<NodeValueType> valueTypes;
    std::vector<NodeType> nodeTypes;

    vec2 scroll;
    float zoom = 1;
    float zoomSpeed = 1;
    
    NodeEditor(Nodes nodes, std::vector<NodeType> nodeTypes, std::vector<NodeValueType> valueTypes);

    void draw(ImDrawList* drawList);

    void deleteNode(Node node);

    void deleteConnection(Connection &c);

    bool isSelected(Node node);

    bool containsLoop();

    json toJson(Nodes nodes);

    Nodes fromJson(json input, bool &success);

  private:
    bool hasFocus = false, multiSelect = false;
    vec2 pos, drawPos;
    vec2 mousePos, prevMousePos;
    vec2 dragDelta;

    // --- add node menu: ---
    std::string filter;
    const char *addMenuId;
    vec2 addPos;
    int addMenuSelectedI = -1;

    void drawAddMenu();
    // ---

    Node hoveringNode, activeNode;
    int hoveringNodeI = -1;

    Node currentlyResizing;
    vec2 nodeSizeBeforeResizing;

    Node currentlyDragging;

    void updateNode(int i);
    void drawNode(Node node, ImDrawList *drawList);
    void resizeNode(Node node, ImDrawList *drawList);
    void dragNode(Node node, ImDrawList *drawList, float dragBarRounding);
    void drawNodeConnectors(Node node, ImDrawList *drawList);

    void drawNodeConnector(Node node, NodeConnector c, ImDrawList *drawList);

    void drawConnections(ImDrawList *drawList);

    vec2 connectorPosition(Node node, NodeConnector c);
    bool isInput(Node node, NodeConnector c);
    bool isConnected(Node n, NodeConnector c);

    ImRect getNodeRectangle(Node node);

    void updateZoom();
    void drawBackground(ImDrawList *drawList);

    bool selecting = false;
    Nodes selectedNodes;
    void updateSelection(ImDrawList *drawList);

    std::unique_ptr<Connection> creatingConnection;

    // returns the connections that have this node as start/source/begin
    std::vector<Connection> getOutputConnections(Node n);

    // returns the connections that have this node as destination
    std::vector<Connection> getInputConnections(Node n);

    bool detectLoopDfs(Node curr, Nodes &toVisit, Nodes &visiting, Nodes &visited);

    NodeConnector connectorByName(Node n, std::string name);

};

#endif
