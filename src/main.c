#include "graphics/graphics.h"
#include "platform/platform_subprocess.h"
#include "util/algebra.h"
#include "util/strings.h"
#include <orca.h>

oc_surface surface;
oc_canvas canvas;

oc_font font;

oc_arena cmdArena;
oc_str8 cmdResult;

f32 lerp(f32 a, f32 b, f32 t) { return (1 - t) * a + t * b; }

ORCA_EXPORT void oc_on_init(void) {
  oc_window_set_title(OC_STR8("Dragit"));

  surface = oc_surface_canvas();
  canvas = oc_canvas_create();

  oc_unicode_range ranges[5] = {
      OC_UNICODE_BASIC_LATIN, OC_UNICODE_C1_CONTROLS_AND_LATIN_1_SUPPLEMENT,
      OC_UNICODE_LATIN_EXTENDED_A, OC_UNICODE_LATIN_EXTENDED_B,
      OC_UNICODE_SPECIALS};
  font = oc_font_create_from_path(OC_STR8("/JetBrainsMono-Regular.ttf"), 5,
                                  ranges);

  oc_arena_init(&cmdArena);
  cmdResult = oc_run_cmd(&cmdArena, OC_STR8("git status"));
}

f32 fontSize = 18.0f;
oc_vec2 pos = {10.0f, 30.0f};
bool dragging = false;
f32 rot = 0.0f;
f32 actualRot = 0.0f;

ORCA_EXPORT void oc_on_mouse_down(oc_mouse_button button) { dragging = true; }
ORCA_EXPORT void oc_on_mouse_up(oc_mouse_button button) { dragging = false; }
ORCA_EXPORT void oc_on_mouse_move(f32 x, f32 y, f32 deltaX, f32 deltaY) {
  if (dragging) {
    pos.x += deltaX;
    pos.y += deltaY;
  }
}
ORCA_EXPORT void oc_on_mouse_wheel(f32 deltaX, f32 deltaY) {
  fontSize += -deltaY * 0.1;
  rot += deltaX * 0.004;
}

ORCA_EXPORT void oc_on_frame_refresh(void) {
  oc_arena_scope scratch = oc_scratch_begin();

  oc_canvas_select(canvas);

  oc_set_color_rgba(0.9f, 0.9f, 0.9f, 1);
  oc_clear();

  oc_set_color_rgba(0, 0, 0, 1);
  oc_set_font(font);
  oc_set_font_size(fontSize);

  actualRot = lerp(actualRot, rot, 0.1);
  oc_mat2x3 transform = oc_mat2x3_mul_m(oc_mat2x3_translate(pos.x, pos.y),
                                        oc_mat2x3_rotate(actualRot));

  oc_matrix_push(transform);
  {
    f32 textY = 0;
    oc_str8_list linebreaks = {0};
    oc_str8_list_push(scratch.arena, &linebreaks, OC_STR8("\n"));
    oc_str8_list lines = oc_str8_split(scratch.arena, cmdResult, linebreaks);
    oc_list_for(lines.list, it, oc_str8_elt, listElt) {
      oc_move_to(0, textY);
      oc_text_outlines(it->string);
      oc_fill();
      textY += fontSize;
    }
  }
  oc_matrix_pop();

  oc_surface_select(surface);
  oc_render(canvas);
  oc_surface_present(surface);

  oc_scratch_end(scratch);
}
