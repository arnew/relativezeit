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
typedef struct {
  uint64_t seconds;
  uint64_t minutes;
  uint32_t hours;
  uint32_t days;
  uint32_t second_count, minute_count, hour_count;
} precision_t;
static precision_t precision;

void touch_precision(precision_t *precision) {
  precision->seconds |= 0x8000000000000000ull;  
    APP_LOG(APP_LOG_LEVEL_DEBUG, "precision %llx %llx %x %x", (unsigned long long)precision->seconds,(unsigned long long)precision->minutes, (unsigned int)precision->hours, (unsigned int)precision->days);

}


void shift_precision_seconds(precision_t *precision) {
  precision->minutes |= (precision->seconds & 1)? 0x8000000000000000 : 0;
  precision->seconds >>=1;
  precision->second_count++;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "precision shift seconds %llx %llx %x %x", (unsigned long long)precision->seconds,(unsigned long long)precision->minutes, (unsigned int)precision->hours, (unsigned int)precision->days);
}
void shift_precision_minutes(precision_t *precision) {
  precision->hours |= (precision->minutes & 1)? 0x80000000 : 0;
  precision->minutes >>=1; 
    precision->minute_count++;
  precision->second_count = 0;

  APP_LOG(APP_LOG_LEVEL_DEBUG, "precision shift minuts %llx %llx %x %x", (unsigned long long)precision->seconds,(unsigned long long)precision->minutes, (unsigned int)precision->hours, (unsigned int)precision->days);

}
void shift_precision_hours(precision_t *precision) {
  precision->days |= (precision->hours & 1)? 0x80000000 : 0;
  precision->hours >>=1;  
      precision->hour_count++;
precision->minute_count =0;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "precision shift hours %llx %llx %x %x", (unsigned long long)precision->seconds,(unsigned long long)precision->minutes, (unsigned int)precision->hours, (unsigned int)precision->days);

}
void shift_precision_days(precision_t *precision) {
  precision->days >>=1;  
  precision->hour_count = 0;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "precision shift days %llx %llx %x %x", (unsigned long long)precision->seconds,(unsigned long long)precision->minutes, (unsigned int)precision->hours, (unsigned int)precision->days);

}


void update_precision(precision_t *precision) {
  shift_precision_seconds(precision);
  if(precision->second_count == 60) shift_precision_minutes(precision);
  if(precision->minute_count == 60) shift_precision_hours(precision);
  if(precision->hour_count == 24) shift_precision_days(precision);
}

int CountOnesFromInteger(uint64_t value) {
    int count =0;
  while(value != 0) {
    count++;
    value &= value-1;
  }
    return count;
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  double adjustor =   ((60.0+60+24+31+80) -
                       CountOnesFromInteger(precision.seconds>>60)*20 -
    CountOnesFromInteger(precision.seconds) - 
    CountOnesFromInteger(precision.minutes) - 
    CountOnesFromInteger(precision.hours) -
    CountOnesFromInteger(precision.days) 
    )/(2*(60+60+24+31) )
    * SECONDS_PER_DAY
                       ;
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
  snprintf(s_buffer2, sizeof(s_buffer2), "%i %i\n%i %i\n%u", 
           CountOnesFromInteger(precision.seconds), 
           CountOnesFromInteger(precision.minutes), 
           CountOnesFromInteger(precision.hours), 
           CountOnesFromInteger(precision.days),
           (unsigned int) adjustor);
  text_layer_set_text(s_weather_layer, s_buffer2);
  #endif
}

static void second_tick(struct tm *tick_time, TimeUnits units_changed) {
  update_precision(&precision);
  update_time();
}


static void tap_handler(AccelAxisType axis, int32_t direction )
{
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tap");
  touch_precision(&precision);
  
  update_time();
}
static void focus_handler(bool in_focus){  
    APP_LOG(APP_LOG_LEVEL_DEBUG, "focus");
  
  touch_precision(&precision);
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
 s_weather_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(105, 100), bounds.size.w, 60));
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
    APP_LOG(APP_LOG_LEVEL_DEBUG, "init");

  persist_read_data(0,&precision, sizeof(precision_t));
  touch_precision(&precision);
  
  // Register with TickTimerService
tick_timer_service_subscribe(SECOND_UNIT, second_tick);
  
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
  persist_write_data(0,&precision, sizeof(precision_t));

}

int main(void) {
  init();
  app_event_loop();
  deinit();
}