#include <orca.h>

#include "orca_ext.h"

#include "graphics/graphics.h"
#include "platform/platform_debug.h"
#include "platform/platform_subprocess.h"
#include "util/algebra.h"
#include "util/debug.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/strings.h"

#include "commits.h"
#include "graph.h"

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

CommitTable commits;
NodeList nodes;

oc_str8_list newlines = {0};
oc_str8_list spaces = {0};

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

  oc_str8_list_push(&appArena, &newlines, OC_STR8("\n"));
  oc_str8_list_push(&appArena, &spaces, OC_STR8(" "));

  CommitTableInit(&commits);
}

f32 scroll = 0;
f32 actualScroll = 0;

// ORCA_EXPORT void oc_on_mouse_down(oc_mouse_button button) { dragging = true;
// } ORCA_EXPORT void oc_on_mouse_up(oc_mouse_button button) { dragging = false;
// }
// ORCA_EXPORT void oc_on_mouse_move(f32 x, f32 y, f32 deltaX, f32 deltaY) {
//   if (dragging) {
//     pos.x += deltaX;
//     pos.y += deltaY;
//   }
// }
ORCA_EXPORT void oc_on_mouse_wheel(f32 deltaX, f32 deltaY) { scroll -= deltaY; }

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
        &frameArena, OC_STR8("git log --all \"--format=%H%n%an%n%P%n%s\""));
    oc_str8_list commitLines = oc_str8_split(&frameArena, allCommits, newlines);

    oc_str8 hash;
    oc_str8 authorName;
    oc_str8 parentHashesStr;
    oc_str8 summary;
    int numParts = 4;

    int i = 0;
    oc_str8_list_for(commitLines, it) {
      switch (i % numParts) {
      case 0: {
        hash = it->string;
      } break;
      case 1: {
        authorName = it->string;
      } break;
      case 2: {
        parentHashesStr = it->string;
      } break;
      case 3: {
        summary = it->string;
      } break;
      }
      i = (i + 1) % numParts;

      if (i == 0) {
        // Finished a commit, store it
        Commit *commit = oc_arena_push_type(&appArena, Commit);
        memset(commit, 0, sizeof(Commit));
        commit->hash = oc_str8_push_copy(&appArena, hash);
        commit->authorName = oc_str8_push_copy(&appArena, authorName);
        commit->summary = oc_str8_push_copy(&appArena, summary);

        oc_str8_list parentHashes =
            oc_str8_split(scratch.arena, parentHashesStr, spaces);
        oc_str8_list_for(parentHashes, it) {
          if (it->string.len > 0) {
            oc_str8_list_push(&appArena, &commit->parents,
                              oc_str8_push_copy(&appArena, it->string));
          }
        }

        CommitTableInsert(&commits, commit);
        // oc_log_info("stored commit:\n");
        // log_commit(commit);
      }
    }
    oc_log_info("stored %d commits\n", commits.count);

    // First compute depths for all commits
    oc_log_info("computing commit metadata (depth + tracks)\n");
    for (i32 b = 0; b < HASH_NUM_BUCKETS; b++) {
      for (Commit *commit = commits.buckets[b]; commit != NULL;
           commit = commit->_hashNext) {
        initCommitMetadata(&commits, commit, false);
      }
    }

    // Now sort them into tracks by leaf commits
    // wow I love looping over my "hash table" like this it's so good
    oc_log_info("sorting commits into tracks\n");
    int currentTrack = 0;
    for (i32 b = 0; b < HASH_NUM_BUCKETS; b++) {
      for (Commit *commit = commits.buckets[b]; commit != NULL;
           commit = commit->_hashNext) {
        if (commit->hasChildren) {
          continue;
        }

        currentTrack += 1;
        setCommitTrack(&commits, commit, currentTrack);
      }
    }

    // Now create graph nodes for each?? I guess??
    oc_log_info("creating graph nodes\n");
    NodeListInit(&appArena, &nodes, commits.count);
    for (i32 b = 0; b < HASH_NUM_BUCKETS; b++) {
      for (Commit *commit = commits.buckets[b]; commit != NULL;
           commit = commit->_hashNext) {
        NodeListPush(&nodes, (Node){
                                 .commit = commit,
                             });
      }
    }

    nextAppState = ACTIVE;
  } break;
  case ACTIVE: {
    actualScroll = lerp(actualScroll, scroll, 0.2);
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

  oc_set_font(font);

  switch (appState) {
  case INITIAL: {
    oc_set_color_rgba(0, 0, 0, 1);
    oc_set_font(font);
    oc_set_font_size(20);
    oc_move_to(10, 30);
    oc_text_outlines(OC_STR8("Loading commits..."));
    oc_fill();
  } break;
  case ACTIVE: {
    int maxDepth = 0;
    for (int i = 0; i < nodes.count; i++) {
      Node *node = &nodes.nodes[i];
      maxDepth = oc_max_i32(maxDepth, node->commit->depth);
    }

    oc_matrix_push(oc_mat2x3_translate(0, actualScroll));
    {
      for (int i = 0; i < nodes.count; i++) {
        Node *node = &nodes.nodes[i];

        f32 x = node->commit->track * 30;
        f32 y = (maxDepth - node->commit->depth + 1) * 30;
        oc_set_color_rgba(0, 0, 0, 1);
        oc_circle_fill(x, y, 5);

        f32 fontSize = 18;
        oc_move_to(x + 15, y + fontSize / 2 - 3); // dunno
        oc_set_font_size(fontSize);
        oc_text_outlines(node->commit->summary);
        oc_fill();
      }
    }
    oc_matrix_pop();
  } break;
  default:
    break;
  }

  oc_surface_select(surface);
  oc_render(canvas);
  oc_surface_present(surface);

  oc_scratch_end(scratch);
}
