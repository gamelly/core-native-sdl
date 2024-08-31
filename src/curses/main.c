#include "zeebo_engine.h"
#include "ncurses/include/curses.h"

int curses_main_core(lua_State *L) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(0); 
    refresh();
    endwin();
}
