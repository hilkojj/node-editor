#include <iostream>
#include <algorithm>

#include "node_editor.h"
#include <GLFW/glfw3.h>

int nodeEditorI = 0;

NodeEditor::NodeEditor(Nodes nodes, std::vector<NodeType> nodeTypes)
    : nodes(nodes), nodeTypes(nodeTypes), id("node_editor_" + (nodeEditorI++)), addMenuId((id + "_add_menu").c_str())
{
}

void NodeEditor::deleteNode(Node node)
{
    activeNode = NULL;
    for (int i = 0; i < nodes.size(); i++)
    {
        if (nodes[i] == node)
        {
            nodes[i] = nodes[nodes.size() - 1];
            nodes.pop_back();
        }
    }
}

// Try to find in the Haystack the Needle - ignore case
bool findStringIC(const std::string &strHaystack, const std::string &strNeedle)
{
    auto it = std::search(
        strHaystack.begin(), strHaystack.end(),
        strNeedle.begin(), strNeedle.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
    return (it != strHaystack.end());
}

void NodeEditor::updateZoom()
{
    if (ImGui::GetIO().MouseWheel == 0) return;

    float newZoom = max(.1, min(10., zoom + ImGui::GetIO().MouseWheel * .1 * zoomSpeed));
    float factor = 1 - zoom / newZoom;
    scroll -= mousePos * factor;
    zoom = newZoom;
}

void NodeEditor::draw(ImDrawList *drawList)
{
    updateZoom();
    pos = ImGui::GetWindowPos();
    mousePos = (ImGui::GetMousePos() - pos) / zoom;
    dragDelta = ImGui::GetMouseDragDelta() / zoom;

    if (ImGui::IsMouseDown(2))
        scroll += mousePos - prevMousePos;
    else scrollBeforeDrag = scroll;

    drawPos = pos / zoom + scroll;

    drawAddMenu();
    hoveringNode = NULL;
    hoveringNodeI = -1;
    for (int i = 0; i < nodes.size(); i++) updateNode(i);
    if (ImGui::IsMouseDown(0) && hoveringNodeI >= 0)
    {
        nodes[hoveringNodeI] = nodes[nodes.size() - 1];
        nodes[nodes.size() - 1] = hoveringNode;
        activeNode = hoveringNode;
    }
    for (auto node : nodes) drawNode(node, drawList);

    hasFocus = ImGui::IsWindowFocused();
    prevMousePos = mousePos;
}

void NodeEditor::updateNode(int i)
{
    Node node = nodes[i];
    ImRect nodeRect = getNodeRectangle(node);
    if (
        hasFocus 
        && nodeRect.Contains(ImGui::GetMousePos())
        && (!currentlyDragging || currentlyDragging == node)
        && (!currentlyResizing || currentlyResizing == node)
    )
    {
        hoveringNode = node;
        hoveringNodeI = i;
    }
}

void NodeEditor::drawNode(Node node, ImDrawList *drawList)
{
    bool hovering = hoveringNode == node;
    bool active = activeNode == node;
    int rounding = (node->collapsed ? 15 : 4) * zoom;
    int roundingFlags = ImDrawCornerFlags_Top | ImDrawCornerFlags_BotLeft | (node->collapsed ? ImDrawCornerFlags_BotRight : 0);
    ImRect nodeRect = getNodeRectangle(node);
    for (int i = 0; i < 5; i++)
        drawList->AddRectFilled(
            nodeRect.Min - vec2(i),
            nodeRect.Max + vec2(i),
        ImColor(.0f, .0, .0, .07), rounding + i, roundingFlags);
    
    drawList->AddRectFilled(nodeRect.Min, nodeRect.Max, ImColor(.3f, .3, .35, .85), rounding, roundingFlags);

    // drag bar:
    ImRect dragRect = ImRect(nodeRect.Min, ImVec2(nodeRect.Max.x, nodeRect.Min.y + 30 * zoom));
    if (!node->collapsed)
    {
        int dragBarRounding = ImDrawCornerFlags_TopLeft | ImDrawCornerFlags_TopRight;
        drawList->AddRectFilled(dragRect.Min, dragRect.Max, ImColor(.4f, .4, .4), rounding, dragBarRounding);
        drawList->AddRectFilled(ImVec2(dragRect.Min.x, dragRect.Min.y + 15 * zoom), dragRect.Max, ImColor(.37f, .37, .37));
    }
    bool mouseOverDragBar = hovering && !currentlyResizing && dragRect.Contains(ImGui::GetMousePos());
    
    if (mouseOverDragBar || currentlyDragging == node)
    {
        if (!currentlyDragging)
        {
            nodePosBeforeDragging = node->position;
            currentlyDragging = node;
            ImGui::ResetMouseDragDelta();
        }
        if (ImGui::IsMouseDown(0))
            node->position = nodePosBeforeDragging + dragDelta;
        else
        {
            currentlyDragging = NULL;
            ImGui::SetTooltip(node->type->description.c_str());

            if (dragDelta.x + dragDelta.y == 0 && ImGui::IsMouseReleased(0))
                node->collapsed = !node->collapsed;
        }
    }
    // resize triangle:
    if (!node->collapsed)
    {
        ImVec2 a = ImVec2(nodeRect.Max.x, nodeRect.Max.y - 20 * zoom),
            b = ImVec2(nodeRect.Max.x - 20 * zoom, nodeRect.Max.y),
            c = nodeRect.Max;
        bool mouseOverResizeTriangle = hovering && !currentlyDragging && ImTriangleContainsPoint(a, b, c, ImGui::GetMousePos());
        drawList->AddTriangleFilled(
            a, b, c, 
            mouseOverResizeTriangle ? ImColor(.7f, .7, .7) : ImColor(.5f, .5, .5)
        );
        if (mouseOverResizeTriangle || node == currentlyResizing)
        {
            if (!currentlyResizing)
            {
                nodeSizeBeforeResizing = node->size;
                currentlyResizing = node;
                ImGui::ResetMouseDragDelta();
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

            if (ImGui::IsMouseDown(0))
            {
                node->size = nodeSizeBeforeResizing + dragDelta;
                if (node->size.x < 100) node->size.x = 100;
                if (node->size.y < 100) node->size.y = 100;
            } else currentlyResizing = NULL;
        }
    }
    drawList->AddRect(nodeRect.Min, nodeRect.Max, active ? ImColor(.4f, .2, 1.) : (hovering ? ImColor(.4f, .1, .6) : ImColor(.4f, .4, .4)), rounding, roundingFlags, 2);

    drawList->AddText(NULL, 13 * zoom, nodeRect.Min + vec2(25, 9) * zoom, ImColor(1.f, 1., 1.), node->type->name.c_str(), node->type->name.c_str() + node->type->name.length());

    if (node->collapsed)
        drawList->AddTriangleFilled(
            nodeRect.Min + vec2(10) * zoom,
            nodeRect.Min + vec2(20, 15) * zoom,
            nodeRect.Min + vec2(10, 20) * zoom, ImColor(1.f, 1., 1.)
        );
    else
        drawList->AddTriangleFilled(
            nodeRect.Min + vec2(10, 11) * zoom,
            nodeRect.Min + vec2(20, 11) * zoom,
            nodeRect.Min + vec2(15, 21) * zoom, ImColor(1.f, 1., 1.)
        );

    if (active && ImGui::IsKeyPressed(GLFW_KEY_DELETE))
        deleteNode(node);
}

ImRect NodeEditor::getNodeRectangle(Node node)
{
    ImRect rect(drawPos.x + node->position.x, drawPos.y + node->position.y, 
        drawPos.x + node->position.x + node->size.x, drawPos.y + node->position.y + node->size.y);
    if (node->collapsed)
        rect.Max.y = rect.Min.y + 30;

    rect.Max *= zoom;
    rect.Min *= zoom;
    return rect;
}

void NodeEditor::drawAddMenu()
{
    if (hasFocus && ImGui::IsKeyPressed(GLFW_KEY_A) && ImGui::IsKeyDown(GLFW_KEY_LEFT_SHIFT))
        ImGui::OpenPopup(addMenuId);

    if (ImGui::BeginPopupContextWindow(addMenuId))
    {
        for (auto c : ImGui::GetIO().InputQueueCharacters)
            if (filter.length() > 0 || c != 'A')
                filter += c;

        if (filter.length())
        {
            if (addMenuSelectedI < 0) addMenuSelectedI = 0;
            if (ImGui::IsKeyPressed(GLFW_KEY_BACKSPACE))
                filter.pop_back();

            ImGui::Text(("Filter: " + filter).c_str());
        } else ImGui::Text("Add node");
        ImGui::Separator();
        addMenuSelectedI += ImGui::IsKeyPressed(GLFW_KEY_DOWN) ? 1 : (ImGui::IsKeyPressed(GLFW_KEY_UP) ? -1 : 0);

        int i = 0;
        for (auto nodeType : nodeTypes)
        {
            if (!findStringIC(nodeType->name, filter))
                continue;
            bool selected = addMenuSelectedI == i;

            std::string name = selected ? ">" + nodeType->name + "<" : nodeType->name;
            if (ImGui::MenuItem(name.c_str(), NULL, selected) || (selected && ImGui::IsKeyPressed(GLFW_KEY_ENTER)))
            {
                Node n = std::make_shared<Node_>(Node_{ nodeType });
                n->position = addPos;
                n->size = vec2(100, 100);
                nodes.push_back(n);
                activeNode = n;

                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(nodeType->description.c_str());

            i++;
        }
        if (addMenuSelectedI >= i) addMenuSelectedI = i - 1;
        if (addMenuSelectedI < -1) addMenuSelectedI = -1;
        if (ImGui::IsKeyPressed(GLFW_KEY_ESCAPE)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    else
    {
        filter = "";
        addMenuSelectedI = -1;
        addPos = mousePos - scroll;
    }
}
