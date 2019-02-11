#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
  char *name;
  uint32_t population;
} City;

void next_timestep() {
  mvprintw(0, 0, "A");
  mvprintw(10, 10, "B");
  mvprintw(50, 50, "C");
}

void update_ui(const City *c) {
  printw("Hello");
  printw("Population: %u", c->population);
}

int main() {
  srand(time(NULL));
  WINDOW *root = initscr(); /* initialize the curses library */

  raw();                /* Line buffering disabled	*/
  cbreak();             /* Line buffering disabled pass on everything to me*/
  keypad(stdscr, TRUE); /* For keyboard arrows 	*/
  noecho();             /* Do not echo out input */
  nodelay(root, true);  /* Make getch non-blocking */
  refresh();

  //
  wborder(root, '|', '|', '-', '-', '+', '+', '+', '+');
  City city;
  bool quit = false;
  while (!quit) {
    next_timestep();
    update_ui(&city);
  }
  return 0;
}
