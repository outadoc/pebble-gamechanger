#include <pebble.h>
#include <ctype.h>

// Basalt and Chalk have noticeably smaller screens than Emery/Gabbro, so every
// size constant below gets a smaller value for them instead of scaling off the window
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_CHALK)
#define DEVICE_IS_SMALL
#endif

// Striped background
#define NUM_STRIPES 8

// Roundrect frame (centered on screen, width drives height via a fixed aspect ratio)
#define ROUNDRECT_SIZE_RATIO 0.7

#ifdef DEVICE_IS_SMALL
#define ROUNDRECT_WIDTH 130
#define ROUNDRECT_RADIUS_OUTER 18
#define ROUNDRECT_RADIUS_INNER 14
#define ROUNDRECT_BORDER_WIDTH 14
#else
#define ROUNDRECT_WIDTH 180
#define ROUNDRECT_RADIUS_OUTER 25
#define ROUNDRECT_RADIUS_INNER 20
#define ROUNDRECT_BORDER_WIDTH 20
#endif

#define ROUNDRECT_HEIGHT (ROUNDRECT_WIDTH * ROUNDRECT_SIZE_RATIO)

// Time + date text layers (centered as a group)
#ifdef DEVICE_IS_SMALL
#define TIME_LAYER_HEIGHT 46
#define DATE_LAYER_HEIGHT 22
// Nudges the group up slightly to compensate for the time font's line-height
#define GROUP_VERTICAL_OFFSET -3
#else
#define TIME_LAYER_HEIGHT 60
#define DATE_LAYER_HEIGHT 30
#define GROUP_VERTICAL_OFFSET -5
#endif

static const GColor STRIPE_COLORS[] = {
    GColorRed,
    GColorOrange,
    GColorChromeYellow,
    GColorPictonBlue,
    GColorDarkCandyAppleRed,
};
static const int NUM_STRIPE_COLORS = sizeof(STRIPE_COLORS) / sizeof(STRIPE_COLORS[0]);

static GFont s_title_font;
static GFont s_body_font;
static Window *s_main_window;
static Layer *s_window_layer;
static Layer *s_background_layer;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;

static void update_time()
{
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_time_buffer[8];
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_time_buffer);

  // Write the current date into a buffer, in uppercase
  static char s_date_buffer[16];
  strftime(s_date_buffer, sizeof(s_date_buffer), "%a %b %d", tick_time);
  for (char *c = s_date_buffer; *c != '\0'; c++)
  {
    *c = toupper((unsigned char)*c);
  }

  // Display the date
  text_layer_set_text(s_date_layer, s_date_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  update_time();
}

static GRect rect_centered_in(GRect bounds, int width, int height)
{
  return GRect(
      bounds.origin.x + (bounds.size.w - width) / 2,
      bounds.origin.y + (bounds.size.h - height) / 2,
      width,
      height);
}

static void background_update_proc(Layer *layer, GContext *ctx)
{
  // Stripes must always fill the whole window, not just the unobstructed area
  GRect bounds = layer_get_bounds(layer);
  int stripe_height = bounds.size.h / NUM_STRIPES;

  for (int i = 0; i < NUM_STRIPES; i++)
  {
    // Make the last stripe absorb any leftover rounding pixels
    int height = (i == NUM_STRIPES - 1) ? (bounds.size.h - stripe_height * i) : stripe_height;

    graphics_context_set_fill_color(ctx, STRIPE_COLORS[i % NUM_STRIPE_COLORS]);
    graphics_fill_rect(ctx, GRect(bounds.origin.x, bounds.origin.y + stripe_height * i, bounds.size.w, height), 0, GCornerNone);
  }

  // The roundrect content, on the other hand, should shift within the unobstructed area
  GRect unobstructed_bounds = layer_get_unobstructed_bounds(layer);

  // Draw a black roundrect centered on the unobstructed area
  GRect roundrect_bounds = rect_centered_in(unobstructed_bounds, ROUNDRECT_WIDTH, ROUNDRECT_HEIGHT);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, roundrect_bounds, ROUNDRECT_RADIUS_OUTER, GCornersAll);

  // Draw a white roundrect on top of it
  GRect roundrect2_bounds = rect_centered_in(
      unobstructed_bounds,
      ROUNDRECT_WIDTH - ROUNDRECT_BORDER_WIDTH,
      ROUNDRECT_HEIGHT - ROUNDRECT_BORDER_WIDTH);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, roundrect2_bounds, ROUNDRECT_RADIUS_INNER, GCornersAll);
}

// Positions the time+date group, centered as a whole, within the given bounds
static void layout_time_date_layers(GRect bounds)
{
  int group_top = bounds.origin.y + (bounds.size.h - (TIME_LAYER_HEIGHT + DATE_LAYER_HEIGHT)) / 2 + GROUP_VERTICAL_OFFSET;

  layer_set_frame(text_layer_get_layer(s_time_layer), GRect(0, group_top, bounds.size.w, TIME_LAYER_HEIGHT));
  layer_set_frame(text_layer_get_layer(s_date_layer), GRect(0, group_top + TIME_LAYER_HEIGHT, bounds.size.w, DATE_LAYER_HEIGHT));
}

static void unobstructed_area_change_handler(AnimationProgress progress, void *context)
{
  // Redraw the background so the roundrect tracks the animated unobstructed area
  layer_mark_dirty(s_background_layer);

  // Re-run layout every frame of the animation so the time+date group shifts smoothly
  layout_time_date_layers(layer_get_unobstructed_bounds(s_window_layer));
}

static void main_window_load(Window *window)
{
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_window_layer = window_layer;

#ifdef DEVICE_IS_SMALL
  s_title_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_MOUSEMEMOIRS_46));
  s_body_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_TRADE_GOTHIC_LT_STD_BOLD_CONDENSED_18));
#else
  s_title_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_MOUSEMEMOIRS_58));
  s_body_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_TRADE_GOTHIC_LT_STD_BOLD_CONDENSED_23));
#endif

  // Create the striped background Layer
  s_background_layer = layer_create(bounds);
  layer_set_update_proc(s_background_layer, background_update_proc);

  // Create the time TextLayer
  s_time_layer = text_layer_create(GRectZero);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_font(s_time_layer, s_title_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Create the date TextLayer
  s_date_layer = text_layer_create(GRectZero);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_font(s_date_layer, s_body_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  // Center the time+date group as a whole within whatever area is unobstructed right now
  layout_time_date_layers(layer_get_unobstructed_bounds(window_layer));

  // Add layers to the Window
  layer_add_child(window_layer, s_background_layer);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // Keep the content centered as the Timeline Quick View obstruction appears/disappears
  UnobstructedAreaHandlers unobstructed_handlers = {
      .change = unobstructed_area_change_handler,
  };
  unobstructed_area_service_subscribe(unobstructed_handlers, NULL);
}

static void main_window_unload(Window *window)
{
  unobstructed_area_service_unsubscribe();

  // Destroy TextLayers
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);

  // Destroy background Layer
  layer_destroy(s_background_layer);

  fonts_unload_custom_font(s_title_font);
  fonts_unload_custom_font(s_body_font);
}

static void init()
{
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set the background color
  window_set_background_color(s_main_window, GColorBlack);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers){
                                                .load = main_window_load,
                                                .unload = main_window_unload});

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Make sure the time is displayed from the start
  update_time();

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit()
{
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void)
{
  init();
  app_event_loop();
  deinit();
}
