
#include "glm/glm.hpp"
using namespace glm;

// implicit conversions for glm::vec2 <-> ImVec2
#define IM_VEC2_CLASS_EXTRA \
        ImVec2(const vec2& f) { x = f.x; y = f.y; } \
        operator vec2() const { return vec2(x, y); }

#include "imgui.h"
#include "imgui_internal.h"
