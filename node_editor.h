#ifndef NODE_EDITOR_H
#define NODE_EDITOR_H

#include "node.h"
#include "imgui_includes.h"

class NodeEditor
{

  public:
    const std::string id;

    Nodes nodes;
    std::vector<NodeType> nodeTypes;

    vec2 scroll;
    float zoom = 1;
    float zoomSpeed = 1;
    
    NodeEditor(Nodes nodes, std::vector<NodeType> nodeTypes);

    void draw(ImDrawList* drawList);

    void deleteNode(Node node);

  private:
    bool hasFocus = false;
    vec2 pos, drawPos;
    vec2 mousePos, prevMousePos;
    vec2 dragDelta;

    vec2 scrollBeforeDrag;

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
    vec2 nodePosBeforeDragging;

    void updateNode(int i);
    void drawNode(Node node, ImDrawList *drawList);

    ImRect getNodeRectangle(Node node);

    void updateZoom();

};

#endif
