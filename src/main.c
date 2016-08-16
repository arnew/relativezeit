#include <pebble.h>
#include "main.h"
#define DEBUG 1
static Window *s_main_window;

static char s_buffer[12];
static TextLayer *s_timedetail_layer;
static TextLayer *s_time_layer;
static TextLayer *s_timerough_layer;
#if DEBUG
static TextLayer *s_weather_layer;
#endif
static GFont s_weather_font;

char* empty = "";
static uint32_t precision;

static void verticalAlignTextLayer(TextLayer *layer) {
    GRect frame = layer_get_frame(text_layer_get_layer(layer));
    GSize content = text_layer_get_content_size(layer);
    layer_set_frame(text_layer_get_layer(layer),
           GRect(frame.origin.x, frame.origin.y + (frame.size.h - content.h) / 2, 
           frame.size.w, content.h));
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  double adjustor =0x80000000*60.0/(1.0+precision);
  //temp = ((temp + adjustor/2)/adjustor)*adjustor;

  // Write the current hours and minutes into a buffer
  char*format;
  TextLayer *layer = s_time_layer;
if(adjustor < SECONDS_PER_MINUTE) { 
  layer = s_timedetail_layer;
  format = clock_is_24h_style() ? "%H:%M:%S" : "%I:%M:%S";
} else if (adjustor < SECONDS_PER_MINUTE*5 ) {
  format = clock_is_24h_style() ? "%H:%M" : "%I:%M";
} else if (adjustor < SECONDS_PER_MINUTE*15 ) {
  temp = ((temp + 30*5) / (60*5))*60*5;
  format = clock_is_24h_style() ? "%H::%M" : "%I::%M";
} else if (adjustor < SECONDS_PER_HOUR) {
  temp = ((temp + 30*15) / (60*15))*60*15;
  format = clock_is_24h_style() ? "%H %M" : "%I %M";
} else if (adjustor < SECONDS_PER_DAY/2 ) {
  temp = ((temp + 30*60) / (60*60))*60*60;
  format = clock_is_24h_style() ? "%H" : "%I";
} else if (adjustor < SECONDS_PER_DAY) {  
  layer = s_timerough_layer;
  format = "%A";
} else {  
layer = s_timerough_layer;
  format = "%B";
}
  
  struct tm *tick_time = localtime(&temp);
  strftime(s_buffer, sizeof(s_buffer),  format, tick_time);
   text_layer_set_text(s_timedetail_layer, NULL);
  text_layer_set_text(s_time_layer, NULL);
  text_layer_set_text(s_timerough_layer, NULL);

  // Display this time on the TextLayer
  text_layer_set_text(layer, s_buffer);
  //verticalAlignTextLayer(layer);

  #if DEBUG
   static char s_buffer2[20];
  snprintf(s_buffer2, sizeof(s_buffer2), "%u\n%u", (unsigned int) adjustor, (unsigned int) precision);  
  text_layer_set_text(s_weather_layer, s_buffer2);
  #endif
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  uint32_t old_precision = precision;
  precision /= 1.005;
  #if DEBUG >1
 precision /= 1+(DEBUG-1/20.0);
  #endif
  if(precision > old_precision ) 
    precision = 1;
  update_time();
}
static void tap_handler(AccelAxisType axis, int32_t direction )
{
  uint32_t old_precision = precision;
  precision++;
  precision*=4;
  if(precision < old_precision) 
    precision = UINT32_MAX;
  update_time();
}
static void focus_handler(bool in_focus){  
  uint32_t old_precision = precision; 
  precision++;
  precision*=1.5;
  
  if(precision < old_precision) 
    precision = UINT32_MAX;
  #if DEBUG > 10
    precision = UINT32_MAX;
  #endif
   update_time();
}

static void main_window_load(Window *window) {
 // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create the TextLayer with specific bounds
  s_time_layer = text_layer_create(      GRect(0, PBL_IF_ROUND_ELSE(58, 52)+5, bounds.size.w, 43));
  s_timedetail_layer = text_layer_create(      GRect(0, PBL_IF_ROUND_ELSE(58, 52)+8, bounds.size.w,  39));
  s_timerough_layer = text_layer_create(      GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w,  43+10));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  
    text_layer_set_background_color(s_timedetail_layer, GColorClear);
  text_layer_set_text_color(s_timedetail_layer, GColorBlack);
  text_layer_set_text(s_timedetail_layer, "00:00:00");
  text_layer_set_text_alignment(s_timedetail_layer, GTextAlignmentCenter);
  text_layer_set_font(s_timedetail_layer, fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS));
  
    text_layer_set_background_color(s_timerough_layer, GColorClear);
  text_layer_set_text_color(s_timerough_layer, GColorBlack);
  text_layer_set_text(s_timerough_layer, "Word");
  text_layer_set_text_alignment(s_timerough_layer, GTextAlignmentCenter);
      text_layer_set_font(s_timerough_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));

  
  #if DEBUG
 s_weather_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(125, 120), bounds.size.w, 40));
// Style the text
text_layer_set_background_color(s_weather_layer, GColorClear);
text_layer_set_text_color(s_weather_layer, GColorBlack);
text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
text_layer_set_text(s_weather_layer, "Loading...");
  // Create second custom font, apply it and add to Window
text_layer_set_font(s_weather_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_layer));
  #endif
  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_timedetail_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_timerough_layer));
}

static void main_window_unload(Window *window) {
 // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_timedetail_layer);
  text_layer_destroy(s_timerough_layer);
  // Destroy weather elements
  #if DEBUG
text_layer_destroy(s_weather_layer);
  #endif
fonts_unload_custom_font(s_weather_font);
}

static void init() {
  precision = persist_read_int(0);
  if(precision == 0) {
  precision = 0x01000000;
  } else {
    #if DEBUG > 10
 precision *= 4;
    #endif
  }
  // Register with TickTimerService
tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  accel_tap_service_subscribe(tap_handler);
  app_focus_service_subscribe(focus_handler);
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  // Make sure the time is displayed from the start
update_time();

}

static void deinit() {
 // Destroy Window
  window_destroy(s_main_window);
  persist_write_int(0,precision);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}