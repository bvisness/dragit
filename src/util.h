#pragma once

#include "util/typedefs.h"

bool hit_test_center_rect(oc_vec2 pos, oc_vec2 center, f32 w, f32 h) {
    return center.x - w <= pos.x &&
              pos.x <= center.x + w &&
              center.y - h <= pos.y &&
              pos.y <= center.y + h;
}
