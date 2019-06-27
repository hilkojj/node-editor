
#include "glm/glm.hpp"
using namespace glm;

#include "../json.hpp"
using json = nlohmann::json;

// implicit conversions for glm::vec2 <-> ImVec2
#define IM_VEC2_CLASS_EXTRA \
        ImVec2(const vec2& f) { x = f.x; y = f.y; } \
        operator vec2() const { return vec2(x, y); }


// implicit conversions for glm::vec3/vec4 <-> ImVec4/ImColor
#define IM_VEC4_CLASS_EXTRA \
    ImVec4(const vec4& f) { x = f.r; y = f.g; z = f.b; w = f.a; } \
    operator vec4() const { return vec4(x,y,z,w); } \
    ImVec4(const vec3& f) { x = f.r; y = f.g; z = f.b; w = 1.f; } \
    operator vec3() const { return vec3(x,y,z); }

#include "imgui.h"
#include "imgui_internal.h"
