#include <orca.h>

#include "orca_ext.h"

#include "graphics/graphics.h"
#include "platform/platform_subprocess.h"
#include "util/debug.h"
#include "util/memory.h"
#include "util/strings.h"

oc_surface surface;
oc_canvas canvas;
oc_font font;

oc_arena appArena;
oc_arena frameArena;

void work();
void draw();

typedef enum AppState {
  INITIAL,
  LOADING,
  ACTIVE,
} AppState;
AppState appState;
AppState nextAppState;

oc_str8_list linebreaks = {0};

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

  oc_arena_init(&appArena);
  oc_arena_init(&frameArena);

  oc_str8_list_push(&appArena, &linebreaks, OC_STR8("\n"));
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
  oc_arena_clear(&frameArena);

  work();
  draw();

  appState = nextAppState;
}

void work() {
  oc_arena_scope scratch = oc_scratch_begin();

  switch (appState) {
  case INITIAL: {
    nextAppState = LOADING;
  } break;
  case LOADING: {
    oc_str8 allCommits = oc_run_cmd(
        &frameArena, OC_STR8("git log \"--format=%H%n%ct%n%an%n%s\""));
    oc_str8_list commitLines =
        oc_str8_split(&frameArena, allCommits, linebreaks);
    oc_log_info("total output length: %d. num lines: %d\n", allCommits.len,
                commitLines.eltCount);

    oc_str8 hash;
    oc_str8 timestamp;
    oc_str8 authorName;
    oc_str8 summary;
    int numParts = 4;
    int i = 0;
    oc_str8_list_for(commitLines, it) {
      switch (i % numParts) {
      case 0:
        hash = it->string;
        break;
      case 1:
        timestamp = it->string;
        break;
      case 2:
        authorName = it->string;
        break;
      case 3:
        summary = it->string;
        break;
      }
      i = (i + 1) % numParts;

      if (i == 0) {
        // Finished a commit, store it somewhere or whatever
        oc_log_info("hash: %.*s\n", oc_str8_printf(hash));
        oc_log_info("timestamp: %.*s\n", oc_str8_printf(timestamp));
        oc_log_info("authorName: %.*s\n", oc_str8_printf(authorName));
        oc_log_info("summary: %.*s\n", oc_str8_printf(summary));
      }
    }
    oc_log_info("ok no more commits");
    nextAppState = ACTIVE;
  } break;
  default:
    break;
  }

  oc_scratch_end(scratch);
}

void draw() {
  oc_arena_scope scratch = oc_scratch_begin();

  oc_canvas_select(canvas);

  oc_set_color_rgba(0.95f, 0.95f, 0.95f, 1);
  oc_clear();

  oc_set_color_rgba(0, 0, 0, 1);
  oc_set_font(font);
  oc_set_font_size(fontSize);

  switch (appState) {
  case INITIAL: {
    oc_set_color_rgba(0, 0, 0, 1);
    oc_set_font(font);
    oc_set_font_size(fontSize);
    oc_move_to(10, 30);
    oc_text_outlines(OC_STR8("Loading commits..."));
    oc_fill();
  } break;
  default:
    break;
  }

  oc_surface_select(surface);
  oc_render(canvas);
  oc_surface_present(surface);

  oc_scratch_end(scratch);
}
