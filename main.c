#include <assert.h>
#include <errno.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// NOTE: Choice of UI: terminal or GUI based
#define USER_INTERFACE_GUI
// #define USER_INTERFACE_TUI
// TODO: Moved to config file, implement here

#ifdef USER_INTERFACE_GUI

#include <GL/glew.h>
#include <SDL2/SDL.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GL3_IMPLEMENTATION
#define NK_KEYSTATE_BASED_INPUT

#include "nuklear.h"
#include "nuklear_sdl_gl3.h"

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#endif

#include "cJSON.h"

struct Resolution {
  int32_t width;
  int32_t height;
};

/// Game configuration initialized once at startup
static struct {
  char *FILEPATH_ROOT;
  char *FILEPATH_RSRC;
  bool GUI;
  bool HARD_MODE;
  bool FULLSCREEN;
  int LANGUAGE;
  struct Resolution RESOLUTION;
} CONFIG;

// NOTE: Used for development
#define DEBUG

#define C_KEY_DOWN 258
#define C_KEY_UP 259
#define C_KEY_LEFT 260
#define C_KEY_RIGHT 261
#define C_KEY_ENTER 10
#define C_KEY_ESCAPE 27

// Language definitions
#define WINTER "winter"
#define SPRING "spring"
#define SUMMER "summer"
#define AUTUMN "autumn"

/***** string utility functions *****/
// Returns callee owned ptr to the concatenation of lhs & rhs, NULL on failure
char *str_concat_new(const char *lhs, const char *rhs) {
  if (lhs == NULL || rhs == NULL) {
    return NULL;
  }
  const size_t lhs_lng = strlen(lhs);
  const size_t rhs_lng = strlen(rhs);
  const size_t new_lng = lhs_lng + rhs_lng;
  char *new_str = (char *)calloc(new_lng, sizeof(char));
  strncpy(new_str, lhs, lhs_lng);
  strncpy(&new_str[lhs_lng], rhs, rhs_lng);
  return new_str;
}

/***** file utility functions *****/
// Returns callee owned ptr to file contents, NULL on failure
const char *open_file(const char *filepath) {
  if (filepath == NULL) {
    return NULL;
  }
  FILE *f = fopen(filepath, "r");
  if (f == NULL) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  const size_t size = ftell(f);
  const char *file_contents = (char *)calloc(size, sizeof(char));
  rewind(f);
  fread((void *)file_contents, 1, size, f);
  fclose(f);
  return file_contents;
}

/***** ncurses utility functions *****/
/// Window, MoVe, CLearR, PRINT, Word
static inline void wmvclrprintw(WINDOW *win, int y, int x, const char *fmt,
                                ...) {
  wmove(win, y, x);
  wclrtoeol(win);
  va_list args;
  va_start(args, fmt);
  vw_printw(win, fmt, args);
  va_end(args);
}

/// Window, MoVe, CLearR, PRINT
static inline void wmvclrprint(WINDOW *win, int y, int x, const char *str) {
  wmove(win, y, x);
  wclrtoeol(win);
  wprintw(win, str);
}

/// Window, MoVe, PRINT
static inline void wmvprint(WINDOW *win, int y, int x, const char *str) {
  wmove(win, y, x);
  wprintw(win, str);
}

/// Window, MoVe, PRINT, Word
static inline void wmvprintw(WINDOW *win, int y, int x, const char *fmt, ...) {
  wmove(win, y, x);
  va_list args;
  va_start(args, fmt);
  vw_printw(win, fmt, args);
  va_end(args);
}

// Allocs a new window and sets a box around it plus displays it
WINDOW *create_newwin(const int height, const int width, const int starty,
                      const int startx) {
  WINDOW *local_win = newwin(height, width, starty, startx);
  box(local_win, 0, 0);
  wrefresh(local_win);
  return local_win;
}

void destroy_win(WINDOW *win) {
  wborder(win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
  wclear(win);
  wrefresh(win);
  delwin(win);
}

// TODO: Refactor into math.h?
static inline float maxf(const float a, const float b) { return a < b ? b : a; }

static inline float maxi(const uint32_t a, const uint32_t b) {
  return a < b ? b : a;
}

static inline size_t uni_randu(const size_t to) { return rand() % to; }

static WINDOW *root = NULL; // FIXME: ...

// NOTE: Julian calendar introduced Jan. 1st of 45 BC
struct Date {
  uint32_t day;
  uint32_t month;
  uint32_t year;
};

// NOTE: Not historically correct in classic Latin times (Kal, a.d VI Non, etc)
// Current date in this timestep
static struct Date date;

// NOTE: Modern Roman numerals (I, V, X, L, C, D, M), (1, 5, 10, 50, 100, 500,
// 1000) NOTE: Using subtractive notation API: Callee-responsible for freeing
// the returned pointer
static inline const char *roman_numeral_str(const uint32_t n) {
  const div_t M = div(n, 1000);
  const div_t C = div(M.rem, 100);
  const div_t X = div(C.rem, 10);
  const div_t I = div(X.rem, 1);

  char Ms[M.quot];
  for (int32_t i = 0; i < M.quot; i++) {
    Ms[i] = (char)'M';
  }

  char *Cs = NULL;
  size_t Cs_size = 0;
  switch (C.quot) {
  case 1:
    Cs = "C";
    Cs_size = 1;
    break;
  case 2:
    Cs = "CC";
    Cs_size = 2;
    break;
  case 3:
    Cs = "CCC";
    Cs_size = 3;
    break;
  case 4:
    Cs = "CD";
    Cs_size = 2;
    break;
  case 5:
    Cs = "D";
    Cs_size = 1;
    break;
  case 6:
    Cs = "DC";
    Cs_size = 2;
    break;
  case 7:
    Cs = "DCC";
    Cs_size = 3;
    break;
  case 8:
    Cs = "DCCC";
    Cs_size = 4;
    break;
  case 9:
    Cs = "CM";
    Cs_size = 2;
    break;
  }

  char *Xs = NULL;
  size_t Xs_size = 0;
  switch (X.quot) {
  case 1:
    Xs = "X";
    Xs_size = 1;
    break;
  case 2:
    Xs = "XX";
    Xs_size = 2;
    break;
  case 3:
    Xs = "XXX";
    Xs_size = 3;
    break;
  case 4:
    Xs = "XL";
    Xs_size = 2;
    break;
  case 5:
    Xs = "L";
    Xs_size = 1;
    break;
  case 6:
    Xs = "LX";
    Xs_size = 2;
    break;
  case 7:
    Xs = "LXX";
    Xs_size = 3;
    break;
  case 8:
    Xs = "LXXX";
    Xs_size = 4;
    break;
  case 9:
    Xs = "XC";
    Xs_size = 2;
    break;
  }

  char *Is = NULL;
  size_t Is_size = 0;
  switch (I.quot) {
  case 1:
    Is = "I";
    Is_size = 1;
    break;
  case 2:
    Is = "II";
    Is_size = 2;
    break;
  case 3:
    Is = "III";
    Is_size = 3;
    break;
  case 4:
    Is = "IV";
    Is_size = 2;
    break;
  case 5:
    Is = "V";
    Is_size = 2;
    break;
  case 6:
    Is = "VI";
    Is_size = 2;
    break;
  case 7:
    Is = "VII";
    Is_size = 3;
    break;
  case 8:
    Is = "VIII";
    Is_size = 4;
    break;
  case 9:
    Is = "IX";
    Is_size = 2;
    break;
  }

  size_t str_len = M.quot + Cs_size + Xs_size + Is_size;
  char *str = calloc(str_len + 1, 1);

  size_t p = 0;
  memcpy(&str[p], Ms, M.quot);
  p += M.quot;

  memcpy(&str[p], Cs, Cs_size);
  p += Cs_size;

  memcpy(&str[p], Xs, Xs_size);
  p += Xs_size;

  memcpy(&str[p], Is, Is_size);
  p += Is_size;

  p++;
  str[p] = '\0';

  return str;
}

static inline const char *get_month_str(const struct Date date) {
  assert(date.month <= 11);
  static const char *month_strs[] = {"Ianuarius", "Februarius", "Martius",
                                     "Aprilis",   "Maius",      "Iunius",
                                     "Iulius",    "Augustus",   "September",
                                     "October",   "November",   "December"};
  return month_strs[date.month];
}

static inline uint32_t get_days_in_month(const struct Date date) {
  assert(date.month <= 11);
  static const uint32_t month_lngs[] = {31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};
  return month_lngs[date.month];
}

static inline void increment_date(struct Date *date) {
  if (date->day + 1 > get_days_in_month(*date) - 1) {
    date->month++;
    date->day = 0;
  } else {
    date->day += 1;
  }

  if (date->month == 12) {
    date->year++;
    date->month = 0;
  }
}

static uint64_t timestep = 0;
static uint32_t simulation_speed = 1;

static inline uint32_t ms_per_timestep_for(const uint32_t speed) {
  if (speed == 0) {
    return 0;
  }
#ifdef DEBUG
  return (1000.0f / (2.0f * (float)speed));
#else
  return (1000.0f / (float)speed);
#endif
}

// NOTE: Callee resonsible for freeing string returned
const char *get_year_str(const struct Date *date) {
  // TODO: Consuls Date generation
  return "Year of Cornelius Lentulus CON II & M. Porcius Cato CON I";
  /*
    static const char* consuls_str[] = { "P. Sulpicius Galba Maximus", "C.
    Aurelius Cotta", "L. Cornelius Lentulus", "P.Villius Tappulus", "T.
    Quinctius Flamininus", "Sex. Aelius Paetus Catus", "C. Cornelius Cethegus",
    "Q. Minucius Rufus", "L. Furius Purpureo", "M. Claudius Marcellus", "L.
    Valerius Flaccus", "M. Porcius Cato"
    };
    static const uint32_t NUM_CONSULS = sizeof(consuls_str) / sizeof(char*);
    static uint8_t consuls_cnt[sizeof(consuls_str) / sizeof(char*)] = {0};
    size_t c0 = uni_randu(NUM_CONSULS);
    size_t c1 = uni_randu(NUM_CONSULS);
    consuls_cnt[c0]++;
    consuls_cnt[c1]++;
    char* str = (char*) calloc(128, 1);
    const char* s0 = consuls_str[c0];
    const char* s1 = consuls_str[c1];
    const char* n0 = roman_numeral_str(consuls_cnt[c0]);
    const char* n1 = roman_numeral_str(consuls_cnt[c1]);
    sprintf(str, "Year of %s CON %s & %s CON %s ", s0, n0, s1, n1);
    free((void*)n0); free((void*)n1);
    return str;
  */
}

const char *get_season_str(const struct Date *date) {
  switch (date->month) {
  case 0:
    return WINTER;
  case 1:
    return WINTER;
  case 2:
    return WINTER;
  case 3:
    return SPRING;
  case 4:
    return SPRING;
  case 5:
    return SUMMER;
  case 6:
    return SUMMER;
  case 7:
    return SUMMER;
  case 8:
    return SUMMER;
  case 9:
    return AUTUMN;
  case 10:
    return AUTUMN;
  case 11:
    return WINTER;
  case 12:
    return WINTER;
  }
  assert(false);
  return "";
}

static inline bool is_winter(const struct Date *date) {
  return strncmp(WINTER, get_season_str(date), strlen(WINTER)) == 0;
}

enum CapacityType { Political = 0, Military = 1, Civic = 1 };

const char *lut_capacity_str(const enum CapacityType type) {
  static const char *strs[3] = {"Political", "Military", "Civic"};
  return strs[type];
}

enum FarmProduceType { Grapes = 0, Wheat = 1, NUMBER_OF_PRODUCE };

const char *lut_farm_produce_str(const enum FarmProduceType type) {
  static const char *strs[2] = {"Grapes", "Wheat"};
  return strs[type];
}

// Ring buffer with strings basically
#define EVENTLOG_CAPACITY 10
struct EventLog {
  char **lines;
  int32_t curr_line; // Curr line for pushing msgs (a.k.a p(ush)) -1 == empty
  int32_t read_line; // Curr line for reading msgs (a.k.a r(ead)) -1 == empty
  const uint32_t capacity;
};

struct EventLog eventlog_new() {
  struct EventLog log = {.capacity = EVENTLOG_CAPACITY};
  log.lines = (char **)calloc(log.capacity, sizeof(char *));
  log.curr_line = -1;
  log.read_line = -1;
  return log;
}

// FIXME: EventLog ring buffer printout does not print in the correct order
// Should print [oldest msg ... least oldest msg] but prints [0 ... X], [X + 1,
// 2X], etc ...
void eventlog_rewind(struct EventLog *log) { log->read_line = -1; }

// Adds msg to the eventlog by copying over the string
void eventlog_add_msg(struct EventLog *log, const char *msg) {
  assert(log);
  assert(msg);

  log->curr_line = (log->curr_line + 1) % log->capacity;

  if (log->lines[log->curr_line] != NULL) {
    free(log->lines[log->curr_line]);
    log->lines[log->curr_line] = NULL;
  }

  if (log->curr_line == log->read_line) {
    log->read_line = (log->read_line + 1) % log->capacity;
  }

  const size_t len = strlen(msg) + 1;
  log->lines[log->curr_line] = (char *)calloc(len, sizeof(char));
  strcpy(log->lines[log->curr_line], msg);
}

void eventlog_clear(struct EventLog *log) {
  assert(log);
  for (size_t i = 0; i < log->capacity; i++) {
    if (log->lines[i]) {
      free(log->lines[i]);
      log->lines[i] = NULL;
    }
  }
  log->curr_line = -1;
  log->read_line = -1;
}

bool eventlog_next_msg(struct EventLog *log, char **msg) {
  assert(log);
  assert(log->lines);
  assert(msg);

  // empty ring
  if (log->curr_line == -1) {
    return false;
  }

  const uint32_t nxt_msg_line = (log->read_line + 1) % log->capacity;

  // did reach end of ring
  if (log->read_line == log->curr_line) {
    eventlog_rewind(log);
    // ring not full must return to start at -1 (0) instead of at p + 1
    if (log->lines[log->read_line] == NULL) {
      log->read_line = -1;
    }
    return false;
  }

  *msg = log->lines[nxt_msg_line];
  log->read_line = nxt_msg_line;
  return true;
}

struct FarmArgument {
  float tax_percentage; // [0, 1] in land tax
  size_t area;          // Land area used (jugerum, cirka 0.6 hectare)
  enum FarmProduceType produce;
};

struct Policy {
  enum CapacityType type;
  uint8_t cost;
  const char *name_str;
  const char *description_str;
  struct Effect *effect;
  size_t num_effects;
};

struct Construction {
  bool construction_finished;
  bool maintained;
  const char *name_str;
  const char *description_str;
  float construction_cost; // Cost in gold per timestep construction period
  float cost;
  float maintenance;
  struct Effect *effect;
  size_t num_effects;       // Construction variants
  size_t construction_time; // Time to build in timesteps (days)
  struct Date construction_started;
  struct Date construction_completed;
};

struct City {
  char *name;
  // Farming
  float *produce_values; // Value in terms of produce
  size_t land_area;      // Land area available (used by farms, mansio, castrum)
  size_t land_area_used; // Land used
  float food_production; // TODO: Food production / day in terms of calories?
  float food_usage;      // Higher usage than production means importation
  // Gold
  float gold;       // Pounds of gold (kg??) (negative, debt??)
  float gold_usage; // Income / Lose
  /// Capacity
  uint32_t political_capacity;
  uint32_t political_usage;
  uint32_t diplomatic_capacity;
  uint32_t diplomatic_usage;
  uint32_t military_capacity;
  uint32_t military_usage;
  /// Demographics
  float population;
  float birthrate;       // Births / 1'000 pops / timestep
  float births;          // Accumulated from previous timesteps
  float deathrate;       // Deaths / 1'000 pops / timestep
  float deaths;          // Accumulated from previous timesteps
  float immigrationrate; // Immigration in # pops / 1'000 pops / timestep
  float immigrations;    // Accumulated from previous timesteps
  float emmigrationrate; // Emmigration in # pops / 1'000 pops / timestep
  float emmigrations;    // Accumulated from previous timesteps
  /// Effects
  struct Effect *effects;
  size_t num_effects;
  size_t num_effects_capacity;
  // Construction projects available
  struct Construction *construction_projects;
  size_t num_construction_projects;
  size_t num_construction_projects_capacity;
  // Constructions raised and still standing
  struct Construction *constructions;
  size_t num_constructions;
  size_t num_constructions_capacity;
  // Policies
  bool policies_enabled;
  struct Policy *policies;
  size_t num_policies;
  size_t num_policies_capacity;
  struct EventLog *log;
  struct Popup *popups;
  size_t num_popups;
  size_t num_popups_capacity;
};

#define FOREVER -1
// NOTE: When either one of the human readable strings are NULL the effect is
// hidden in the UI.
struct Effect {
  char *name_str;        // Human readable name of the effect
  char *description_str; // Human readable description of the effect
  int64_t duration; // Negative for forever, 0 = done/inactive, timesteps left
  void *arg;        // Custom argument provided
  void (*tick_effect)(struct Effect *e, const struct City *c, struct City *c1);
};

struct Popup {
  char *title;
  char *description;
  uint32_t num_choices;
  char **choices;
  char **hover_txts; // Description text for the choices when hovering over them
  void (*callback)(const struct Popup *p, const struct City *c,
                   struct City *c1);
  uint32_t choice_choosen; // Set by the popup handling mechanic and processed
                           // by the callback
};

/// NOTE: All city_add_* functions returns a ptr to the last element added
struct Popup *city_add_popup(struct City *c, const struct Popup p) {
  if (c->num_popups + 1 > c->num_popups_capacity) {
    c->num_popups_capacity += 10;
    c->popups =
        realloc(c->popups, sizeof(struct Popup) * c->num_popups_capacity);
  }
  c->popups[c->num_popups++] = p;
  return &c->popups[c->num_popups - 1];
}

struct Effect *city_add_effect(struct City *c, const struct Effect e) {
  if (c->num_effects + 1 > c->num_effects_capacity) {
    c->num_effects_capacity += 10;
    c->effects =
        realloc(c->effects, sizeof(struct Effect) * c->num_effects_capacity);
  }
  c->effects[c->num_effects++] = e;
  return &c->effects[c->num_effects - 1];
}

struct Construction *city_add_construction(struct City *c,
                                           const struct Construction con) {
  if (c->num_constructions + 1 > c->num_constructions_capacity) {
    c->num_constructions_capacity += 10;
    c->constructions =
        realloc(c->constructions,
                sizeof(struct Construction) * c->num_constructions_capacity);
  }
  c->constructions[c->num_constructions++] = con;
  return &c->constructions[c->num_constructions - 1];
}

struct Construction *
city_add_construction_project(struct City *c, const struct Construction con) {
  if (c->num_construction_projects + 1 >
      c->num_construction_projects_capacity) {
    c->num_construction_projects_capacity += 10;
    c->construction_projects = realloc(
        c->construction_projects,
        sizeof(struct Construction) * c->num_construction_projects_capacity);
  }
  c->construction_projects[c->num_construction_projects++] = con;
  return &c->construction_projects[c->num_construction_projects - 1];
}

struct Policy *city_add_policy(struct City *c, const struct Policy p) {
  if (c->num_policies + 1 > c->num_policies_capacity) {
    c->num_policies_capacity += 10;
    c->policies =
        realloc(c->policies, sizeof(struct Policy) * c->num_policies_capacity);
  }
  c->policies[c->num_policies++] = p;
  return &c->policies[c->num_construction_projects_capacity - 1];
}

/// Calculates the population changes this timestep
void population_calculation(const struct City *c, struct City *c1) {
  assert(c);
  assert(c1);
  // TODO: Formula for population growth depending on food/etc in Roman times
  c1->birthrate += c->birthrate;
  c1->deathrate += c->deathrate;
  c1->immigrationrate += c->immigrationrate;
  c1->emmigrationrate += c->emmigrationrate;

  // BIDE model
  c1->immigrations += c->immigrationrate * c->population;
  c1->births = c->birthrate * c->population;
  c1->deaths = c->deathrate * c->population;
  c1->emmigrations = c->emmigrationrate * c->population;
  c1->population =
      c->population + c->births + c->immigrations - c->deaths - c->emmigrations;
}

/// Apply and deal with the effects in place on the city
void simulate_next_timestep(const struct City *c, struct City *c1) {
  assert(c);
  assert(c1);

  memset(c1, 0, sizeof(struct City)); // Reset next state

  // NOTE: Move shared resources
  c1->name = c->name;
  c1->num_effects = c->num_effects;
  c1->effects = c->effects;
  c1->num_effects_capacity = c->num_effects_capacity;
  c1->num_construction_projects = c->num_construction_projects;
  c1->construction_projects = c->construction_projects;
  c1->num_construction_projects_capacity =
      c->num_construction_projects_capacity;
  c1->num_constructions = c->num_constructions;
  c1->constructions = c->constructions;
  c1->num_constructions_capacity = c->num_constructions_capacity;
  c1->land_area = c->land_area;
  c1->produce_values = c->produce_values;
  c1->log = c->log;
  c1->popups = c->popups;
  c1->num_popups = c->num_popups;
  c1->num_popups_capacity = c->num_popups_capacity;

  // Compute effects affecting the change of rate
  for (int32_t i = 0; i < c1->num_effects; i++) {
    if (c1->effects[i].duration == 0) {
      continue;
    }

    c1->effects[i].tick_effect(&c1->effects[i], c, c1);
    if (c1->effects[i].duration == FOREVER) {
      continue;
    }

    c1->effects[i].duration--;
    if (c1->effects[i].duration == 0) {
      c1->effects[i] = c1->effects[c1->num_effects - 1];
      c1->num_effects--;
      i--;
    }
  }

  // Construction effects
  for (size_t i = 0; i < c1->num_constructions; i++) {
    const struct Construction *con = &c1->constructions[i];
    if (!con->construction_finished) {
      continue;
    }
    if (!con->maintained) {
      continue;
    }
    for (size_t j = 0; j < con->num_effects; j++) {
      // con->effect[j].tick_effect(con->effect, c, c1);
    }
  }

  // Compute changes during this timestep
  population_calculation(c, c1);

  // Compute changes based on current state
  c1->gold = c->gold - c1->gold_usage;

  // Import / Export of foodstuffs
  // TODO: Seperate food prices and demands and consumption
  const float avg_food_price = 0.01f;
  c1->gold += avg_food_price * (c->food_production - c->food_usage);

  timestep++;
  increment_date(&date);
}

void building_maintenance_tick_effect(struct Effect *e, const struct City *c,
                                      struct City *c1) {
  for (size_t i = 0; i < c->num_constructions; i++) {
    c1->gold_usage += c->constructions[i].maintenance;
  }
}

void farm_tick_effect(struct Effect *e, const struct City *c, struct City *c1) {
  c1->birthrate += 0.0000001f;
  if (is_winter(&date)) {
    return;
  }
  c1->food_production += 0.1f;
}

void aqueduct_tick_effect(struct Effect *e, const struct City *c,
                          struct City *c1) {
  c1->diplomatic_capacity += 1;
  c1->immigrationrate += 0.000001f;
}

void basilica_tick_effect(struct Effect *e, const struct City *c,
                          struct City *c1) {
  c1->political_capacity += 1;
}

void forum_tick_effect(struct Effect *e, const struct City *c,
                       struct City *c1) {
  c1->diplomatic_capacity += 1;
  c1->political_capacity += 1;
}

void coin_mint_tick_effect(struct Effect *e, const struct City *c,
                           struct City *c1) {
  c1->gold_usage -= 0.5f;
}

void temple_of_jupiter_tick_effect(struct Effect *e, const struct City *c,
                                   struct City *c1) {
  c1->diplomatic_capacity += 1;
}

void temple_of_mars_tick_effect(struct Effect *e, const struct City *c,
                                struct City *c1) {
  c1->military_capacity += 1;
}

void pops_eating_tick_effect(struct Effect *e, const struct City *c,
                             struct City *c1) {
  const uint32_t consumption = c->population * 0.0001f;
  c1->food_usage += consumption;
}

void colonia_capacity_tick_effect(struct Effect *e, const struct City *c,
                                  struct City *c1) {
  static uint8_t lvl = 1;
  c1->political_capacity += 5 * lvl;
}

void senate_house_tick_effect(struct Effect *e, const struct City *c,
                              struct City *c1) {
  c1->policies_enabled = true;
  c1->diplomatic_capacity += 2;
  c1->political_capacity += 2;
  c1->gold_usage += 0.2f;
}

void land_tax_tick_effect(struct Effect *e, const struct City *c,
                          struct City *c1) {
  const struct FarmArgument *arg = (struct FarmArgument *)e->arg;
  const float land_tax_price = 0.2f;
  c1->gold_usage -= land_tax_price * arg->area *
                    arg->tax_percentage; // TODO: Gold income perhaps ... ?
}

void insula_tick_effect(struct Effect *e, const struct City *c,
                        struct City *c1) {
  // TODO: Insula tick effect
}

void event_log_test_effect(struct Effect *e, const struct City *c,
                           struct City *c1) {
  static int i = 1;
  const int lng = snprintf(NULL, 0, "Message #%u", i) + 1;
  char msg[lng];
  snprintf(msg, lng, "Message #%u", i);
  eventlog_add_msg(c->log, msg);
  i++;
}

void imperator_demands_money_callback(const struct Popup *p,
                                      const struct City *c, struct City *c1) {
  switch (p->choice_choosen) {
  case 0:
    c1->gold -= 50.0f;
    break;
  case 1:
    c1->population -= 50.0f;
    break;
  }
}

// TODO: Expand this to a decision tree and then a story in which you either
// fail or get the choice of moving the capital of the Roman Empire to your city
// or move to Rome and then the game turns in a slow downward turn into decline?
// Fun?
void imperator_demands_money(struct Effect *e, const struct City *c,
                             struct City *c1) {
  if (timestep % 100 != 0) {
    return;
  }
  const float x = rand() / (float)RAND_MAX;
  if (x < 1.0f) {
#ifdef USER_INTERFACE_TUI
    const int h = 10;
    const int w = 40;
    WINDOW *win = create_newwin(h, w, LINES / 2 - h / 2, COLS / 2 - w / 2);
    mvwprintw(
        win, 1, 1,
        "Emperor <EMPEROR-NAME> demands more gold for his construction of "
        "<CONSTRUCTION-NAME> \n \t\t\t y/n");
    nodelay(win, false); /* Make getch non-blocking */
    while (true) {
      const char ch = wgetch(win);
      if (ch == 'y') {
        mvwprintw(win, 1, 1,
                  "Emperor <EMPEROR-NAME> is satisfied and his opinion of you "
                  "has been elevated");
        c1->gold -= 50.0f;
        break;
      }
      if (ch == 'n') {
        mvwprintw(win, 1, 1,
                  "Emperor <EMPEROR-NAME> recalls some of <LEGIO-NAME> from "
                  "<CITY-NAME>");
        c1->population -= 50.0f;
        break;
      }
    }
    destroy_win(win);
#endif
#ifdef USER_INTERFACE_GUI
    struct Popup popup;
    popup.title = "Imperator requires resources ...";
    popup.num_choices = 2;
    popup.choices = (char **)calloc(2, sizeof(char *));
    popup.hover_txts = (char **)calloc(2, sizeof(char *));
    static char *choices[2] = {"Send a wagon of gold!",
                               "Send him the finest Legionnaires!"};
    static char *hover_txts[2] = {"-50.0 gold", "-50 population"};
    popup.choices = choices;
    popup.hover_txts = hover_txts;
    popup.callback = imperator_demands_money_callback;
    city_add_popup(c1, popup);
#endif
  }
}

void building_tick_effect(struct Effect *e, const struct City *c,
                          struct City *c1) {
  struct Construction *arg = (struct Construction *)e->arg;

  c1->gold -= arg->construction_cost; // FIXME: Gold usage instead?

  const int lng = snprintf(NULL, 0, "%ld days left, costing %.2f gold / day",
                           e->duration, arg->construction_cost) +
                  1;
  snprintf(e->description_str, lng, "%ld days left, costing %.2f gold / day",
           e->duration, arg->construction_cost);

  if (e->duration == 1) {
    arg->maintained = true;
    arg->construction_finished = true;
    arg->construction_completed = date;

    const int lng = snprintf(NULL, 0, "Finished construction of a %s.",
                             arg->effect->name_str) +
                    1;
    char msg[lng];
    snprintf(msg, lng, "Finished construction of a %s.",
             arg->effect->name_str);
    eventlog_add_msg(c1->log, msg);

    free(e->name_str);
    e->name_str = NULL;
    free(e->description_str);
    e->description_str = NULL;
  }
}

bool build_construction(struct City *c, const struct Construction cp,
                        const struct Effect activated_effect) {
  if (c->gold >= cp.cost) {
    c->gold -= cp.cost;

    struct Construction *construction = city_add_construction(c, cp);
    construction->maintained = false;
    construction->construction_cost =
        construction->cost / construction->construction_time;
    construction->effect = calloc(1, sizeof(struct Effect));
    construction->construction_started = date;
    // Linking the construction and its active effect
    construction->effect[0] = activated_effect; // TODO: Expand with more than one activated effect
    construction->num_effects = 1;

    int lng = snprintf(NULL, 0, "Building of a %s started ..", cp.name_str) + 1;
    char msg[lng];
    snprintf(msg, lng, "Building of a %s started ..", cp.name_str);
    eventlog_add_msg(c->log, msg);

    lng = snprintf(NULL, 0, "9999 days left, costing %.2f / day",
                   construction->construction_cost) +
          1;
    char *description_str =
        (char *)calloc(lng, sizeof(char)); // Filled by the building_tick_effect

    lng = snprintf(NULL, 0, "Building %s", cp.name_str) + 1;
    char *name_str = (char *)calloc(lng, sizeof(char));
    snprintf(name_str, lng, "Building %s", cp.name_str);

    struct Effect building_effect = {.name_str = name_str,
                                     .description_str = description_str,
                                     .duration = cp.construction_time,
                                     .arg = construction,
                                     .tick_effect = building_tick_effect};
    city_add_effect(c, building_effect);

    return true;
  }
  return false;
}

void construction_menu(struct City *c) {
  const int h = 4 + c->num_construction_projects;
  const int w = 60;
  WINDOW *win = create_newwin(h, w, LINES / 2 - h / 2, COLS / 2 - w / 2);
  keypad(win, true); /* For keyboard arrows 	*/
  mvwprintw(win, 1, 1, "[Q]");
  const char *str = "Constructions";
  mvwprintw(win, 1, w / 2 - strlen(str) / 2, str);

  uint8_t hselector = 0;
  uint8_t selector = 0;
  bool done = false;
  while (!done) {
    uint32_t offset = 0;
    uint32_t s = 2;

    for (size_t i = 0; i < c->num_construction_projects; i++) {
      if (i == selector) {
        wattron(win, A_REVERSE);
      }

      if (i == selector && c->construction_projects[i].num_effects > 0) {
        wmvclrprintw(win, s + i + offset, 1, "%s (%.1f gold)",
                     c->construction_projects[i].effect[hselector].name_str,
                     c->construction_projects[i].cost);

        const uint8_t hinset = w - 7;
        wmvprintw(win, s + i + offset, hinset, "%u / %u", hselector + 1,
                  c->construction_projects[i].num_effects);
      } else {
        wmvclrprintw(win, s + i + offset, 1, "%s (%.1f gold)",
                     c->construction_projects[i].effect[0].name_str,
                     c->construction_projects[i].cost);
      }

      if (i == selector) {
        wattroff(win, A_REVERSE);
        offset = 1;
        wmvclrprintw(win, s + i + offset, 1, " - %s",
                     c->construction_projects[i].description_str);
      }
    }

    switch (wgetch(win)) {
    case C_KEY_DOWN:
      hselector = 0;
      if (selector >= c->num_construction_projects - 1) {
        break;
      }
      selector++;
      break;
    case C_KEY_UP:
      hselector = 0;
      if (selector == 0) {
        break;
      }
      selector--;
      break;
    case C_KEY_ENTER:
      done = build_construction(
          c, c->construction_projects[selector],
          c->construction_projects[selector]
              .effect[hselector %
                      c->construction_projects[selector].num_effects]);
      if (done) {
        wclear(root);
      }
      break;
    case C_KEY_LEFT:
      if (hselector == 0) {
        break;
      }
      hselector--;
      break;
    case C_KEY_RIGHT:
      if (hselector == c->construction_projects[selector].num_effects - 1) {
        break;
      }
      hselector++;
      break;
    case 'q':
      done = true;
      break;
    }
  }
  destroy_win(win);
}

bool quit_menu(struct City *c) {
  // TODO: Export City state to JSON
  // TODO: Load City state from JSON
  // TODO: Quit & Save, Save, Restart, Quit
  return true;
}

void policy_menu(struct City *c) {
  const int h = 2 + c->num_policies;
  const int w = 60;
  WINDOW *win = create_newwin(h, w, LINES / 2 - h / 2, COLS / 2 - w / 2);
  keypad(win, true); /* For keyboard arrows 	*/
  mvwprintw(win, 1, 1, "[Q]");
  const char *str = "Policies";
  mvwprintw(win, 1, w / 2 - strlen(str) / 2, str);

  uint8_t hselector = 0;
  uint8_t selector = 0;
  bool done = false;
  while (!done) {
    uint32_t offset = 0;
    uint32_t s = 2;

    for (size_t i = 0; i < c->num_policies; i++) {
      if (i == selector) {
        wattron(win, A_REVERSE);
      }

      if (i == selector && c->policies[i].num_effects > 0) {
        wmvclrprintw(win, s + i + offset, 1, "[%s]: %s (%u cost)",
                     lut_capacity_str(c->policies[i].type),
                     c->policies[i].effect[hselector].name_str,
                     c->policies[i].cost);

        const uint8_t hinset = w - 7;
        wmvprintw(win, s + i + offset, hinset, "%u / %u", hselector + 1,
                  c->policies[i].num_effects);
      } else {
        wmvclrprintw(win, s + i + offset, 1, "%s (%u cost)",
                     c->policies[i].effect[0].name_str, c->policies[i].cost);
      }

      if (i == selector) {
        wattroff(win, A_REVERSE);
        offset = 1;
        wmvclrprintw(win, s + i + offset, 1, "%s",
                     c->policies[i].description_str);
      }
    }

    switch (wgetch(win)) {
    case C_KEY_DOWN:
      hselector = 0;
      if (selector >= c->num_policies - 1) {
        break;
      }
      selector++;
      break;
    case C_KEY_UP:
      hselector = 0;
      if (selector <= 0) {
        break;
      }
      selector--;
      break;
    case C_KEY_ENTER:
      // done = implement_policy(c->policies[i].effects[hselector])
      if (done) {
        wclear(root);
      }
      break;
    case C_KEY_LEFT:
      if (hselector == 0) {
        break;
      }
      hselector--;
      break;
    case C_KEY_RIGHT:
      if (hselector == c->policies[selector].num_effects - 1) {
        break;
      }
      hselector++;
      break;
    case 'q':
      done = true;
      break;
    }
  }
  destroy_win(win);
}

void help_menu(struct City *c) {
  // TODO: Implement help menu
  // TODO: There should be a explain word/concept discovery functionality like
  // Emacs's explain-function
}

/// Display terminal-based user interface
void update_tui(const struct City *c) {
  assert(c);
  int row = 0;
  wmvclrprintw(root, row++, 0, "%s", c->name);
  wmvclrprintw(root, row++, 0, "%s, day %s of %s, %s", get_year_str(&date),
               roman_numeral_str(date.day + 1), get_month_str(date),
               get_season_str(&date));

  // TODO: Merge effects from constructions with effects
  row += 1;
  wmvclrprintw(root, row++, 0, "[1] EFFECTS [+]");
  for (size_t i = 0; i < c->num_effects; i++) {
    if (c->effects[i].name_str) {
      wmvclrprintw(root, row++, 0, "%s: %s", c->effects[i].name_str,
                   c->effects[i].description_str);
    }
  }

  row += 1;
  wmvclrprintw(root, row++, 0, "[2] CONSTRUCTIONS [+]");
  for (size_t i = 0; i < c->num_constructions; i++) {
    struct Construction *con = &c->constructions[i];
    if (!con->construction_finished) {
      continue;
    }
    if (con->maintained) {
      wmvclrprintw(root, row++, 0, "%s: %s", con->effect[0].name_str,
                   con->effect[0].description_str);
    } else {
      wmvclrprintw(root, row++, 0, "[UNMAINTAINED] %s: %s",
                   con->effect[0].name_str, con->effect[0].description_str);
    }
  }

  row += 1;
  wmvclrprintw(root, row++, 0, "[3] RESOURCES [+]");
  wmvclrprintw(root, row++, 0, "Gold (%'.2f): %.2f kg", c->gold_usage, c->gold);
  wmvclrprintw(root, row++, 0, "Land: %u jugerum", c->land_area);
  wmvclrprintw(root, row++, 0, "Food consumption: %'.1f kcal",
               c->food_production - c->food_usage);
  if (c->food_production - c->food_usage > 0.0f) {
    wmvclrprint(root, row++, 0, "EXPORTING FOOD");
  } else {
    wmvclrprint(root, row++, 0, "IMPORTING FOOD");
  }

  row += 1;
  wmvclrprintw(root, row++, 0, "Political  power: %u / %u", c->political_usage,
               c->political_capacity);
  wmvclrprintw(root, row++, 0, "Military   power: %u / %u", c->military_usage,
               c->military_capacity);
  wmvclrprintw(root, row++, 0, "Diplomatic power: %u / %u", c->diplomatic_usage,
               c->diplomatic_capacity);

  row += 1;
  wmvclrprintw(root, row++, 0, "[4] DEMOGRAPHICS [+]");
  wmvclrprintw(root, row++, 0, "Population: %'.0f", c->population);
  // TODO: Subtotal - military abled men X (of which Y are available due to
  // various reasons)
  wmvclrprintw(root, row++, 0, "Births: %'.1f / day", c->births);
  wmvclrprintw(root, row++, 0, "Deaths: %'.1f / day", c->deaths);
  wmvclrprintw(root, row++, 0, "Immigrations: %'.1f / day", c->immigrations);
  wmvclrprintw(root, row++, 0, "Emmigrations: %'.1f / day", c->emmigrations);

  // TODO: Implement controls and menu system
  wmvclrprintw(
      root, LINES - 1, 0,
      "Speed: %u / 9 | Q: menu | C: construction | P: policy | H: help",
      simulation_speed);
  // TODO: Show drastically declining (good) numbers in reddish, yellowish for
  // stable values, green for increasing (good) values and vice versa for bad
  // values like deaths
  // TODO: Show deltas for the month next to the current values
}

#ifdef USER_INTERFACE_GUI
void glfw_error_callback(int e, const char *d) {
  fprintf(stderr, "[glfw3]: Error %d: %s", e, d);
}

struct nk_image nk_image_load(const char *filename) {
  int x, y, n;
  GLuint tex;
  unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
  if (!data) {
    fprintf(stderr, "[SDL]: failed to load image: %s", filename);
  }

  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  GL_LINEAR_MIPMAP_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               data);
  glGenerateMipmap(GL_TEXTURE_2D);
  stbi_image_free(data);
  return nk_image_id((int)tex);
}

// TODO:
void gui_quit_menu(const struct City *c, struct nk_context *ctx) {
  assert(c);
  assert(ctx);
}

void gui_construction_menu(struct City *c, struct nk_context *ctx) {
  const nk_flags win_flags = NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE |
                             NK_WINDOW_CLOSABLE | NK_WINDOW_SCALABLE;
  if (nk_begin(ctx, "Construction", nk_rect(200, 500, 800, 300), win_flags)) {
    if (nk_tree_push(ctx, NK_TREE_TAB, "Construction projects", NK_MAXIMIZED)) {
      for (size_t i = 0; i < c->num_construction_projects; i++) {
        int32_t selector =
            0; // TODO: Implement selection of different building types
        const struct Construction *proj = &c->construction_projects[i];
        nk_layout_row_dynamic(ctx, 0.0f, 4);
        nk_label(ctx, proj->name_str, NK_TEXT_ALIGN_LEFT);
        nk_label(ctx, proj->description_str, NK_TEXT_ALIGN_LEFT);
        const int lng = snprintf(NULL, 0, "%.2f gold / %.2ld days", proj->cost,
                                 proj->construction_time) +
                        1;
        char str[lng];
        snprintf(str, lng, "%.2f gold / %.2ld days", proj->cost,
                 proj->construction_time);
        nk_label(ctx, str, NK_TEXT_ALIGN_CENTERED);
        if (nk_button_label(ctx, "Build")) {
          build_construction(c, *proj, proj->effect[selector]);
          // TODO: Add visual indication or smt to show that building failed or started
        }
      }
      nk_tree_pop(ctx);
    }

    if (nk_tree_push(ctx, NK_TREE_TAB, "Constructions", NK_MINIMIZED)) {
      for (size_t i = 0; i < c->num_constructions; i++) {
        const struct Construction *con = &c->constructions[i];
        // TODO: snprintf(str, lng, "%s built %s", con->name_str,
        // str_from_date(con->construction_date));
        const int lng =
            snprintf(NULL, 0, "%s - %s", con->name_str, con->description_str) +
            1;
        char str[lng];
        snprintf(str, lng, "%s - %s", con->name_str, con->description_str);

        nk_layout_row_dynamic(ctx, 0.0f, 2);
        nk_label(ctx, str, NK_TEXT_ALIGN_LEFT);

        // Construction detail pane
        nk_menubar_begin(ctx);
        if (nk_menu_begin_label(ctx, "!", NK_TEXT_ALIGN_RIGHT, nk_vec2(100, 200))) {
          nk_layout_row_dynamic(ctx, 0.0f, 1);

          if (nk_menu_item_label(ctx, "Demolish", NK_TEXT_ALIGN_CENTERED)) {
            // TODO: Implement demolishion of constructions (for resources a.k.a
            // gold)
          }

          if (nk_menu_item_label(ctx, "?", NK_TEXT_ALIGN_CENTERED)) {
            // TODO: Implement information pane about constructions (useful for
            // micromanagement)
          }
        }
        nk_menubar_end(ctx);

      }
      nk_tree_pop(ctx);
    }
  }
  nk_end(ctx);
}

void gui_event_log(const struct City *c, struct nk_context *ctx) {
  const nk_flags win_flags = NK_WINDOW_MOVABLE | NK_WINDOW_BORDER |
                             NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE;
  if (nk_begin(ctx, "Event log", nk_rect(50, 600, 600, 400), win_flags)) {
    nk_button_label(ctx, "HELLO");
    char *msg = NULL;
    while (eventlog_next_msg(c->log, &msg)) {
      nk_layout_row_dynamic(ctx, 0.0f, 1);
      nk_label(ctx, msg, NK_TEXT_ALIGN_LEFT);
    }
  }
  nk_end(ctx);
}

void gui_military_menu(struct City *c, struct nk_context *ctx) {
  // TODO:
}

void gui_help_menu(struct City *c, struct nk_context *ctx) {
  // TODO: Help menu is used to look things up and search for in-game things
}

// TODO: Esc should open this menu
void gui_menu(struct City *c, struct nk_context *ctx) {
  const nk_flags win_flags = NK_WINDOW_MOVABLE | NK_WINDOW_BORDER |
                             NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE;
  const uint32_t win_width = 500;
  const uint32_t win_height = 500;
  const struct nk_rect win_rect =
      nk_rect((CONFIG.RESOLUTION.width / 2.0f) - (win_width / 2.0f),
              (CONFIG.RESOLUTION.height / 2.0f) - (win_height / 2.0f),
              win_width, win_height);
  if (nk_begin(ctx, "Menu", win_rect, win_flags)) {
    nk_layout_row_dynamic(ctx, 0.0f, 1);
    if (nk_button_label(ctx, "Save game")) {
      // TODO: Save game
    }

    if (nk_button_label(ctx, "Load game")) {
      // TODO: Load game
    }

    if (nk_button_label(ctx, "Quit")) {
      // TODO: Quit game
    }
  }
  nk_end(ctx);
}

/// GUI specific resources initialized once at startup
static struct {
  struct nk_image policy_icon;
  struct nk_image construction_icon;
} GUI;

// ----------- Custom GUI widgets  -----------

void gui_widget_capacity(struct City*c, struct nk_context* ctx, const char* name) {
  if (nk_group_begin(ctx, name, NK_WINDOW_NO_SCROLLBAR)) {

    static const float ratio[3] = {0.25f, 0.5f, 0.25f};
    nk_layout_row(ctx, NK_DYNAMIC, 0.0f, 3, ratio);
    nk_spacing(ctx, 1);

    if (nk_group_begin(ctx, "inner", NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {

      nk_layout_row_dynamic(ctx, 35.0f, 1);
      nk_label(ctx, name, NK_TEXT_ALIGN_CENTERED);
      nk_label(ctx, "1 / 3", NK_TEXT_ALIGN_CENTERED);
      nk_group_end(ctx);
    }

    nk_spacing(ctx, 1);

    nk_group_end(ctx);
  }
}

// TODO: Remember the windows placement between opening & closing
// Display graphical-based user interface (GUI)
void update_gui(struct City *c, struct nk_context *ctx) {
  // NOTE: Nuklear does not allow nested windows this is a work-around.
  static bool open_construction_window = false;
  static bool open_event_log_window = false;
  static bool open_help_window = false;
  static bool open_military_window = false;
  static bool open_diplomatic_window = false;
  static bool open_civic_window = false;

  // Main window
  const nk_flags main_win_flags =
      NK_WINDOW_BORDER | NK_WINDOW_MINIMIZABLE | NK_WINDOW_MOVABLE;

  if (nk_begin(ctx, c->name, nk_rect(50, 50, 800, 600), main_win_flags)) {
    nk_layout_row_dynamic(ctx, 0.0f, 1);
    const int lng =
        snprintf(NULL, 0, "%s, day %s of %s, %s", get_year_str(&date),
                 roman_numeral_str(date.day + 1), get_month_str(date),
                 get_season_str(&date)) +
        1;
    char str[lng];
    snprintf(str, lng, "%s, day %s of %s, %s", get_year_str(&date),
             roman_numeral_str(date.day + 1), get_month_str(date),
             get_season_str(&date));
    nk_label(ctx, str, NK_TEXT_ALIGN_CENTERED);

    nk_layout_row_dynamic(ctx, 0.0f, 1);
    nk_slider_int(ctx, 0, (int*) (&simulation_speed), 11, 1);
    nk_layout_row_dynamic(ctx, 0.0f, 10);
    for (size_t i = 0; i < 10; i++) {
      char str[4];
      snprintf(str, 4, "%li", i);
      nk_label(ctx, str, NK_TEXT_ALIGN_CENTERED);
    }

    nk_layout_row_static(ctx, 50, 100, 6);

    if (nk_button_label(ctx, "Military")) {
      open_military_window = !open_military_window;
    }

    if (nk_button_label(ctx, "Diplomatic")) {
      open_diplomatic_window = !open_diplomatic_window;
    }

    if (nk_button_label(ctx, "Civic")) {
      open_civic_window = !open_civic_window;
    }

    if (nk_button_label(ctx, "Event log")) {
      open_event_log_window = !open_event_log_window;
    }

    if (nk_button_image(ctx, GUI.construction_icon)) {
      open_construction_window = !open_construction_window;
    }

    if (nk_button_label(ctx, "?")) {
      open_help_window = !open_help_window;
    }

    // Capacities
    nk_layout_row_dynamic(ctx, 35, 3);
    gui_widget_capacity(c, ctx, "Military");
    gui_widget_capacity(c, ctx, "Civic");
    gui_widget_capacity(c, ctx, "Diplomatic");

    // Other vitals
    nk_layout_row_dynamic(ctx, 35, 2);
    gui_widget_capacity(c, ctx, "Gold");
    gui_widget_capacity(c, ctx, "Food");

    if (nk_tree_push(ctx, NK_TREE_TAB, "Statistics", NK_MINIMIZED)) {
      nk_layout_row_dynamic(ctx, 0.0f, 1);
      nk_label(ctx, "Population: 1'000", NK_TEXT_ALIGN_LEFT);

      const int lng =
          snprintf(NULL, 0, "Gold: %.2f (%.2f)", c->gold, c->gold_usage) + 1;
      char gold_str[lng];
      snprintf(gold_str, lng, "Gold: %.2f (%.2f)", c->gold, c->gold_usage);
      nk_layout_row_dynamic(ctx, 0.0f, 1);
      nk_label(ctx, gold_str, NK_TEXT_ALIGN_LEFT);

      nk_tree_pop(ctx);
    }

    // Effects
    if (nk_tree_push(ctx, NK_TREE_TAB, "Effects", NK_MAXIMIZED)) {
      for (size_t i = 0; i < c->num_effects; i++) {
        const struct Effect *e = &c->effects[i];
        if (e->name_str) {
          const int lng =
              snprintf(NULL, 0, "%s - %s", e->name_str, e->description_str) + 1;
          snprintf(str, lng, "%s - %s", e->name_str, e->description_str);
          nk_label(ctx, str, NK_TEXT_ALIGN_LEFT);
        }
      }
      nk_tree_pop(ctx);
    }
  }
  nk_end(ctx);

  if (open_construction_window) {
    gui_construction_menu(c, ctx);
  }

  if (open_event_log_window) {
    gui_event_log(c, ctx);
  }

  if (open_help_window) {
    gui_help_menu(c, ctx);
  }

  if (open_military_window) {
    gui_military_menu(c, ctx);
  }
}
#endif

// Parses the config.json at the project root and inits the Config struct at
// startup
void parse_config_file() {
  const char *raw_json = open_file("config.json");

  if (raw_json) {
    cJSON *json = cJSON_Parse(raw_json);

    if (json) {
      struct cJSON *root_folder =
          cJSON_GetObjectItemCaseSensitive(json, "root_folder");
      if (cJSON_IsString(root_folder) && root_folder->valuestring) {
        CONFIG.FILEPATH_ROOT = root_folder->valuestring;
      }

      struct cJSON *gui = cJSON_GetObjectItem(json, "gui");
      if (cJSON_IsBool(gui)) {
        CONFIG.GUI = gui->valueint;
      }

      struct cJSON *hard_mode = cJSON_GetObjectItem(json, "hard_mode");
      if (cJSON_IsBool(hard_mode)) {
        CONFIG.HARD_MODE = hard_mode->valueint;
      }

      struct cJSON *language = cJSON_GetObjectItem(json, "language");
      if (cJSON_IsNumber(language)) {
        CONFIG.LANGUAGE = language->valueint;
      }

      struct cJSON *resolution = cJSON_GetObjectItem(json, "resolution");
      if (cJSON_IsObject(resolution)) {
        struct Resolution res;

        struct cJSON *width = cJSON_GetObjectItem(resolution, "width");
        if (cJSON_IsNumber(width)) {
          res.width = width->valueint;
        }

        struct cJSON *height = cJSON_GetObjectItem(resolution, "height");
        if (cJSON_IsNumber(height)) {
          res.height = height->valueint;
        }

        CONFIG.RESOLUTION = res;
      }

      struct cJSON *fullscreen = cJSON_GetObjectItem(json, "fullscreen");
      if (cJSON_IsBool(fullscreen)) {
        CONFIG.FULLSCREEN = fullscreen->valueint;
      }

    } else {
      const char *error_ptr = cJSON_GetErrorPtr();
      if (error_ptr) {
        fprintf(stderr, "[ColoniaC]: cJSON error before: %s \n", error_ptr);
      }
    }
  } else {
    fprintf(stderr, "[ColoniaC]: Failed to load config.json");
  }
  CONFIG.FILEPATH_RSRC = str_concat_new(CONFIG.FILEPATH_ROOT, "resources/");
}

int main(void) {
  srand(time(NULL));
  parse_config_file();

#ifdef USER_INTERFACE_GUI
  /* SDL setup */
  SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_WindowFlags win_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
  if (CONFIG.FULLSCREEN) {
    win_flags |= SDL_WINDOW_FULLSCREEN;
  }
  SDL_Window *sdl_window = SDL_CreateWindow(
      "ColoniaC", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      CONFIG.RESOLUTION.width, CONFIG.RESOLUTION.height, win_flags);
  SDL_GL_CreateContext(sdl_window);

  // OpenGL
  glViewport(0, 0, CONFIG.RESOLUTION.width, CONFIG.RESOLUTION.height);
  glewExperimental = true;
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Error could not initalize GLEW \n");
    return -1;
  }

  // Init Nuklear - GUI
  struct nk_context *ctx = nk_sdl_init(sdl_window);

  const bool USE_CUSTOM_FONT = true;
  if (USE_CUSTOM_FONT) {
    struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);
    const float FONT_HEIGHT = 15.0f;
    const char *font_name = "fonts/CONSTANTINE/Constantine.ttf";
    const char *font_filepath = str_concat_new(CONFIG.FILEPATH_RSRC, font_name);
    struct nk_font *font =
        nk_font_atlas_add_from_file(atlas, font_filepath, FONT_HEIGHT, NULL);
    if (font_filepath) {
      free((void *)font_filepath);
    }
    if (font == NULL) {
      fprintf(stderr, "Could not load custom font. \n");
      return -1;
    }
    nk_sdl_font_stash_end();
    nk_style_set_font(ctx, &font->handle);
  } else {
    struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);
    nk_sdl_font_stash_end();
  }

  const char *icon_name = "icons/greek-temple.png";
  const char *filepath = str_concat_new(CONFIG.FILEPATH_RSRC, icon_name);
  GUI.construction_icon = nk_image_load(filepath);
  if (filepath) {
    free((void *)filepath);
  }
#endif

#ifdef USER_INTERFACE_TERMINAL
  root = initscr();     /* initialize the curses library */
  cbreak();             /* Line buffering disabled pass on everything to me*/
  keypad(stdscr, true); /* For keyboard arrows 	*/
  noecho();             /* Do not echo out input */
  nodelay(root, true);  /* Make getch non-blocking */
  refresh();
#endif

  struct City *cities = (struct City *)calloc(2, sizeof(struct City));

  struct City *city = &cities[0];
  city->name = "Eboracum";
  city->gold = 100.0f;
  city->population = 300.0f;
  city->birthrate = 0.00021f;
  city->deathrate = 0.0002f;
  city->emmigrationrate = 0.0005f;
  city->immigrationrate = 0.0020f;
  city->land_area = 100;
  city->produce_values = (float *)calloc(sizeof(float), NUMBER_OF_PRODUCE);
  city->produce_values[Grapes] = 0.35f;
  city->produce_values[Wheat] = 0.45f;
  struct EventLog log = eventlog_new();
  city->log = &log;

  struct Effect aqueduct_construction_effect = {
      .name_str = "Aqueduct",
      .description_str = "Provides drinking water and bathing water",
      .duration = FOREVER,
      .tick_effect = aqueduct_tick_effect};

  struct Construction aqueduct = {.name_str = "Aqueduct",
                                  .description_str =
                                      "Provides fresh water for the city.",
                                  .cost = 25.0f,
                                  .maintenance = 0.25f,
                                  .construction_time = 6 * 30,
                                  .effect = &aqueduct_construction_effect,
                                  .num_effects = 1};

  struct FarmArgument grape_farm_arg = {.produce = Grapes, .area = 0};

  struct Effect grape_farm_construction_effect = {
      .name_str = "Farm",
      .description_str = "piece of land that produces",
      .duration = FOREVER,
      .arg = &grape_farm_arg,
      .tick_effect = farm_tick_effect};

  struct FarmArgument wheat_farm_arg = {.produce = Wheat, .area = 0};

  struct Effect wheat_farm_construction_effect = {
      .name_str = "Farm",
      .description_str = "piece of land that produces",
      .duration = FOREVER,
      .arg = &wheat_farm_arg,
      .tick_effect = farm_tick_effect};

  struct Effect farm_construction_effects[2] = {grape_farm_construction_effect,
                                                wheat_farm_construction_effect};

  struct Construction farm = {.cost = 2.0f,
                              .construction_time = 7,
                              .maintenance = 0.0f,
                              .name_str = "Farm",
                              .description_str =
                                  "Piece of land generating various produce.",
                              .effect = farm_construction_effects,
                              .num_effects = 2};

  struct Effect basilica_construction_effect = {
      .name_str = "Basilica",
      .description_str = "Public building ...",
      .duration = FOREVER,
      .tick_effect = basilica_tick_effect};

  struct Construction basilica = {.cost = 15.0f,
                                  .maintenance = 0.2f,
                                  .construction_time = 3 * 30,
                                  .name_str = "Basilica",
                                  .description_str =
                                      "Public building used official matters.",
                                  .effect = &basilica_construction_effect,
                                  .num_effects = 1};

  struct Effect forum_construction_effect = {.name_str = "Forum",
                                             .description_str =
                                                 "Public space for commerce.",
                                             .duration = FOREVER,
                                             .tick_effect = forum_tick_effect};

  struct Construction forum = {.cost = 50.0f,
                               .maintenance = 0.5f,
                               .construction_time = 12 * 30,
                               .name_str = "Forum",
                               .description_str = "Public space for commerce.",
                               .effect = &forum_construction_effect,
                               .num_effects = 1};

  struct Effect coin_mint_construction_effect = {
      .name_str = "Coin mint",
      .description_str =
          "Produces coinage, ensures commerce is not disrupted by war.",
      .duration = FOREVER,
      .tick_effect = coin_mint_tick_effect};

  struct Construction coin_mint = {
      .cost = 30.0f,
      .maintenance = 0.1f,
      .construction_time = 2 * 30,
      .name_str = "Coin mint",
      .description_str =
          "Produces coinage, ensures commerce is not disrupted by war.",
      .effect = &coin_mint_construction_effect,
      .num_effects = 1};

  const size_t num_temples = 2;
  struct Effect *temple_effects =
      (struct Effect *)calloc(num_temples, sizeof(struct Effect));

  struct Effect temple_of_mars_construction_effect = {
      .name_str = "Temple of Mars",
      .description_str = "House of the God of warfare.",
      .duration = FOREVER,
      .tick_effect = temple_of_mars_tick_effect};

  struct Effect temple_of_jupiter_construction_effect = {
      .name_str = "Temple of Jupiter",
      .description_str = "Located at the north-face of the Forum.",
      .duration = FOREVER,
      .tick_effect = temple_of_jupiter_tick_effect};

  temple_effects[0] = temple_of_jupiter_construction_effect;
  temple_effects[1] = temple_of_mars_construction_effect;

  struct Construction temple = {
      .name_str = "Temple",
      .description_str = "Used in sacrifies and other Roman traditions",
      .cost = 25.0f,
      .maintenance = 0.15f,
      .construction_time = 4 * 30,
      .effect = temple_effects,
      .num_effects = num_temples};

  struct Effect senate_house_construction_effect = {
      .name_str = "Senate house",
      .description_str = "Enables policies to be enacted",
      .duration = FOREVER,
      .tick_effect = senate_house_tick_effect};

  struct Construction senate_house = {
      .name_str = "Senate house",
      .description_str = "Meeting place of the ruling class",
      .cost = 50.0f,
      .maintenance = 0.05f,
      .construction_time = 30,
      .effect = &senate_house_construction_effect,
      .num_effects = 1};

  struct Effect insula_construction_effect;
  insula_construction_effect.name_str = "Insula";
  insula_construction_effect.duration = 0;
  insula_construction_effect.arg = 0;
  insula_construction_effect.tick_effect = insula_tick_effect;

  struct Construction insula = {.name_str = "Insula",
                                .description_str =
                                    "Apartment block with space for ",
                                .cost = 10.0f,
                                .maintenance = 0.05f,
                                .construction_time = 30,
                                .effect = &insula_construction_effect,
                                .num_effects = 1};

  city_add_construction_project(city, insula);
  city_add_construction_project(city, senate_house);
  city_add_construction_project(city, aqueduct);
  city_add_construction_project(city, farm);
  city_add_construction_project(city, basilica);
  city_add_construction_project(city, forum);
  city_add_construction_project(city, coin_mint);
  city_add_construction_project(city, temple);

  city_add_construction(city, farm);
  city_add_construction(city, farm);

  struct Effect pops_food_eating = {.duration = FOREVER};
  pops_food_eating.tick_effect = pops_eating_tick_effect;

  struct Effect emperor_gold_demands = {.duration = FOREVER};
  emperor_gold_demands.tick_effect = imperator_demands_money;

  struct Effect building_maintenance = {.duration = FOREVER};
  building_maintenance.tick_effect = building_maintenance_tick_effect;

  struct Effect colonia_capacity = {.duration = FOREVER};
  colonia_capacity.name_str = "Colonia organisation";
  colonia_capacity.description_str = "Established colonia.";
  colonia_capacity.tick_effect = colonia_capacity_tick_effect;

  struct Effect event_log_tester = {.duration = FOREVER};
  event_log_tester.name_str = "Debug Event";
  event_log_tester.description_str = "Testing the event log";
  event_log_tester.tick_effect = event_log_test_effect;

  city_add_effect(city, event_log_tester);
  city_add_effect(city, pops_food_eating);
  city_add_effect(city, emperor_gold_demands);
  city_add_effect(city, building_maintenance);
  city_add_effect(city, colonia_capacity);

  /// Policies
  city->num_policies = 0;
  city->political_capacity = 10;
  city->policies =
      (struct Policy *)calloc(city->num_policies, sizeof(struct Policy));

  struct Effect land_tax_effect = {.duration = FOREVER};
  land_tax_effect.tick_effect = land_tax_tick_effect;

  struct Policy land_tax;
  land_tax.name_str = "Enact land tax";
  land_tax.description_str = "Places a tax on owning land.";
  land_tax.cost = 1;
  land_tax.type = Political;
  land_tax.effect = &land_tax_effect;
  land_tax.num_effects = 1;

  city_add_policy(city, land_tax);

  bool quit = false;

  struct timeval t0;
  struct timeval t1;
  gettimeofday(&t0, NULL);
  uint64_t dt = 0;
  uint8_t cidx = 0;

  while (!quit) {
    gettimeofday(&t1, NULL);
    dt = ((t1.tv_sec * 1000) + (t1.tv_usec / 1000)) -
         ((t0.tv_sec * 1000) + (t0.tv_usec / 1000));

    const uint32_t ms_per_timestep = ms_per_timestep_for(simulation_speed);
    if (dt >= ms_per_timestep && ms_per_timestep != 0) {
      simulate_next_timestep(&cities[cidx], &cities[(cidx + 1) % 2]);
      cidx = (cidx + 1) % 2;
      t0 = t1;
    }

#ifdef USER_INTERFACE_GUI
    /* Input */
    SDL_Event evt;
    nk_input_begin(ctx);
    while (SDL_PollEvent(&evt)) {
      if (evt.type == SDL_QUIT) {
        return 0;
      }
      if (evt.type == SDL_KEYDOWN) {
        switch (evt.key.keysym.sym) {
        case SDLK_ESCAPE:
          gui_quit_menu(&cities[cidx], ctx);
          return 0;
          break;
        case SDLK_SPACE:
          if (simulation_speed == 0) {
            simulation_speed = 5; // TODO: last_simulation_speed;
          } else {
            simulation_speed = 0;
          }
          break;
        case SDLK_0:
          simulation_speed = 0;
          break;
        case SDLK_1:
          simulation_speed = 1;
          break;
        case SDLK_2:
          simulation_speed = 2;
          break;
        case SDLK_3:
          simulation_speed = 3;
          break;
        case SDLK_4:
          simulation_speed = 4;
          break;
        case SDLK_5:
          simulation_speed = 5;
          break;
        case SDLK_6:
          simulation_speed = 6;
          break;
        case SDLK_7:
          simulation_speed = 7;
          break;
        case SDLK_8:
          simulation_speed = 8;
          break;
        case SDLK_9:
          simulation_speed = 9;
          break;
        }
      }
      nk_sdl_handle_event(&evt);
    }
    nk_input_end(ctx);
    update_gui(&cities[cidx], ctx);
    SDL_GetWindowSize(sdl_window, &CONFIG.RESOLUTION.width,
                      &CONFIG.RESOLUTION.height);
    glViewport(0, 0, CONFIG.RESOLUTION.width, CONFIG.RESOLUTION.height);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
    SDL_GL_SwapWindow(sdl_window);
#endif

#ifdef USER_INTERFACE_TERMINAL
    werase(root);
    update_tui(&cities[cidx]);

    char ch = getch();

    switch (ch) {
    case ' ':
    case '0':
      simulation_speed = 0;
      break;
    case '1':
      simulation_speed = 1;
      break;
    case '2':
      simulation_speed = 2;
      break;
    case '3':
      simulation_speed = 3;
      break;
    case '4':
      simulation_speed = 4;
      break;
    case '5':
      simulation_speed = 5;
      break;
    case '6':
      simulation_speed = 6;
      break;
    case '7':
      simulation_speed = 7;
      break;
    case '8':
      simulation_speed = 8;
      break;
    case '9':
      simulation_speed = 9;
      break;
    case ' ':
      simulation_speed = 0;
      break;
    case 'h':
      help_menu(&cities[cidx]);
      break;
    case 'q':
      if (quit_menu(&cities[cidx])) {
        return 0;
      }
      break;
    case 'c':
      construction_menu(&cities[cidx]);
      break;
    case 'p':
      policy_menu(&cities[cidx]);
      break;
    }
#endif
  }
  if (CONFIG.FILEPATH_ROOT) {
    free((void *)CONFIG.FILEPATH_ROOT);
  }
  if (CONFIG.FILEPATH_RSRC) {
    free((void *)CONFIG.FILEPATH_RSRC);
  }
  return 0;
}
