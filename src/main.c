#include <orca.h>

oc_surface surface;
oc_canvas canvas;

oc_font font;

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
}

ORCA_EXPORT void oc_on_frame_refresh(void) {
  oc_arena_scope scratch = oc_scratch_begin();

  oc_canvas_select(canvas);

  oc_set_color_rgba(0.9f, 0.9f, 0.9f, 1);
  oc_clear();

  oc_set_color_rgba(0, 0, 0, 1);
  oc_set_font(font);
  oc_set_font_size(18);
  oc_move_to(10.0f, 30.0f);
  oc_text_outlines(OC_STR8("Hello, Orca!"));
  oc_fill();

  oc_surface_select(surface);
  oc_render(canvas);
  oc_surface_present(surface);

  oc_scratch_end(scratch);
}
