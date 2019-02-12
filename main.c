#include <assert.h>
#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

static uint32_t timestep = 0;

struct City {
  char *name;
  uint32_t population;
  uint32_t food;
  uint32_t gold;
};

void next_timestep() {
  //
  timestep++;
}

void update_ui(const struct City *c) {
  assert(c);
  mvprintw(0, 0, "%s", c->name);
  mvprintw(2, 0, "%u", timestep);
  mvprintw(4, 0, "Population: %u", c->population);
  mvprintw(6, 0, "Food: %u", c->food);
  mvprintw(8, 0, "Gold: %u", c->gold);
}

int main() {
  srand(time(NULL));
  WINDOW *root = initscr(); /* initialize the curses library */

  cbreak();             /* Line buffering disabled pass on everything to me*/
  keypad(stdscr, TRUE); /* For keyboard arrows 	*/
  noecho();             /* Do not echo out input */
  nodelay(root, true);  /* Make getch non-blocking */
  refresh();

  struct City city = {0};
  bool quit = false;
  char ch = 0;
  uint32_t MS_PER_TIMESTEP = 100;

  struct timeval t0;
  struct timeval t1;
  gettimeofday(&t0, NULL);
  uint64_t dt = 0;

  while (!quit) {
    gettimeofday(&t1, NULL);
    dt = ((t1.tv_sec * 1000) + (t1.tv_usec / 1000)) -
         ((t0.tv_sec * 1000) + (t0.tv_usec / 1000));

    if (dt >= MS_PER_TIMESTEP) {
      next_timestep();
      t0 = t1;
    }

    // Input
    ch = getch();
    switch (ch) {
    case '1':
      MS_PER_TIMESTEP = 100;
      break;
    case '2':
      MS_PER_TIMESTEP = 50;
      break;
    case 'q':
      return 0;
    }

    update_ui(&city);
  }
  return 0;
}
