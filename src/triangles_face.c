#include <pebble.h>
  
#define NUM_BACKGROUND_TRIANGLES 18
#define HOUR_HAND_LENGTH 30
#define MINUTE_HAND_LENGTH (2*HOUR_HAND_LENGTH)
#define EDGE_WIDTH 4
static bool FILL_TRIS=true;
static bool RANDOMIZE_SIZE=true;
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

#define NUM_PRIMES 4
static int s_primes[NUM_PRIMES] = {2, 3, 5, 7};

// Computes a 'Halton value' given a number and a base - this is the 'reverse'
// of the number, in the given base, as a value between 0 and 1.
static float generateHaltonValue(int number, int base) {
  int temp_number = number;
  float inverse_base = 1.0f/((float)base);
  float current_position = inverse_base;
  float output = 0.0f;
  while ( temp_number > 0 ) {
    output += current_position * (temp_number % base);
    temp_number /= base;
    current_position *= inverse_base;
  }
  return output;
}

// Generate a bright color.  This is done by converting the float 'hue angle'
// (in the range [0..1] to a point on the 'middle edges' of the RGB cube - 
// that is, those between the solid 'primary' colors (R, G, B) and the solid
// 'secondary' colors (C, M, Y).  (This is approximately equal to choosing a
// point with the passed-in value as hue, full saturation and full value.)
static void generateBrightColor(float hue_angle, GColor *out_color) {
  unsigned r=0, g=0, b=0;
  float expanded_hue = 6.0f * hue_angle;
  int edge = (unsigned) (expanded_hue);
  if ( (edge < 0) || (edge > 5) ) {
    // our input value was out of range; return black as a 'sentinel'
    *out_color = GColorBlack;
    return;
  }
  int dist = (unsigned) (256.0f*(expanded_hue-edge));
  switch (edge) {
  case 0: // (1,0,0) -> (1, 1, 0)
    r = 255; g = dist; b = 0;
    break;
  case 1: // (1, 1, 0) -> (0, 1, 0)
    r = 255-dist; g = 255; b = 0;
    break;
  case 2: // (0, 1, 0) -> (0, 1, 1)
    r = 0; g = 255; b = dist;
    break;
  case 3: // (0, 1, 1) -> (0, 0, 1)
    r = 0; g = 255-dist; b = 255;
    break;
  case 4: // (0, 0, 1) -> (1, 0, 1)
    r = dist; g = 0; b = 255;
    break;
  case 5: // (1, 0, 1) -> (1, 0, 0)
    r = 255; g = 0; b = 255-dist;
    break;
  }
  *out_color = GColorFromRGB(r,g,b);
}

// Create a clock triangle from the appropriate parameters
static void generateClockTriangle(
  int hour,
  int minute,
  float size,
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
  int hour_hand_length = (int) (size*HOUR_HAND_LENGTH);
  int minute_hand_length = (int) (size*MINUTE_HAND_LENGTH);
  // Position the hour hand
  out_triangle->hour_hand.x = center.x + (sin_lookup(hour_angle) * hour_hand_length)/TRIG_MAX_RATIO;
  out_triangle->hour_hand.y = center.y - (cos_lookup(hour_angle) * hour_hand_length)/TRIG_MAX_RATIO;
  // Position the minute hand
  out_triangle->minute_hand.x = center.x + (sin_lookup(minute_angle) * minute_hand_length)/TRIG_MAX_RATIO;
  out_triangle->minute_hand.y = center.y - (cos_lookup(minute_angle) * minute_hand_length)/TRIG_MAX_RATIO;
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
  float random_hue = ((float) rand())/((float) RAND_MAX);
  generateBrightColor(random_hue, &random_color);
  generateClockTriangle(
    tick_time->tm_hour,
    tick_time->tm_min,
    1.0f,
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
  // Decide on a 'starting value' for our Halton grid
  int halton_index = 256+(rand()&127);
  // shuffle our primes
  for (int prime_idx = 0; prime_idx < NUM_PRIMES-1; prime_idx++) {
    int random_idx = rand() % (4-prime_idx);
    int temp = s_primes[prime_idx];
    s_primes[prime_idx] = s_primes[random_idx];
    s_primes[random_idx] = temp;
  }
  int x_base = s_primes[0];
  int y_base = s_primes[1];
  int size_base = s_primes[2];
  int color_base = s_primes[3];
  // Now build all the triangles
  for (int tri_idx = 0; tri_idx < NUM_BACKGROUND_TRIANGLES; tri_idx++) {
    int x_offset = (int) (144.0f * generateHaltonValue(halton_index, x_base));
    int y_offset = (int) (168.0f * generateHaltonValue(halton_index, y_base));
    float size_uniform = generateHaltonValue(halton_index, size_base);
    float random_size = (size_uniform < 0.5f) ? size_uniform+0.5f : size_uniform*2.0f;
    float random_hue = generateHaltonValue(halton_index, color_base);
    GColor random_color;
    generateBrightColor(random_hue, &random_color);
    int random_hour = rand() % 12;
    int random_minutes = random_hour*5;
    int minuteoffset = 4+(rand()%19); // 4..22 - a restriction of 0..30
    random_minutes = (rand()&1) ? random_minutes-minuteoffset : random_minutes+minuteoffset;
    random_minutes = (random_minutes+60)%60; // clamp to 0..60 range
    generateClockTriangle(
      random_hour,
      random_minutes,
      (RANDOMIZE_SIZE)?random_size:1.0f,
      random_color,
      (GPoint) {.x = x_offset, .y = y_offset},
      &(s_background_triangles[tri_idx])
    );
    halton_index++;
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