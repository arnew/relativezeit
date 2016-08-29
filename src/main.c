#include <pebble.h>
#include "main.h"
#include <glancing/glancing.h>

// todo list:
// only redraw on changes, done
// enable logging only on debug, done
// disable accellerometer -> doesnt seem to be a problem
#define DEBUG 1

static GlanceState glancing = 0;
static unsigned int last_selection;
static Window *s_main_window;

static char s_buffer[12];
static TextLayer *s_timedetail_layer;
static TextLayer *s_time_layer;
static TextLayer *s_timerough_layer;

char *empty = "";
typedef struct
{
  uint64_t seconds;
  uint64_t minutes;
  uint32_t hours;
  uint32_t days;
  uint32_t second_count, minute_count, hour_count;
} precision_t;
static precision_t precision;
typedef struct
{
  float a, b, c, d, e;
} weights_t;
static weights_t weights;

static uint64_t top64 = 0x8000000000000000ull;

void
shift_precision_seconds (precision_t * precision)
{
  precision->minutes |= (precision->seconds & 1) ? top64 : 0;
  precision->seconds >>= 1;
  precision->second_count++;
}

void
shift_precision_minutes (precision_t * precision)
{
  precision->hours |= (precision->minutes & 1) ? 0x80000000 : 0;
  precision->minutes >>= 1;
  precision->minute_count++;
  precision->second_count = 0;
}

void
shift_precision_hours (precision_t * precision)
{
  precision->days |= (precision->hours & 1) ? 0x80000000 : 0;
  precision->hours >>= 1;
  precision->hour_count++;
  precision->minute_count = 0;
}

void
shift_precision_days (precision_t * precision)
{
  precision->days >>= 1;
  precision->hour_count = 0;
}

int
CountOnesFromInteger (uint64_t value)
{
  int count = 0;
  while (value != 0)
    {
      count++;
      value &= value - 1;
    }
  return count;
}

static double slow_adj = 7;

inline float
min (float a, float b)
{
  if (a < b)
    return a;
  return b;
}

inline float
max (float a, float b)
{
  if (a < b)
    return b;
  return a;
}

void
enforce_weights (precision_t * precision, weights_t * weights, float v,
		 float w, float b)
{
  int i = CountOnesFromInteger (precision->seconds >> 60),
    j = CountOnesFromInteger (precision->seconds << 4),
    k = CountOnesFromInteger (precision->minutes),
    l = CountOnesFromInteger (precision->hours),
    m = CountOnesFromInteger (precision->days);
  // if (i)
  weights->a = (weights->a * max (0, (v + w * i / 4))) + b;
//  if (j)
  weights->b = (weights->b * max (0, (v + w * j / 56))) + b;
//  if (k && CountOnesFromInteger (precision->minutes>>30))
  weights->c = (weights->c * max (0, (v + w * k / 60))) + b;
//  if (l && CountOnesFromInteger (precision->hours>>12))
  weights->d = (weights->d * max (0, (v + w * l / 24))) + b;
  //if (m && CountOnesFromInteger (precision->days>>15))
  weights->e = (weights->e * max (0, (v + w * m / 31))) + b;
#if DEBUG
  APP_LOG (APP_LOG_LEVEL_DEBUG, "weights: %i %i %i %i %i",
	   (int) (10000 * weights->a), (int) (10000 * weights->b),
	   (int) (10000 * weights->c), (int) (10000 * weights->d),
	   (int) (10000 * weights->e));
#endif
}


void
touch_precision (precision_t * precision)
{
  last_selection = -1;
  static int state = 0;
  if (precision->seconds & 0xF000000000000000 && glancing != GLANCING_ACTIVE)
    {
#if DEBUG
      APP_LOG (APP_LOG_LEVEL_DEBUG, "state: %i", state);
#endif
      if (state == 1)
	{
#if DEBUG
	  APP_LOG (APP_LOG_LEVEL_DEBUG, "positive enforce weights");
#endif
	  enforce_weights (precision, &weights, 1, 2, 1);
	}
      else if (state >= 8)
	{
	  // memset(precision, 0x55, sizeof(precision_t));

	  memset (&weights, 0, sizeof (weights_t));
//          weights.a = weights.b = weights.c = weights.d = weights.e = 1;
	}
      state++;
    }
  else if (!(state & top64))
    state = 0;
  precision->seconds |= top64;
}


void
update_precision (precision_t * precision)
{
  enforce_weights (precision, &weights, 0.99999, -0.00005, 0);

  shift_precision_seconds (precision);

  if ((precision->second_count % 60) == 0)
    {
      shift_precision_minutes (precision);
    }
  if ((precision->minute_count % 60) == 0)
    {
      shift_precision_hours (precision);
    }
  if ((precision->hour_count % 24) == 0)
    shift_precision_days (precision);
#if DEBUG

  APP_LOG (APP_LOG_LEVEL_DEBUG, "precision: %i:%u %i:%u %i:%u %i",
	   CountOnesFromInteger (precision->seconds),
	   (unsigned int) precision->second_count,
	   CountOnesFromInteger (precision->minutes),
	   (unsigned int) precision->minute_count,
	   CountOnesFromInteger (precision->hours),
	   (unsigned int) precision->hour_count,
	   CountOnesFromInteger (precision->days));
#endif
}


static void
update_time ()
{

  // Get a tm structure
  time_t temp = time (NULL);
  double adjustor =
    (CountOnesFromInteger (precision.seconds >> 60) / 4.0 * (1 + weights.a)) +
    (CountOnesFromInteger (precision.seconds << 4) / 56.0 * (1 + weights.b)) +
    (CountOnesFromInteger (precision.minutes) / 60.0 * (1 + weights.c)) +
    (CountOnesFromInteger (precision.hours) / 24.0 * (1 + weights.d)) +
    (CountOnesFromInteger (precision.days) / 31.0 * (1 + weights.e));
  //temp = ((temp + adjustor/2)/adjustor)*adjustor;

  // Write the current hours and minutes into a buffer
  char *format = NULL;
  TextLayer *layer = s_time_layer;
  slow_adj = (slow_adj * 0.99 + adjustor * 0.01);
  unsigned int selection = (unsigned int) max (slow_adj, adjustor);
#if DEBUG
  APP_LOG (APP_LOG_LEVEL_DEBUG, "relativity: %u %u %u",
	   (unsigned int) adjustor, (unsigned int) slow_adj, last_selection);
#endif
  if (selection < 8 && selection == last_selection)
    return;

  switch (selection)
    {
    default:
    case 8:
      layer = s_timedetail_layer;
      format = clock_is_24h_style ()? "%H:%M:%S" : "%I:%M:%S";
      break;
    case 7:
      format = clock_is_24h_style ()? "%H:%M" : "%I:%M";

      break;
    case 6:
      temp = ((temp + 30 * 5) / (60 * 5)) * 60 * 5;
      format = clock_is_24h_style ()? "%H::%M" : "%I::%M";

      break;
    case 5:
      temp = ((temp + 30 * 15) / (60 * 15)) * 60 * 15;
      format = clock_is_24h_style ()? "%H %M" : "%I %M";

      break;
    case 4:
      temp = ((temp + 30 * 60) / (60 * 60)) * 60 * 60;
      format = clock_is_24h_style ()? "%H" : "%I";

      break;
    case 3:
      {
	layer = s_timerough_layer;
	format = NULL;
	//temp = temp + 3*60*60;
	struct tm *tick_time = localtime (&temp);
	char *tageszeiten[] = { "Nacht", "Morgen", "Mittag", "Abend" };
	strncpy (s_buffer, tageszeiten[tick_time->tm_hour / 6],
		 sizeof (s_buffer));
      }
      break;
    case 2:
      layer = s_timerough_layer;
      format = "%A";		// Wochentag

      break;
    case 1:
      layer = s_timerough_layer;
      format = "%B";		// Monat

      break;

    case 0:
      {
	format = NULL;		// Monat
	struct tm *tick_time = localtime (&temp);
	snprintf (s_buffer, sizeof (s_buffer), "%i",
		  tick_time->tm_year + 1900);
      }
      break;
    }

  if (format)
    {
      struct tm *tick_time = localtime (&temp);
      strftime (s_buffer, sizeof (s_buffer), format, tick_time);
    }
  text_layer_set_text (s_timedetail_layer, NULL);
  text_layer_set_text (s_time_layer, NULL);
  text_layer_set_text (s_timerough_layer, NULL);

  // Display this time on the TextLayer
  text_layer_set_text (layer, s_buffer);
  last_selection = selection;

}


void
glancing_callback (GlancingData * data)
{
  glancing = data->state;
  switch (data->state)
    {
    case GLANCING_ACTIVE:
      APP_LOG (APP_LOG_LEVEL_DEBUG, "glancing active");
      break;
    case GLANCING_TIMEDOUT:
      APP_LOG (APP_LOG_LEVEL_DEBUG, "glancing timed out");
      break;
    case GLANCING_INACTIVE:
    default:
      APP_LOG (APP_LOG_LEVEL_DEBUG, "glancing inactive");
      break;
    }
}

static void
second_tick (struct tm *tick_time, TimeUnits units_changed)
{
  if (units_changed & MINUTE_UNIT)
    last_selection = -1;
  update_precision (&precision);
  if (glancing & GLANCING_ACTIVE)
    touch_precision (&precision);
  update_time ();
}

static void
tap_handler (AccelAxisType axis, int32_t direction)
{
#if DEBUG
  APP_LOG (APP_LOG_LEVEL_DEBUG, "tap");
#endif
  touch_precision (&precision);
  update_time ();
}

static void
focus_handler (bool in_focus)
{
#if DEBUG
  APP_LOG (APP_LOG_LEVEL_DEBUG, "focus");
#endif
  touch_precision (&precision);
  update_time ();
}

static void
main_window_load (Window * window)
{
  // Get information about the Window
  Layer *window_layer = window_get_root_layer (window);
  GRect bounds = layer_get_bounds (window_layer);

  // Create the TextLayer with specific bounds
  s_time_layer =
    text_layer_create (GRect
		       (0, PBL_IF_ROUND_ELSE (58, 52) + 5, bounds.size.w,
			43));
  s_timedetail_layer =
    text_layer_create (GRect
		       (0, PBL_IF_ROUND_ELSE (58, 52) + 8, bounds.size.w,
			39));
  s_timerough_layer =
    text_layer_create (GRect
		       (0, PBL_IF_ROUND_ELSE (58, 52), bounds.size.w,
			43 + 10));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color (s_time_layer, GColorClear);
  text_layer_set_text_color (s_time_layer, GColorBlack);
  text_layer_set_text (s_time_layer, "00:00");
  text_layer_set_font (s_time_layer,
		       fonts_get_system_font (FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment (s_time_layer, GTextAlignmentCenter);


  text_layer_set_background_color (s_timedetail_layer, GColorClear);
  text_layer_set_text_color (s_timedetail_layer, GColorBlack);
  text_layer_set_text (s_timedetail_layer, "00:00:00");
  text_layer_set_text_alignment (s_timedetail_layer, GTextAlignmentCenter);
  text_layer_set_font (s_timedetail_layer,
		       fonts_get_system_font (FONT_KEY_LECO_38_BOLD_NUMBERS));

  text_layer_set_background_color (s_timerough_layer, GColorClear);
  text_layer_set_text_color (s_timerough_layer, GColorBlack);
  text_layer_set_text (s_timerough_layer, "Word");
  text_layer_set_text_alignment (s_timerough_layer, GTextAlignmentCenter);
  text_layer_set_font (s_timerough_layer,
		       fonts_get_system_font (FONT_KEY_BITHAM_42_LIGHT));

  // Add it as a child layer to the Window's root layer
  layer_add_child (window_layer, text_layer_get_layer (s_time_layer));
  layer_add_child (window_layer, text_layer_get_layer (s_timedetail_layer));
  layer_add_child (window_layer, text_layer_get_layer (s_timerough_layer));

}

static void
main_window_unload (Window * window)
{
  // Destroy TextLayer
  text_layer_destroy (s_time_layer);
  text_layer_destroy (s_timedetail_layer);
  text_layer_destroy (s_timerough_layer);
  // Destroy weather elements

}

static void
init ()
{
#if DEBUG
  APP_LOG (APP_LOG_LEVEL_DEBUG, "init");
#endif
  // read state
  if (persist_exists (0))
    persist_read_data (0, &precision, sizeof (precision_t));
  else
    memset (&precision, 0x55, sizeof (precision_t));

  if (persist_exists (1))
    persist_read_data (1, &weights, sizeof (weights_t));
  else
    memset (&weights, 0, sizeof (weights_t));
  touch_precision (&precision);

  // Register with TickTimerService
  tick_timer_service_subscribe (SECOND_UNIT, second_tick);

  glancing_service_subscribe (5 * 1000, true, true, glancing_callback);
  accel_tap_service_subscribe (tap_handler);
  app_focus_service_subscribe (focus_handler);
  // Create main Window element and assign to pointer
  s_main_window = window_create ();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers (s_main_window, (WindowHandlers)
			      {
			      .load = main_window_load,.unload =
			      main_window_unload}
  );

  // Show the Window on the watch, with animated=true
  window_stack_push (s_main_window, true);
  // Make sure the time is displayed from the start
  update_time ();

}

static void
deinit ()
{
  // Destroy Window
  window_destroy (s_main_window);
  // save state
  persist_write_data (0, &precision, sizeof (precision_t));
  persist_write_data (1, &weights, sizeof (weights_t));

}

int
main (void)
{
  init ();
  app_event_loop ();
  deinit ();
}
