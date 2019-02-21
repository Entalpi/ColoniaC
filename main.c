#include <assert.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

// TODO: Add an event log
// Event: "Our scouts report that barbarians are gathering under the cmd of
// <BARBARIAN-CHIEF-NAME>" Housing? Water? Aqueducts and baths? Diseases? Food
// spoiling? Food production should vary with seasons
// Tax collection should be simulated by trying to model how they collected
// taxes in the early Roman Empire.

// TODO: Refactor into math.h?
static inline float maxf(const float a, const float b) { return a < b ? b : a; }

static inline float maxi(const uint32_t a, const uint32_t b) {
  return a < b ? b : a;
}

static WINDOW *root = NULL; // FIXME: ...

/***** ncurses utility functions *****/
/// MVove, CLearR, PRINT Word
static inline void mvclrprintw(WINDOW *win, int y, int x, const char *fmt,
                               ...) {
  move(y, x);
  clrtoeol();
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

const char *get_year_str(const uint64_t timestep) {
  // TODO: Implement time system
  // Need to store some consuls names and generate the sequences dynamically
  return "Year of CON III Grassius & CON I Octavian";
}

const char *get_season_str(const uint64_t timestep) {
  // TODO: Implement
  return "winter";
}

struct City {
  char *name;
  uint32_t population;
  //
  int32_t food; // TODO: Calories? Negative means starvation?
  //
  int32_t gold; // Pounds of gold (kg??) (negative, debt?? )
  //
  float birthrate;       // Births / 1'000 pops / timestep
  float births;          // Accumulated from previous timesteps
  float deathrate;       // Deaths / 1'000 pops / timestep
  float deaths;          // Accumulated from previous timesteps
  float immigrationrate; // Immigration in # pops / 1'000 pops / timestep
  float immigrations;    // Accumulated from previous timesteps
  float emmigrationrate; // Emmigration in # pops / 1'000 pops / timestep
  float emmigrations;    // Accumulated from previous timesteps
  //
  // TODO: Ensure order independence of the execution of effects
  struct Effect *effects;
  size_t num_effects;
};

#define FOREVER -1
struct Effect {
  void *resrc;      // Ptr to the resource affected
  int64_t duration; // Negative for forever
  void (*tick_effect)(struct Effect *effect);
};

void farm_tick_effect(struct Effect *effect) {
  assert(effect);
  assert(effect->resrc);
  // TODO: Formula for typical farm output in number of calories around in Roman
  // times
  if (timestep % 2 == 0) {
    *(uint32_t *)(effect->resrc) += 1;
  }
}

void pops_eating_tick_effect(struct Effect *effect) {
  assert(effect);
  assert(effect->resrc);
  struct City *c = (struct City *)effect->resrc;
  // TODO: Formula for typical human consumption counted in calories in Roman
  // times
  const uint32_t consumption = c->population * 0.0001f;
  c->food -= consumption;
}

void pops_population_tick_effect(struct Effect *effect) {
  assert(effect);
  assert(effect->resrc);
  struct City *c = (struct City *)effect->resrc;
  // TODO: Formula for population growth depending on food/etc in Roman times
  // TODO: Uniform random value:
  // https://codereview.stackexchange.com/questions/159604/uniform-random-numbers-in-an-integer-interval-in-c
  c->births += c->birthrate * c->population;
  c->immigrations += c->immigrationrate * c->population;
  c->deaths += c->deathrate * c->population;
  c->emmigrations += c->emmigrationrate * c->population;
  c->population += c->births + c->immigrations - c->deaths - c->emmigrations;
  mvclrprintw(root, 10, 0, "Births: %f", c->births);
  mvclrprintw(root, 11, 0, "Deaths: %f", c->deaths);
  mvclrprintw(root, 12, 0, "Immigrations: %f", c->immigrations);
  mvclrprintw(root, 13, 0, "Emmigrations: %f", c->emmigrations);
  // Accumulate the remainder to the next timestep
  c->births = maxf(c->births - (int)c->births, 0.0f);
  c->deaths = maxf(c->deaths - (int)c->deaths, 0.0f);
  c->immigrations = maxf(c->immigrations - (int)c->immigrations, 0.0f);
  c->emmigrations = maxf(c->emmigrations - (int)c->emmigrations, 0.0f);
}

// TODO: Expand this to a decision tree and then a story in which you either
// fail or get the choice of moving the capital of the Roman Empire to your city
// or move to Rome and then the game turns in a slow downward turn into decline?
// Fun?
void imperator_demands_money(struct Effect *effect) {
  assert(effect);
  assert(effect->resrc);
  if (timestep % 100 != 0) {
    return;
  }
  struct City *c = (struct City *)effect->resrc;
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
        c->gold -= 50;
        break;
      }
      if (ch == 'n') {
        mvwprintw(win, 1, 1,
                  "Emperor <EMPEROR-NAME> recalls some of <LEGIO-NAME> from "
                  "<CITY-NAME>");
        c->population -= 50;
        break;
      }
    }
    destroy_win(win);
  }
}

/// Apply and deal with the effects in place on the city
void next_timestep(struct City *c) {
  for (size_t i = 0; i < c->num_effects; i++) {
    c->effects[i].tick_effect(&c->effects[i]);
    if (c->effects[i].duration == FOREVER) {
      continue;
    }
    c->effects[i].duration--;
    if (c->effects[i].duration == 0) {
      c->effects[i] = c->effects[c->num_effects - 1];
      c->num_effects--;
      i--;
    }
  }
  timestep++;
}

/// Display vital numbers in the user interface
void update_ui(const struct City *c) {
  assert(c);
  mvclrprintw(root, 0, 0, "Colonia %s", c->name);
  mvclrprintw(root, 1, 0, "%s, day %u, hour %u, %s", get_year_str(timestep),
              timestep / 24, // TODO: Use the Roman calendar
              timestep % 24, get_season_str(timestep));
  mvclrprintw(root, 3, 0, "Population: %u", c->population);
  mvclrprintw(root, 4, 0, "Food: %i kg", c->food);
  mvclrprintw(root, 5, 0, "Gold: %i kg", c->gold);

  // TODO: Implement controls and menu system
  mvclrprintw(
      root, LINES - 1, 0,
      "Speed: %u | Q: menu (quit for now) | C: construction | P: policy",
      simulation_speed);
  // TODO: Show drastically declining (good) numbers in reddish, yellowish for
  // stable values, green for increasing (good) values and vice versa for bad
  // values like deaths
}

int main() {
  srand(time(NULL));
  root = initscr(); /* initialize the curses library */

  // TOOD: Add help flag with descriptions
  // TODO: Hard mode = Everything is in Latin with Roman measurements. Enjoy.

  cbreak();             /* Line buffering disabled pass on everything to me*/
  keypad(stdscr, TRUE); /* For keyboard arrows 	*/
  noecho();             /* Do not echo out input */
  nodelay(root, true);  /* Make getch non-blocking */
  refresh();

  // Default city
  struct City city = {0};
  city.name = "Eboracum";
  city.food = 100;
  city.gold = 100;
  city.population = 5500; // TODO: Several types of people?
  city.birthrate = 0.001f;
  city.deathrate = 0.002f;
  city.emmigrationrate = 0.001f;
  city.immigrationrate = 0.00205f;

  struct Effect farm = {.resrc = &city.food, .duration = FOREVER};
  farm.tick_effect = farm_tick_effect;

  struct Effect food_eating = {.resrc = &city, .duration = FOREVER};
  food_eating.tick_effect = pops_eating_tick_effect;

  struct Effect pops = {.resrc = &city, .duration = FOREVER};
  pops.tick_effect = pops_population_tick_effect;

  struct Effect emperor_gold_demands = {.resrc = &city, .duration = FOREVER};
  emperor_gold_demands.tick_effect = imperator_demands_money;

  city.num_effects = 4;
  city.effects = calloc(city.num_effects, sizeof(struct Effect));

  city.effects[0] = farm;
  city.effects[1] = pops;
  city.effects[2] = food_eating;
  city.effects[3] = emperor_gold_demands;

  bool quit = false;
  char ch = 0;

  struct timeval t0;
  struct timeval t1;
  gettimeofday(&t0, NULL);
  uint64_t dt = 0;

  while (!quit) {
    gettimeofday(&t1, NULL);
    dt = ((t1.tv_sec * 1000) + (t1.tv_usec / 1000)) -
         ((t0.tv_sec * 1000) + (t0.tv_usec / 1000));

    // TODO: Pass the delta rest onto next timestep
    const uint32_t ms_per_timestep = ms_per_timestep_for(simulation_speed);
    if (dt >= ms_per_timestep && ms_per_timestep != 0) {
      next_timestep(&city);
      t0 = t1;
    }

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
    case 'q':
      return 0;
    }

    update_ui(&city);
  }
  return 0;
}
