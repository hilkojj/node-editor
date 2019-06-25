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
    if (ImGui::GetIO().MouseWheel == 0 || !hasFocus) return;

    float newZoom = max(.1, min(10., zoom + ImGui::GetIO().MouseWheel * .15 * zoomSpeed * zoom));
    float factor = 1 - zoom / newZoom;
    scroll -= mousePos * factor;
    zoom = newZoom;
}

void NodeEditor::drawBackground(ImDrawList *drawList)
{
    vec2 windowSize = ImGui::GetWindowSize();
    float gridSize = 50 * zoom;
    ImColor color(1.f, 1., 1., .2);
    for (float x = fmod(drawPos.x * zoom, gridSize); x < windowSize.x + pos.x; x += gridSize)
        drawList->AddLine(vec2(x, pos.y), vec2(x, pos.y + windowSize.y), color, max(1.f, zoom));

    for (float y = fmod(drawPos.y * zoom, gridSize); y < windowSize.y + pos.y; y += gridSize)
        drawList->AddLine(vec2(pos.x, y), vec2(pos.x + windowSize.x, y), color, max(1.f, zoom));
}

bool NodeEditor::isSelected(Node node)
{
    for (Node n : selectedNodes) if (n == node) return true;
    return false;
}

void NodeEditor::draw(ImDrawList *drawList)
{
    multiSelect = ImGui::IsKeyDown(GLFW_KEY_LEFT_SHIFT) || ImGui::IsKeyDown(GLFW_KEY_LEFT_CONTROL);
    updateZoom();
    pos = ImGui::GetWindowPos();
    mousePos = (ImGui::GetMousePos() - pos) / zoom;
    dragDelta = ImGui::GetMouseDragDelta() / zoom;

    // update scroll:
    if (ImGui::IsMouseDown(2) && hasFocus)
        scroll += mousePos - prevMousePos;
    else scrollBeforeDrag = scroll;

    drawPos = pos / zoom + scroll;

    drawBackground(drawList);
    drawAddMenu();
    hoveringNode = NULL;
    hoveringNodeI = -1;
    // updateNode() will set hoveringNode if needed:
    for (int i = 0; i < nodes.size(); i++) updateNode(i);
    // draw the nodes:
    for (auto node : nodes) drawNode(node, drawList);

    updateSelection(drawList);

    hasFocus = ImGui::IsWindowFocused();
    prevMousePos = mousePos;
}

void NodeEditor::updateSelection(ImDrawList *drawList)
{
    if (!hasFocus) return;
    if (ImGui::IsMouseDown(0) && hoveringNode) // a node was clicked:
    {
        // bring clicked node to foreground:
        nodes[hoveringNodeI] = nodes[nodes.size() - 1];
        nodes[nodes.size() - 1] = hoveringNode;
        activeNode = hoveringNode;

        if (multiSelect) // select multiple nodes when CTRL or SHIFT is pressed:
        {
            if (!isSelected(activeNode)) selectedNodes.push_back(activeNode);
            if (!isSelected(nodes[hoveringNodeI])) selectedNodes.push_back(nodes[hoveringNodeI]);
        } else if (!isSelected(activeNode)) selectedNodes.clear();

    } // clear selection if user clicks on background:
    else if (ImGui::IsMouseDown(0) && !currentlyDragging && !currentlyResizing && !multiSelect) selectedNodes.clear();

    selecting = false;
    if (currentlyDragging || currentlyResizing || dragDelta.x + dragDelta.y == 0 || !ImGui::IsMouseDown(0)) return;
    selecting = true;

    // get the selection rectangle:
    ImRect selectRect = ImRect(mousePos - scroll - dragDelta + drawPos, mousePos - scroll + drawPos);
    selectRect.Max *= zoom;
    selectRect.Min *= zoom;
    ImRect temp = selectRect;
    selectRect.Min.x = min(temp.Max.x, temp.Min.x);
    selectRect.Max.x = max(temp.Max.x, temp.Min.x);
    selectRect.Min.y = min(temp.Max.y, temp.Min.y);
    selectRect.Max.y = max(temp.Max.y, temp.Min.y);
    if (!multiSelect) selectedNodes.clear();
    // look for nodes in selection rectangle:
    for (auto n : nodes) if (getNodeRectangle(n).Overlaps(selectRect) && !isSelected(n)) selectedNodes.push_back(n);

    // draw selection rectangle:
    drawList->AddRectFilled(selectRect.Min, selectRect.Max, ImColor(.1f, 1., 1., .5));
    drawList->AddRect(selectRect.Min, selectRect.Max, ImColor(.1f, 1., 1.), 0, 0, 2);
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
    bool selected = isSelected(node);
    int rounding = (node->collapsed ? 15 : 4) * zoom;
    int roundingFlags = ImDrawCornerFlags_Top | ImDrawCornerFlags_BotLeft | (node->collapsed ? ImDrawCornerFlags_BotRight : 0);
    ImRect nodeRect = getNodeRectangle(node);
    // draw shadow using a hack:
    for (int i = 0; i < 8; i++)
        drawList->AddRectFilled(
            nodeRect.Min - vec2(i),
            nodeRect.Max + vec2(i),
        ImColor(.0f, .0, .0, .07), rounding + i * 2, roundingFlags);
    // node background:
    drawList->AddRectFilled(nodeRect.Min, nodeRect.Max, ImColor(.3f, .3, .35, .85), rounding, roundingFlags);

    resizeNode(node, drawList);
    // node outline:
    drawList->AddRect(nodeRect.Min, nodeRect.Max, 
        active || selected ? ImColor(.4f, .2, 1.) :
        (hovering ? ImColor(.4f, .1, .6) : ImColor(.4f, .4, .4)),
        rounding, roundingFlags, 2);
    drawNodeConnectors(node, drawList);
    dragNode(node, drawList, rounding);


    // node title:
    drawList->AddText(NULL, 13 * zoom, nodeRect.Min + vec2(25, 9) * zoom, ImColor(1.f, 1., 1.), node->type->name.c_str());

    // draw collapse button (click on collapse-button is handled in dragNode()):
    if (node->collapsed)
        drawList->AddTriangleFilled(
            nodeRect.Min + vec2(10) * zoom,
            nodeRect.Min + vec2(20, 15) * zoom,
            nodeRect.Min + vec2(10, 20) * zoom, ImColor(1.f, 1., 1., .5)
        );
    else
        drawList->AddTriangleFilled(
            nodeRect.Min + vec2(10, 11) * zoom,
            nodeRect.Min + vec2(20, 11) * zoom,
            nodeRect.Min + vec2(15, 21) * zoom, ImColor(1.f, 1., 1., .5)
        );

    if (active && ImGui::IsKeyPressed(GLFW_KEY_DELETE))
        deleteNode(node);
}

void NodeEditor::resizeNode(Node node, ImDrawList *drawList)
{
    if (node->collapsed) return;
    ImRect nodeRect = getNodeRectangle(node);
    // Triangle (a, b, c) that can be dragged to resize node:
    ImVec2 a = ImVec2(nodeRect.Max.x, nodeRect.Max.y - 20 * zoom),
        b = ImVec2(nodeRect.Max.x - 20 * zoom, nodeRect.Max.y),
        c = nodeRect.Max;
    bool mouseOverResizeTriangle = hoveringNode == node && !selecting && !currentlyDragging && ImTriangleContainsPoint(a, b, c, ImGui::GetMousePos());
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
void NodeEditor::dragNode(Node node, ImDrawList *drawList, float dragBarRounding)
{
    // drag bar:
    ImRect nodeRect = getNodeRectangle(node);
    ImRect dragRect = ImRect(nodeRect.Min + vec2(2), ImVec2(nodeRect.Max.x - 2, nodeRect.Min.y + 30 * zoom));
    if (!node->collapsed) // draw dragbar
    {
        int dragBarRoundingFlags = ImDrawCornerFlags_TopLeft | ImDrawCornerFlags_TopRight;
        drawList->AddRectFilled(dragRect.Min, dragRect.Max, ImColor(.4f, .4, .4), dragBarRounding, dragBarRoundingFlags);
        drawList->AddRectFilled(ImVec2(dragRect.Min.x, dragRect.Min.y + 15 * zoom), dragRect.Max, ImColor(.37f, .37, .37));
    }
    bool mouseOverDragBar = hoveringNode == node && !selecting && !currentlyResizing && dragRect.Contains(ImGui::GetMousePos());
    
    if (mouseOverDragBar || currentlyDragging == node)
    {
        if (!currentlyDragging)
        {
            currentlyDragging = node;
            ImGui::ResetMouseDragDelta();
        }
        if (ImGui::IsMouseDown(0))
        {
            if (isSelected(node)) // move each selected node:
                for (auto n : selectedNodes) n->position += mousePos - prevMousePos;
            else
            { // if user is dragging a non-selected node, then clear the selection:
                selectedNodes.clear();
                node->position += mousePos - prevMousePos;
            }
        }
        else
        {
            currentlyDragging = NULL;
            ImGui::SetTooltip(node->type->description.c_str());

            // collapse node if collapse-icon was clicked:
            ImRect collapseRect = ImRect(nodeRect.Min + vec2(8 * zoom), nodeRect.Min + vec2(24 * zoom));
            if (dragDelta.x + dragDelta.y == 0 && ImGui::IsMouseReleased(0) && !multiSelect && collapseRect.Contains(ImGui::GetMousePos()))
                node->collapsed = !node->collapsed;
        }
    }
}

void NodeEditor::drawNodeConnectors(Node node, ImDrawList *drawList)
{
    for (NodeConnector c : node->type->inputs) drawNodeConnector(node, c, drawList);
    for (NodeConnector c : node->type->outputs) drawNodeConnector(node, c, drawList);
    for (NodeConnector c : node->additionalInputs) drawNodeConnector(node, c, drawList);
    for (NodeConnector c : node->additionalOutputs) drawNodeConnector(node, c, drawList);
}

void NodeEditor::drawNodeConnector(Node node, NodeConnector c, ImDrawList *drawList)
{
    vec2 pos = connectorPosition(node, c);

    drawList->AddCircleFilled(pos, 6 * zoom, ImColor(c->valType->color));
    drawList->AddCircle(pos, 6 * zoom, ImColor((c->valType->color * vec3(.5))), 12, zoom);

    std::cout << (mousePos - pos).length() << "\n";

    // show type of connector when hovering:
    if ((mousePos - pos).length() < 10) ImGui::SetTooltip(c->valType->name.c_str());

    if (node->collapsed) return;
    // show connector name:
    drawList->AddText(NULL, 13 * zoom, pos, ImColor(vec4(1)), c->name.c_str());
}

vec2 NodeEditor::connectorPosition(Node node, NodeConnector conn)
{
    bool input = isInput(node, conn);
    ImRect nodeRect = getNodeRectangle(node);
    vec2 pos = vec2(input ? nodeRect.Min.x : nodeRect.Max.x, nodeRect.Min.y + 15 * zoom);
    if (node->collapsed) return pos;

    pos.y += 30 * zoom;
    int row = 0;
    if (input)
    {
        for (NodeConnector c : node->type->inputs)
            if (c == conn) break;
            else row++;
        for (NodeConnector c : node->additionalInputs)
            if (c == conn) break;
            else row++;
    }
    else
    {
        for (NodeConnector c : node->type->outputs)
            if (c == conn) break;
            else row++;
        for (NodeConnector c : node->additionalOutputs)
            if (c == conn) break;
            else row++;
    }
    pos.y += 26 * row * zoom;
    return pos;
}

bool NodeEditor::isInput(Node node, NodeConnector conn)
{
    for (NodeConnector c : node->type->inputs) if (c == conn) return true;
    for (NodeConnector c : node->additionalInputs) if (c == conn) return true;
    return false;
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
                Node n = createNode({ nodeType });
                n->position = addPos;
                n->size = vec2(100, 100);
                nodes.push_back(n);
                activeNode = n;
                selectedNodes.clear();

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
