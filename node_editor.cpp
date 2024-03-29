#include <iostream>
#include <algorithm>
#include <GLFW/glfw3.h>

#include "node_editor.h"

int nodeEditorI = 0;
json NodeEditor::copiedNodes = json();

NodeEditor::NodeEditor(Nodes nodes, std::vector<NodeType> nodeTypes, std::vector<NodeValueType> valueTypes)
    :
    nodes(nodes), nodeTypes(nodeTypes), valueTypes(valueTypes),
    id("node_editor_" + (nodeEditorI++)), addMenuId((id + "_add_menu").c_str())
{
    createHistory();
}

void NodeEditor::deleteNode(Node node)
{
    activeNode = NULL; // todo kfhskfgjhfdkgjh
    nodes.erase(
        std::remove_if(
            nodes.begin(), nodes.end(), [node](Node &n) { return n == node; }
        ), nodes.end()
    );
    while (!node->connections.empty()) deleteConnection(node->connections.back());
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

bool shortcutPressed(int key0, int key1)
{
    return (ImGui::IsKeyPressed(key0, false) && ImGui::IsKeyPressed(key1, false))
           || (ImGui::IsKeyPressed(key0, false) && ImGui::IsKeyDown(key1))
           || (ImGui::IsKeyDown(key0) && ImGui::IsKeyPressed(key1, false));
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

    drawPos = pos / zoom + scroll;

    if (shortcutPressed(GLFW_KEY_LEFT_CONTROL, GLFW_KEY_Z)) undo();
    if (shortcutPressed(GLFW_KEY_LEFT_CONTROL, GLFW_KEY_Y)) redo();

    drawBackground(drawList);
    drawAddMenu();
    drawConnections(drawList);
    hoveringNode = NULL;
    hoveringNodeI = -1;
    // updateNode() will set hoveringNode if needed:
    for (int i = 0; i < nodes.size(); i++) updateNode(i);
    // draw the nodes:
    for (auto node : nodes) drawNode(node, drawList);

    updateSelection(drawList);

    hasFocus = ImGui::IsWindowFocused();
    prevMousePos = mousePos;

    if (creatingConnection && !ImGui::IsMouseDown(0)) creatingConnection = NULL;
    if (creatingConnection)
    {
        vec2 originPos = connectorPosition(creatingConnection->srcNode, creatingConnection->output);
        vec2 dstPos = (mousePos - scroll + drawPos) * zoom;

        float xDiff = abs(originPos.x - dstPos.x) * .6;
        drawList->AddBezierCurve(originPos, originPos + vec2(xDiff, 0), dstPos - vec2(xDiff, 0), dstPos, ImColor(vec4(1)), zoom * 3);
    }

    if (ImGui::IsKeyPressed(GLFW_KEY_DELETE))
    {
        if (selectedNodes.empty() && activeNode) deleteNode(activeNode);
        else for (auto &n : selectedNodes) deleteNode(n);
        createHistory();
    }
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

    if (shortcutPressed(GLFW_KEY_C, GLFW_KEY_LEFT_CONTROL) && hasFocus)
    {
        NodeEditor::copiedNodes = toJson(isSelected(activeNode) ? selectedNodes : Nodes{activeNode});
        std::cout << NodeEditor::copiedNodes << "\n";
    }
    if (shortcutPressed(GLFW_KEY_V, GLFW_KEY_LEFT_CONTROL) && hasFocus)
    {
        bool success;
        Nodes pasted = fromJson(copiedNodes, success);
        for (Node n : pasted)
        {
            n->position += vec2(100, 100);
            nodes.push_back(n);
        }
        if (!success) std::cout << "parsing nodes unsuccessful\n";
        selectedNodes = pasted;
        copiedNodes = toJson(pasted); // hack to give the next paste extra offset.
        createHistory();
    }
    if (shortcutPressed(GLFW_KEY_A, GLFW_KEY_LEFT_CONTROL) && hasFocus)
        selectedNodes = nodes;

    selecting = false;
    if (creatingConnection || currentlyDragging || currentlyResizing || dragDelta.x + dragDelta.y == 0 || !ImGui::IsMouseDown(0)) return;
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
                nodeRect.Min - vec2(i * 2),
                nodeRect.Max + vec2(i * 2),
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
}

void NodeEditor::resizeNode(Node node, ImDrawList *drawList)
{
    if (node->collapsed) return;
    ImRect nodeRect = getNodeRectangle(node);
    // Triangle (a, b, c) that can be dragged to resize node:
    ImVec2 a = ImVec2(nodeRect.Max.x, nodeRect.Max.y - 20 * zoom),
            b = ImVec2(nodeRect.Max.x - 20 * zoom, nodeRect.Max.y),
            c = nodeRect.Max;
    bool mouseOverResizeTriangle = hoveringNode == node && !creatingConnection && !selecting && !currentlyDragging && ImTriangleContainsPoint(a, b, c, ImGui::GetMousePos());
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
        } else
        {
            if (currentlyResizing == node) createHistory();
            currentlyResizing = NULL;
        }
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
    bool mouseOverDragBar = hoveringNode == node && !creatingConnection && !selecting && !currentlyResizing && dragRect.Contains(ImGui::GetMousePos());

    if (mouseOverDragBar || currentlyDragging == node)
    {
        if (ImGui::IsMouseDown(0))
        {
            currentlyDragging = node;
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
            if (currentlyDragging == node) createHistory();
            currentlyDragging = NULL;
            ImGui::SetTooltip("%s", node->type->description.c_str());

            // collapse node if collapse-icon was clicked:
            ImRect collapseRect = ImRect(nodeRect.Min + vec2(8 * zoom), nodeRect.Min + vec2(24 * zoom));
            if (dragDelta.x + dragDelta.y == 0 && ImGui::IsMouseReleased(0) && !multiSelect && collapseRect.Contains(ImGui::GetMousePos()))
                node->collapsed = !node->collapsed;
        }
    }
}

void NodeEditor::drawNodeConnectors(Node node, ImDrawList *drawList)
{
    for (const auto& c : node->type->inputs) drawNodeConnector(node, c, drawList);
    for (const auto& c : node->type->outputs) drawNodeConnector(node, c, drawList);
    for (const auto& c : node->additionalInputs) drawNodeConnector(node, c, drawList);
    for (const auto& c : node->additionalOutputs) drawNodeConnector(node, c, drawList);
}

void NodeEditor::drawNodeConnector(Node node, NodeConnector c, ImDrawList *drawList)
{
    vec2 pos = connectorPosition(node, c);

    drawList->AddCircleFilled(pos, 6 * zoom, ImColor(c->valType->color));
    drawList->AddCircle(pos, 6 * zoom, ImColor((c->valType->color * vec3(.5))), 12, zoom);

    // show type of connector when hovering:
    bool hoveringConnector = hasFocus && length(vec2(ImGui::GetMousePos()) - pos) < 15 * zoom;

    bool draggingConnector = hasFocus && dragDelta.x + dragDelta.y != 0 && !currentlyDragging && !currentlyResizing
                                && length(vec2(ImGui::GetMousePos() - dragDelta) - pos) < 15 * zoom;

    if (hoveringConnector) ImGui::SetTooltip("%s", c->valType->name.c_str());

    if (hoveringConnector || draggingConnector)
    {
        bool connIsInput = isInput(node, c); // is this connector on the left side of the node?
        if (creatingConnection && hoveringConnector)
        {
            Connection connection = *creatingConnection;
            connection.dstNode = node;
            connection.input = c;
            if (connIsInput && !isConnected(node, c))
            {
                connection.srcNode->connections.push_back(connection);
                node->connections.push_back(connection);
                bool createsLoop = containsLoop();
                bool typesMatch = c->valType->any || connection.output->valType->any || connection.output->valType->name == c->valType->name;
                if (createsLoop || !ImGui::IsMouseReleased(0) || !typesMatch)
                {
                    if (!typesMatch) ImGui::SetTooltip("Invalid value type (%s -> %s)", connection.output->valType->name.c_str(), c->valType->name.c_str());
                    if (createsLoop) ImGui::SetTooltip("Creates infinite loop");

                    connection.srcNode->connections.pop_back();
                    node->connections.pop_back();
                }
                else
                {
                    creatingConnection = NULL;
                    createHistory();
                }
            }
        }
        else if (draggingConnector)
        {
            if (!connIsInput)
                creatingConnection = std::make_unique<Connection>(Connection{
                        node, NULL,
                        c, NULL
                });
            else if (isConnected(node, c))
            {
                for (auto &connection : getInputConnections(node))
                {
                    if (connection.input != c) continue;
                    // pulling existing connection out of input connector:
                    deleteConnection(connection);
                    creatingConnection = std::make_unique<Connection>(connection);
                    creatingConnection->input = NULL;
                    creatingConnection->dstNode = NULL;
                    createHistory();
                }
            }
        }
    }
    if (node->collapsed) return;
    // show connector name:
    drawList->AddText(NULL, 13 * zoom, pos, ImColor(vec4(1)), c->name.c_str());
}

std::vector<Connection> NodeEditor::getOutputConnections(Node n)
{
    std::vector<Connection> c;
    for (auto &conn : n->connections) if (conn.srcNode == n) c.push_back(conn);
    return c;
}

std::vector<Connection> NodeEditor::getInputConnections(Node n) {
    std::vector<Connection> c;
    for (auto &conn : n->connections) if (conn.dstNode == n) c.push_back(conn);
    return c;
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

            ImGui::Text("%s", ("Filter: " + filter).c_str());
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
                createHistory();

                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", nodeType->description.c_str());

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

bool NodeEditor::isConnected(Node n, NodeConnector c)
{
    for (auto &connection : getInputConnections(n))
        if (connection.input == c) return true;
    for (auto &connection : getOutputConnections(n))
        if (connection.output == c) return true;
    return false;
}

void NodeEditor::drawConnections(ImDrawList *drawList)
{
    for (auto &n : nodes)
    {
        ImColor outlineColor = n == activeNode ? ImColor(.4f, .2, 1.) : ImColor(vec4(vec3(.3), 1));
        for (auto &c : getInputConnections(n))
        {
            vec2 p0 = connectorPosition(n, c.input), p1 = connectorPosition(c.srcNode, c.output);
            float xDiff = abs(p0.x - p1.x) * .6;
            vec2 p0b = p0 - vec2(xDiff, 0), p1b = p1 + vec2(xDiff, 0);
            drawList->AddBezierCurve(p0, p0b, p1b, p1, outlineColor, zoom * 4);
            drawList->AddBezierCurve(p0, p0b, p1b, p1, ImColor(vec4(1)), zoom * 2.5);
        }
    }
}

void moveNode(Node node, Nodes &from, Nodes &to)
{
    int i = 0;
    for (i = from.size() - 1; i >= 0; i--) if (from[i] == node) break;
    from[i] = from[from.size() - 1];
    from.pop_back();
    to.push_back(node);
}

bool containsNode(Node node, Nodes &list)
{
    for (auto &n : list) if (n == node) return true;
    return false;
}

bool NodeEditor::containsLoop() // depth first search
{
    Nodes toVisit = nodes, visiting, visited;

    while (!toVisit.empty())
        if (detectLoopDfs(toVisit.back(), toVisit, visiting, visited))
            return true;
    return false;
}

bool NodeEditor::detectLoopDfs(Node curr, Nodes &toVisit, Nodes &visiting, Nodes &visited)
{
    moveNode(curr, toVisit, visiting);

    for (auto &conn : getOutputConnections(curr))
    {
        Node neighbour = conn.dstNode;
        if (containsNode(neighbour, visited)) continue;

        if (containsNode(neighbour, visiting))
            return true;
        if (detectLoopDfs(neighbour, toVisit, visiting, visited))
            return true;
    }
    moveNode(curr, visiting, visited);
    return false;
}

void NodeEditor::deleteConnection(Connection &c)
{
    for (int i = 0; i < 2; i++)
    {
        Node n = i ? c.srcNode : c.dstNode;
        n->connections.erase(
            std::remove_if(
                    n->connections.begin(), n->connections.end(), [c](auto &conn) {
                        return c.srcNode == conn.srcNode && c.dstNode == conn.dstNode && c.input == conn.input && c.output == conn.output;
                    }
            ), n->connections.end()
        );
    }
}

json NodeEditor::toJson(Nodes nodes)
{
    json out;
    int id = 0;
    for (Node n : nodes)
    {
        json nj;
        nj["id"] = id++;
        nj["type"] = n->type->name;
        nj["sizeX"] = n->size.x;
        nj["sizeY"] = n->size.y;
        nj["posX"] = n->position.x;
        nj["posY"] = n->position.y;
        nj["collapsed"] = n->collapsed;
        for (int i = 0; i < 2; i++)
        {
            auto &additional = i ? n->additionalInputs : n->additionalOutputs;
            if (additional.empty()) continue;

            json additionalJson;
            for (auto &a : additional)
            {
                json aj;
                aj["name"] = a->name;
                aj["description"] = a->description;
                aj["valType"] = a->valType->name;
                additionalJson.push_back(aj);
            }
            nj[i ? "additionalInputs" : "additionalOutputs"] = additionalJson;
        }

        if (!n->children.empty()) nj["children"] = toJson(n->children);

        json connections;
        for (auto &conn : getOutputConnections(n))
        {
            json cj;
            cj["input"] = conn.input->name;
            cj["output"] = conn.output->name;
            int dstNode;
            for (dstNode = 0; dstNode < nodes.size(); dstNode++) if (nodes[dstNode] == conn.dstNode) break;
            if (dstNode == nodes.size()) continue; // destination node not included in selection
            cj["dstNode"] = dstNode;
            connections.push_back(cj);
        }
        nj["outputConnections"] = connections;

        out.push_back(nj);
    }
    return out;
}

Nodes NodeEditor::fromJson(json input, bool &success)
{
    Nodes nodes;
    success = true;
    for (json &nodej : input)
    {
        int id = nodej["id"];
        if (nodes.size() < id + 1) nodes.resize(id + 1);

        NodeType type;
        for (auto &t : nodeTypes) if (t->name == nodej["type"]) type = t;
        if (!type)
        {
            success = false;
            return Nodes();
        }

        vec2 position = vec2(nodej["posX"], nodej["posY"]), size = vec2(nodej["sizeX"], nodej["sizeY"]);

        Nodes children;
        if (nodej.contains("children")) fromJson(nodej["children"], success);
        if (!success) return Nodes();

        std::vector<NodeConnector> additionalInputs, additionalOutputs;

        for (int i = 0; i < 2; i++)
        {
            auto &additional = i ? additionalInputs : additionalOutputs;
            json additionalj = nodej[i ? "additionalInputs" : "additionalOutputs"];

            for (json &addj : additionalj)
            {
                NodeValueType valType;
                for (auto &t : valueTypes) if (t->name == addj["valType"]) valType = t;
                if (!valType)
                {
                    success = false;
                    return Nodes();
                }
                NodeConnector connector = createNodeConnector({addj["name"], addj["description"], valType});
                additional.push_back(connector);
            }
        }
        nodes[id] = createNode({
            type, position, size, nodej["collapsed"], additionalInputs, additionalOutputs, std::vector<Connection>(), children
        });
    }
    for (json &nodej : input)
    {
        int id = nodej["id"];

        for (json &connj : nodej["outputConnections"])
        {
            Connection conn;
            conn.srcNode = nodes[id];
            conn.dstNode = nodes[connj["dstNode"]];
            conn.input = connectorByName(conn.dstNode, connj["input"]);
            conn.output = connectorByName(conn.srcNode, connj["output"]);

            nodes[id]->connections.push_back(conn);
            conn.dstNode->connections.push_back(conn);
        }
    }
    return nodes;
}

NodeConnector NodeEditor::connectorByName(Node n, std::string name)
{
    for (const auto& c : n->type->inputs) if (c->name == name) return c;
    for (const auto& c : n->type->outputs) if (c->name == name) return c;
    for (const auto& c : n->additionalInputs) if (c->name == name) return c;
    for (const auto& c : n->additionalOutputs) if (c->name == name) return c;
    return NULL;
}

void NodeEditor::createHistory()
{
    while (historyI != history.size() - 1)
        history.pop_back();
    history.push_back(toJson(nodes));
    historyI = history.size() - 1;
    if (history.size() > 128)
    {
        int erase = 12;
        history.erase(history.begin(), history.begin() + erase);
        historyI -= erase;
    }
}

void NodeEditor::undo()
{
    if (historyI <= 0) return;
    bool success;
    Nodes oldNodes = fromJson(history[historyI - 1], success);
    if (success)
    {
        historyI--;
        nodes = oldNodes;
    }
}

void NodeEditor::redo()
{
    if (historyI >= history.size() - 1) return;
    bool success;
    Nodes newerNodes = fromJson(history[historyI + 1], success);
    if (success)
    {
        historyI++;
        nodes = newerNodes;
    }
}

