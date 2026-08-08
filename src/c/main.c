#include <pebble.h>

#define NUM_STRIPES 8

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

  // Write the current date into a buffer
  static char s_date_buffer[16];
  strftime(s_date_buffer, sizeof(s_date_buffer), "%a %b %d", tick_time);

  // Display the date
  text_layer_set_text(s_date_layer, s_date_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  update_time();
}

static void background_update_proc(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);
  int stripe_height = bounds.size.h / NUM_STRIPES;

  for (int i = 0; i < NUM_STRIPES; i++)
  {
    // Make the last stripe absorb any leftover rounding pixels
    int height = (i == NUM_STRIPES - 1) ? (bounds.size.h - stripe_height * i) : stripe_height;

    graphics_context_set_fill_color(ctx, STRIPE_COLORS[i % NUM_STRIPE_COLORS]);
    graphics_fill_rect(ctx, GRect(bounds.origin.x, bounds.origin.y + stripe_height * i, bounds.size.w, height), 0, GCornerNone);
  }
}

static void main_window_load(Window *window)
{
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_title_font = fonts_load_custom_font(
                          resource_get_handle(RESOURCE_ID_TRADE_GOTHIC_LT_STD_BOLD_CONDENSED_42));
  s_body_font = fonts_load_custom_font(
                          resource_get_handle(RESOURCE_ID_TRADE_GOTHIC_LT_STD_18));

  // Create the striped background Layer
  s_background_layer = layer_create(bounds);
  layer_set_update_proc(s_background_layer, background_update_proc);

  // Create the time TextLayer
  s_time_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, s_title_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Create the date TextLayer
  s_date_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(110, 104), bounds.size.w, 30));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer, s_body_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  // Add layers to the Window
  layer_add_child(window_layer, s_background_layer);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
}

static void main_window_unload(Window *window)
{
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
