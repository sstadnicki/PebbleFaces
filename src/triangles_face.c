#include <pebble.h>
  
#define NUM_BACKGROUND_TRIANGLE_SETS 3
#define GRID_SIZE 5
#define NUM_BACKGROUND_TRIANGLES (NUM_BACKGROUND_TRIANGLE_SETS*GRID_SIZE)
#define HOUR_HAND_LENGTH 30
#define MINUTE_HAND_LENGTH (2*HOUR_HAND_LENGTH)
#define EDGE_WIDTH 4
static bool FILL_TRIS=false;
#define MIN_HOUR_HAND_LENGTH (HOUR_HAND_LENGTH/2)
#define MAX_HOUR_HAND_LENGTH (2*HOUR_HAND_LENGTH)

// A ClockTriangle holds everything we need to render a
// single triangle for the clockface.
  
typedef struct {
  GPoint center;
  GPoint hour_hand;
  GPoint minute_hand;
  GColor color;
} ClockTriangle;

// We keep an array of background versions of this around
static ClockTriangle s_background_triangles[NUM_BACKGROUND_TRIANGLES];
// Along with one that represents the current time
static ClockTriangle s_foreground_triangle;
// A simple path to render triangles with
static GPathInfo s_render_tri_path_info = {
  .num_points = 3,
  .points = (GPoint[]) {{0,0},{0,0},{0,0}}
};
static GPath *s_render_tri_path = NULL;


// Main window used by the project - there only needs to be one!
static Window * s_main_window;
// A canvas layer for drawing into
static Layer * s_canvas_layer;
// The center of the display
GPoint s_center;

// Generate a random bright color - this is done by randomly choosing a point
// on the 'middle edges' of the RGB cube - that is, those between the solid
// 'primary' colors (R, G, B) and the solid 'secondary' colors (C, M, Y).
static void generateRandomBrightColor(GColor *outColor) {
  unsigned r=0, g=0, b=0;
  unsigned edge = rand() % 6;
  unsigned dist = rand() % 256;
  switch (edge) {
  case 0: // (1,0,0) -> (1, 1, 0)
    r = 255; g = dist; b = 0;
    break;
  case 1: // (1, 1, 0) -> (0, 1, 0)
    r = dist; g = 255; b = 0;
    break;
  case 2: // (0, 1, 0) -> (0, 1, 1)
    r = 0; g = 255; b = dist;
    break;
  case 3: // (0, 1, 1) -> (0, 0, 1)
    r = 0; g = dist; b = 255;
    break;
  case 4: // (0, 0, 1) -> (1, 0, 1)
    r = dist; g = 0; b = 255;
    break;
  case 5: // (1, 0, 1) -> (1, 0, 0)
    r = 255; g = 0; b = dist;
    break;
  }
  *outColor = GColorFromRGB(r,g,b);
}

// Create a clock triangle from the appropriate parameters
static void generateClockTriangle(
  int hour,
  int minute,
  GColor color,
  GPoint center,
  ClockTriangle *out_triangle)
{
  out_triangle->center = center;
  out_triangle->color = color;
  float minute_angle = TRIG_MAX_ANGLE * minute / 60;
  float hour_angle = TRIG_MAX_ANGLE * hour / 12;
  // Make sure to advance the hour angle by the number of minutes!
  hour_angle += minute_angle/12;
  // Position the hour hand
  out_triangle->hour_hand.x = center.x + (sin_lookup(hour_angle) * HOUR_HAND_LENGTH)/TRIG_MAX_RATIO;
  out_triangle->hour_hand.y = center.y - (cos_lookup(hour_angle) * HOUR_HAND_LENGTH)/TRIG_MAX_RATIO;
  // Position the minute hand
  out_triangle->minute_hand.x = center.x + (sin_lookup(minute_angle) * MINUTE_HAND_LENGTH)/TRIG_MAX_RATIO;
  out_triangle->minute_hand.y = center.y - (cos_lookup(minute_angle) * MINUTE_HAND_LENGTH)/TRIG_MAX_RATIO;
}
  
// Render a triangle to the graphics context
static void drawClockTriangle(ClockTriangle * triangle, GContext *ctx, bool is_primary) {
  s_render_tri_path->points[0] = triangle->center;
  s_render_tri_path->points[1] = triangle->hour_hand;
  s_render_tri_path->points[2] = triangle->minute_hand;
  if (FILL_TRIS) {
    graphics_context_set_fill_color(ctx, triangle->color);
    gpath_draw_filled(ctx, s_render_tri_path);
    if (is_primary) {
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_context_set_stroke_width(ctx, EDGE_WIDTH);
      gpath_draw_outline(ctx, s_render_tri_path);
    }
  } else {
    graphics_context_set_stroke_color(ctx, (is_primary)?GColorWhite:triangle->color);
    graphics_context_set_stroke_width(ctx, EDGE_WIDTH);
    gpath_draw_outline(ctx, s_render_tri_path);
  }
//  graphics_draw_line(ctx, triangle->center, triangle->hour_hand);
//  graphics_draw_line(ctx, triangle->hour_hand, triangle->minute_hand);
//  graphics_draw_line(ctx, triangle->minute_hand, triangle->center);
}

static void drawClockFace(GContext *ctx) {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  GColor random_color;
  generateRandomBrightColor(&random_color);
  generateClockTriangle(
    tick_time->tm_hour,
    tick_time->tm_min,
    random_color,
    s_center,
    &s_foreground_triangle);
  // Render all of our background triangles
  for (int tri_idx = 0; tri_idx < NUM_BACKGROUND_TRIANGLES; tri_idx++) {
    drawClockTriangle(&(s_background_triangles[tri_idx]), ctx, false);
  }
  drawClockTriangle(&s_foreground_triangle, ctx, true);
}

static void initBackgroundTriangles() {
  static int shuffle_array[GRID_SIZE];
  int tri_idx=0;
  for (int idx = 0; idx < GRID_SIZE; idx++)
    shuffle_array[idx] = idx;
  for (int set_idx = 0; set_idx < NUM_BACKGROUND_TRIANGLE_SETS; set_idx++) {
    // shuffle our array
    for (int shuffle_idx = 0; shuffle_idx < GRID_SIZE-1; shuffle_idx++) {
      int other_idx = shuffle_idx+(rand() % (GRID_SIZE-1-shuffle_idx));
      int temp = shuffle_array[shuffle_idx];
      shuffle_array[shuffle_idx] = shuffle_array[other_idx];
      shuffle_array[other_idx] = temp;
    }
    // Now add triangles in the various grid boxes
    for (int grid_idx = 0; grid_idx < GRID_SIZE; grid_idx++) {
      int x_offset = (144*grid_idx)/GRID_SIZE;
      int y_offset = (168*shuffle_array[grid_idx])/GRID_SIZE;
      x_offset += (rand()%(144/GRID_SIZE));
      y_offset += (rand()%(168/GRID_SIZE));
      GColor random_color;
      generateRandomBrightColor(&random_color);
      int random_hour = rand() % 12;
      int random_minutes = random_hour*5;
      int minuteoffset = 4+(rand()%19); // 4..22 - a restriction of 0..30
      random_minutes = (rand()&1) ? random_minutes-minuteoffset : random_minutes+minuteoffset;
      random_minutes = (random_minutes+60)%60; // clamp to 0..60 range
      generateClockTriangle(
        random_hour,
        random_minutes,
        random_color,
        (GPoint) {.x = x_offset, .y = y_offset},
        &(s_background_triangles[tri_idx])
      );
      tri_idx++;
  
    }
  }
}

///////////////////////////////////////////////////////////////////
// System/framework functions                                    //
///////////////////////////////////////////////////////////////////
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void updateProc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
  drawClockFace(ctx);
}

static void windowLoad(Window *window) {
 Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);
  s_center = grect_center_point(&window_bounds);
  s_canvas_layer = layer_create(GRect(0, 0, 144, 168));
  layer_set_update_proc(s_canvas_layer, updateProc);
  layer_add_child(window_layer, s_canvas_layer);
  s_render_tri_path = gpath_create(&s_render_tri_path_info);
}

static void windowUnload(Window *window) {
  gpath_destroy(s_render_tri_path);
  layer_destroy(s_canvas_layer);
}

static void init() {
  // Initialize the RNG
  srand(time(NULL));
  
  // Set up a bunch of background triangles
  initBackgroundTriangles();
  
  // Initialize the window
  s_main_window = window_create();
  
  // Set up handlers
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = windowLoad,
    .unload = windowUnload
  });
  
  // show the window (animating)
  window_stack_push(s_main_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit() {
  // free the window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}