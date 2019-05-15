#include <ncurses.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

// TODO: Add an event log
// Event: "Our scouts report that barbarians are gathering under the cmd of
// <BARBARIAN-CHIEF-NAME>" Housing? Water? Aqueducts and baths? Diseases? Food
// spoiling? Food production should vary with seasons

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

/***** ncurses utility functions *****/
/// MVove, CLearR, PRINT Word
static inline void mvclrprintw(WINDOW *win, int y, int x, const char *fmt,
                               ...) {
  wmove(win, y, x);
  wclrtoeol(win);
  va_list args;
  va_start(args, fmt);
  vwprintw(win, fmt, args);
}

// Allocs a new window and sets a box around it plus displays it
WINDOW *create_newwin(const int height, const int width, const int starty,
                      const int startx) {
  WINDOW *local_win;
  local_win = newwin(height, width, starty, startx);
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

static WINDOW *root = NULL; // FIXME: ...

// NOTE: Julian calendar introduced Jan. 1st of 45 BC
struct Date {
  uint32_t day;
  uint32_t month;
  uint32_t year;
};

// NOTE: Not historically correct in classic Latin times (Kal, a.d VI Non, etc)
static struct Date date;

// NOTE: Modern Roman numerals (I, V, X, L, C, D, M), (1, 5, 10, 50, 100, 500, 1000)
// NOTE: Using subtractive notation
// API: Callee-responsible for freeing the returned pointer
static inline const char* roman_numeral_str(const uint32_t n) {
  const div_t M = div(n, 1000);
  const div_t C = div(M.rem, 100);
  const div_t X = div(C.rem, 10);
  const div_t I = div(X.rem, 1);

  char Ms[M.quot];
  for (int32_t i = 0; i < M.quot; i++) {
    Ms[i] = (char) 'M';
  }

  char* Cs = NULL;
  size_t Cs_size = 0;
  switch (C.quot) {
  case 1: Cs = "C";    Cs_size = 1; break;
  case 2: Cs = "CC";   Cs_size = 2; break;
  case 3: Cs = "CCC";  Cs_size = 3; break;
  case 4: Cs = "CD";   Cs_size = 2; break;
  case 5: Cs = "D";    Cs_size = 1; break;
  case 6: Cs = "DC";   Cs_size = 2; break;
  case 7: Cs = "DCC";  Cs_size = 3; break;
  case 8: Cs = "DCCC"; Cs_size = 4; break;
  case 9: Cs = "CM";   Cs_size = 2; break;
  }

  char* Xs = NULL;
  size_t Xs_size = 0;
  switch (X.quot) {
  case 1: Xs = "X";    Xs_size = 1; break;
  case 2: Xs = "XX";   Xs_size = 2; break;
  case 3: Xs = "XXX";  Xs_size = 3; break;
  case 4: Xs = "XL";   Xs_size = 2; break;
  case 5: Xs = "L";    Xs_size = 1; break;
  case 6: Xs = "LX";   Xs_size = 2; break;
  case 7: Xs = "LXX";  Xs_size = 3; break;
  case 8: Xs = "LXXX"; Xs_size = 4; break;
  case 9: Xs = "XC";   Xs_size = 2; break;
  }

  char* Is = NULL;
  size_t Is_size = 0;
  switch (I.quot) {
  case 1: Is = "I";    Is_size = 1; break;
  case 2: Is = "II";   Is_size = 2; break;
  case 3: Is = "III";  Is_size = 3; break;
  case 4: Is = "IV";   Is_size = 2; break;
  case 5: Is = "V";    Is_size = 2; break;
  case 6: Is = "VI";   Is_size = 2; break;
  case 7: Is = "VII";  Is_size = 3; break;
  case 8: Is = "VIII"; Is_size = 4; break;
  case 9: Is = "IX";   Is_size = 2; break;
  }

  size_t str_len = M.quot + Cs_size + Xs_size + Is_size;
  char* str = calloc(str_len + 1, 1);

  size_t p = 0;
  memcpy(&str[p], Ms, M.quot);
  p += M.quot;

  memcpy(&str[p], Cs, Cs_size);
  p += Cs_size;

  memcpy(&str[p], Xs, Xs_size);
  p += Xs_size;

  memcpy(&str[p], Is, Is_size);
  p += Is_size;

  p++; str[p] = '\0';

  return str;
}

static inline const char *get_month_str(const struct Date date) {
  assert(date.month <= 11 && date.month >= 0);
  static const char *month_strs[] = {"Ianuarius", "Februarius", "Martius",
                                     "Aprilis",   "Maius",      "Iunius",
                                     "Iulius",    "Augustus",   "September",
                                     "October",   "November",   "December"};
  return month_strs[date.month];
}

static inline uint32_t get_days_in_month(const struct Date date) {
  assert(date.month <= 11 && date.month >= 0);
  static const uint32_t month_lngs[] = {31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};
  return month_lngs[date.month];
}

static inline void increment_date(struct Date* date) {
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
  assert(speed >= 0);
  switch (speed) {
  case 0:
    return 0;
  default:
    return maxi(500 - 50 * speed, 50);
  }
}

const char *get_year_str(const struct Date* date) {
  // TODO: Implement time system for the consuls
  // Need to store some consuls names and generate the sequences dynamically
  return "Year of CON III Grassius & CON I Octavian";
}

const char *get_season_str(const struct Date* date) {
  switch (date->month) {
  case 0: return WINTER;
  case 1: return WINTER;
  case 2: return WINTER;
  case 3: return SPRING;
  case 4: return SPRING;
  case 5: return SUMMER;
  case 6: return SUMMER;
  case 7: return SUMMER;
  case 8: return SUMMER;
  case 9: return AUTUMN;
  case 10: return AUTUMN;
  case 11: return WINTER;
  case 12: return WINTER;
  }
  return "";
}

static inline bool is_winter(const struct Date* date) {
  return strncmp(WINTER, get_season_str(date), strlen(WINTER)) == 0;
}

struct Construction {
  char* name_str;
  char* description_str;
  uint32_t cost;
  uint32_t maintenance;
  struct Effect *effect;
};

struct City {
  char *name;
  int32_t food_production;
  int32_t food_usage;       // Higher usage than production means importation
  int32_t gold;             // Pounds of gold (kg??) (negative, debt??)
  int32_t gold_usage;       // Income / Lose
  /// Capacity
  uint32_t political_capacity;
  uint32_t political_usage;
  uint32_t diplomatic_capacity;
  uint32_t diplomatic_usage;
  uint32_t military_capacity;
  uint32_t military_usage;
  /// Demographics
  uint32_t population;
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
  // Construction projects available
  struct Construction *construction_projects;
  size_t num_construction_projects;
  // Constructions raised and still standing
  struct Construction *constructions;
  size_t num_constructions;
};

#define FOREVER -1
// TODO: When human readable string are null the effect is hidden?
struct Effect {
  char *name_str;         // Human readable name of the effect
  char *description_str;  // Human readable description of the effect
  int64_t duration; // Negative for forever, 0 = done/inactive
  void (*tick_effect)(const struct City* c, struct City* c1);
};

/// Apply and deal with the effects in place on the city
void simulate_next_timestep(const struct City *c, struct City* c1) {
  memset(c1, 0, sizeof(struct City)); // Reset next state

  c1->name = c->name;
  c1->num_effects = c->num_effects;
  c1->effects = c->effects;
  c1->num_construction_projects = c->num_construction_projects;
  c1->construction_projects = c->construction_projects;
  c1->num_constructions = c->num_constructions;
  c1->constructions = c->constructions;

  for (size_t i = 0; i < c1->num_effects; i++) {
    c1->effects[i].tick_effect(c, c1);
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

  for (size_t i = 0; i < c1->num_constructions; i++) {
    c1->constructions[i].effect->tick_effect(c, c1);
  }

  // Compute changes based on current state
  c1->gold = c->gold - c->gold_usage;

  timestep++;
  increment_date(&date);
}

void pops_population_tick_effect(const struct City* c, struct City* c1) {
  assert(c); assert(c1);
  // TODO: Formula for population growth depending on food/etc in Roman times
  // TODO: Uniform random value:
  // https://codereview.stackexchange.com/questions/159604/uniform-random-numbers-in-an-integer-interval-in-c
  c1->birthrate = c->birthrate;
  c1->deathrate = c->deathrate;
  c1->immigrationrate = c->immigrationrate;
  c1->emmigrationrate = c->emmigrationrate;

  // BEID model
  c1->immigrations = c->immigrationrate * c->population;
  c1->births = c->birthrate * c->population;
  c1->deaths = c->deathrate * c->population;
  c1->emmigrations = c->emmigrationrate * c->population;
  c1->population = c->population + c->births + c->immigrations - c->deaths - c->emmigrations;
}

void building_maintenance_tick_effect(const struct City* c, struct City* c1) {
  assert(c); assert(c1);
  for (size_t i = 0; i < c->num_constructions; i++) {
    c1->gold_usage += c->constructions[i].maintenance;
  }
}

void farm_tick_effect(const struct City* c, struct City* c1) {
  assert(c); assert(c1);
  if (is_winter(&date)) { return; }
  c1->food_production += 1;
}

void pops_eating_tick_effect(const struct City* c, struct City* c1) {
  assert(c); assert(c1);
  const uint32_t consumption = c->population * 0.001f;
  c1->food_usage += consumption;
  mvclrprintw(root, 0, 30, "%u", consumption);
}

void colonia_capacity_tick_effect(const struct City* c, struct City* c1) {
  static uint8_t lvl = 1;
  c1->political_capacity += 1;
}

// TODO: Expand this to a decision tree and then a story in which you either
// fail or get the choice of moving the capital of the Roman Empire to your city
// or move to Rome and then the game turns in a slow downward turn into decline?
// Fun?
void imperator_demands_money(const struct City* c, struct City* c1) {
  if (timestep % 100 != 0) {
    return;
  }
  return;
  const float x = rand() / (float)RAND_MAX;
  if (x < 1.0f) {
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
        c1->gold -= 50;
        break;
      }
      if (ch == 'n') {
        mvwprintw(win, 1, 1,
                  "Emperor <EMPEROR-NAME> recalls some of <LEGIO-NAME> from "
                  "<CITY-NAME>");
        c1->population -= 50;
        break;
      }
    }
    destroy_win(win);
  }
}

bool build_construction(struct City *c, struct Construction *cp) {
  if (c->gold <= 0) {
    return false;
  }
  if ((uint32_t) c->gold >= cp->cost) {
    c->gold -= cp->cost;
    c->effects[c->num_effects++] = *cp->effect;
    return true;
  }
  return false;
}

void construction_menu(struct City *c) {
  const int h = 10;
  const int w = 40;
  WINDOW *win = create_newwin(h, w, LINES / 2 - h / 2, COLS / 2 - w / 2);
  keypad(win, true); /* For keyboard arrows 	*/
  mvwprintw(win, 1, 1, "[Q] \t Constructions \n");

  uint32_t selector = 0;
  bool done = false;
  while (!done) {
    uint32_t offset = 0;
    uint32_t s = 2;
    for (size_t i = 0; i < c->num_construction_projects; i++) {
      mvclrprintw(win, s + i + offset, 1, "%s - cost %u gold",
                  c->construction_projects[i].name_str,
                  c->construction_projects[i].cost);
      if (i == selector) {
        offset = 1;
        mvclrprintw(win, s + i + offset, 1, "%s",
                    c->construction_projects[i].description_str);
      }
    }

    switch (wgetch(win)) {
    case C_KEY_DOWN:
      if (selector >= c->num_construction_projects - 1) {
        break;
      }
      selector++;
      break;
    case C_KEY_UP:
      if (selector <= 0) {
        break;
      }
      selector--;
      break;
    case C_KEY_ENTER:
      build_construction(c, &c->construction_projects[selector]);
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
  // TODO: Implement policy menu
}

void help_menu(struct City *c) {
  // TODO: Implement help menu
}

/// Display vital numbers in the user interface
void update_ui(const struct City *c) {
  assert(c);
  int row = 0;
  mvclrprintw(root, row++, 0, "%s", c->name);
  mvclrprintw(root, row++, 0, "%s, day %s of %s, %s",
              get_year_str(&date), roman_numeral_str(date.day + 1), get_month_str(date),
              get_season_str(&date));

  row += 1;
  mvclrprintw(root, row++, 0, "EFFECTS");
  for (size_t i = 0; i < c->num_effects; i++) {
    if (c->effects[i].name_str) {
      mvclrprintw(root, row++, 0, "%s, %s", c->effects[i].name_str, c->effects[i].description_str);
    }
  }

  row += 1;
  mvclrprintw(root, row++, 0, "CONSTRUCTIONS");
  for (size_t i = 0; i < c->num_constructions; i++) {
    mvclrprintw(root, row++, 0, "%s, %s", c->constructions[i].name_str, c->constructions[i].description_str);
  }

  row += 1;
  mvclrprintw(root, row++, 0, "RESOURCES");
  mvclrprintw(root, row++, 0, "Gold (change %i): %i kg", c->gold_usage, c->gold);
  mvclrprintw(root, row++, 0, "Food: %i kcal", c->food_production - c->food_usage);

  row += 1;
  mvclrprintw(root, row++, 0, "Political  power: %u / %u", c->political_usage, c->political_capacity);
  mvclrprintw(root, row++, 0, "Military   power: %u / %u", c->military_usage, c->military_capacity);
  mvclrprintw(root, row++, 0, "Diplomatic power: %u / %u", c->diplomatic_usage, c->diplomatic_capacity);

  row += 1;
  mvclrprintw(root, row++, 0, "DEMOGRAPHICS");
  mvclrprintw(root, row++, 0, "Population: %u", c->population);
  // TODO: Subtotal - military abled man X (of which Y are available due to various reasons)
  mvclrprintw(root, row++, 0, "Births: %i", (int)c->births);
  mvclrprintw(root, row++, 0, "Deaths: %i", (int)c->deaths);
  mvclrprintw(root, row++, 0, "Immigrations: %i", (int)c->immigrations);
  mvclrprintw(root, row++, 0, "Emmigrations: %i", (int)c->emmigrations);

  // TODO: Implement controls and menu system
  mvclrprintw(root, LINES - 1, 0,
              "Speed: %u / 9 | Q: menu | C: construction | P: policy | H: help",
              simulation_speed);
  // TODO: Show drastically declining (good) numbers in reddish, yellowish for
  // stable values, green for increasing (good) values and vice versa for bad
  // values like deaths
  // TODO: Show deltas for the month next to the current values
}

int main() {
  srand(time(NULL));
  root = initscr(); /* initialize the curses library */

  // TODO: Add help flag with descriptions
  // TODO: Hard mode = Everything is in Latin with Roman measurements. Enjoy.

  cbreak();             /* Line buffering disabled pass on everything to me*/
  keypad(stdscr, true); /* For keyboard arrows 	*/
  noecho();             /* Do not echo out input */
  nodelay(root, true);  /* Make getch non-blocking */
  refresh();

  // Default city
  struct City* cities = (struct City*) calloc(2, sizeof(struct City));

  struct City* city = &cities[0];
  city->name = "Eboracum";
  city->gold = 100;
  city->population = 300;
  city->birthrate = 0.015f;
  city->deathrate = 0.014f;
  city->emmigrationrate = 0.0005f;
  city->immigrationrate = 0.0020f;

  struct Effect aqueduct_construction_effect;

  struct Construction aqueduct = {.name_str = "Aqueduct",
                                  .description_str = "Provides fresh water for the city. Required for baths among other things.",
                                  .cost = 25,
                                  .maintenance = 10,
                                  .effect = &aqueduct_construction_effect};

  struct Effect farm_construction_effect = {.name_str = "Farm",
                                            .description_str = "Piece of land that produces food.",
                                            .duration = FOREVER};
  farm_construction_effect.tick_effect = farm_tick_effect;

  struct Construction farm = {.cost = 2,
                              .maintenance = 0,
                              .name_str = "Farm",
                              .description_str = "Piece of land that produces food.",
                              .effect = &farm_construction_effect};

  // TODO: Coin mint - gold revenue
  // TODO: Farms dont produce food in the winter - need to import and thus decrease gold
  // TODO: Different farms (export fruits/etc to other parts of the empire)
  // TODO: Select export/domestic consumption for each farm
  // TODO: Land area limited - increased by political power expendicture via events
  // TODO: Farms should have areas and thus dependent on area for production output
  // TODO: Farms can have different crops: wheat, oats, rye, wine!
  // TODO: Bakeries & Grinding mills
  // TODO: Diary productions - oxygala (ancient form of yoghurt),
  // TODO: Send lobbyists to Rome to argue for different laws (lex), or even vote in plebiscites?
  // Ex) Lex Canuleia ()
  // TODO: Denarius instead of gold
  // TODO: Publicans (tax auction for tax collectors)

  city->num_construction_projects = 2;
  city->construction_projects = calloc(city->num_construction_projects,
                                      sizeof(struct Construction));

  city->construction_projects[0] = aqueduct;
  city->construction_projects[1] = farm;

  city->num_constructions = 2;
  city->constructions = calloc(city->num_constructions, sizeof(struct Construction));
  memcpy(&city->constructions[0], &farm, sizeof(struct Construction));
  memcpy(&city->constructions[1], &farm, sizeof(struct Construction));
  // TODO: Basilica gives one more political power!

  struct Effect pops_food_eating = {.duration = FOREVER};
  pops_food_eating.tick_effect = pops_eating_tick_effect;

  struct Effect pops_simulation = {.duration = FOREVER};
  pops_simulation.tick_effect = pops_population_tick_effect;

  struct Effect emperor_gold_demands = {.duration = FOREVER};
  emperor_gold_demands.tick_effect = imperator_demands_money;

  struct Effect building_maintenance = {.duration = FOREVER};
  building_maintenance.tick_effect = building_maintenance_tick_effect;

  struct Effect colonia_capacity = {.duration = FOREVER};
  colonia_capacity.name_str = "Colonia organisation";
  colonia_capacity.description_str = "Established colonia.";
  colonia_capacity.tick_effect = colonia_capacity_tick_effect;

  city->num_effects = 5;
  city->effects = calloc(city->num_effects, sizeof(struct Effect));

  city->effects[0] = pops_simulation;
  city->effects[1] = pops_food_eating;
  city->effects[2] = emperor_gold_demands;
  city->effects[3] = building_maintenance;
  city->effects[4] = colonia_capacity;

  bool quit = false;
  char ch = 0;

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

    // TODO: Mnemionc keybindings (E for effects, D for Dempgraphics, H for help, C counstruction, P policy, S for summary (main screen))
    // Input
    ch = getch();
    switch (ch) {
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

    update_ui(&cities[cidx]);
  }
  return 0;
}
