/*  joyterm, a joypad-operable terminal emulator
    Copyright (C) 2020 Lauri Kasanen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <ctype.h>
#include <fcntl.h>
#include <ncurses.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <rote/rote.h>

#include <linux/input.h>

#include "compat.h"

#define DEV "/dev/input/event0"

#define BORDER 2

/*
1234567890123456789012345678901234567890
__ L=tab       1234567890  !"#$%<     __
__ R=shift     qwertyuiop  &/()=>     __
__ Z,Cr=enter  asdfghjkl   ?{}[]|     __
__ Cl=space    zxcvbnm     ,.+-`      __
1234567890123456789012345678901234567890
*/

#define NUMTILES 59
#define XBASE 13

#define X(a) XBASE + a
static const unsigned char xpos[NUMTILES] = {
	X(0), X(1), X(2), X(3), X(4), X(5), X(6), X(7), X(8), X(9),
	X(12), X(13), X(14), X(15), X(16), X(17),

	X(0), X(1), X(2), X(3), X(4), X(5), X(6), X(7), X(8), X(9),
	X(12), X(13), X(14), X(15), X(16), X(17),

	X(0), X(1), X(2), X(3), X(4), X(5), X(6), X(7), X(8),
	X(12), X(13), X(14), X(15), X(16), X(17),

	X(0), X(1), X(2), X(3), X(4), X(5), X(6),
	X(12), X(13), X(14), X(15), X(16),
};
#undef X

static const unsigned char ypos[NUMTILES] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,

	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1,

	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2,

	3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3,
};

static const char tile[NUMTILES] = {
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'!', '\"', '#', '$', '%', '<',

	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
	'&', '/', '(', ')', '=', '>',

	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
	'?', '{', '}', '[', ']', '|',

	'z', 'x', 'c', 'v', 'b', 'n', 'm',
	',', '.', '+', '-', '`',
};

static void outerbox(const unsigned h, const unsigned w, const unsigned y, const unsigned x) {
	// Default ncurses box is inner, draw an outer box on the stdscr

	mvvline(y, x - 1, ACS_VLINE, h);
	mvvline(y, x + w, ACS_VLINE, h);

	mvhline(y - 1, x, ACS_HLINE, w);
	mvhline(y + h, x, ACS_HLINE, w);

	mvaddch(y - 1, x - 1, ACS_ULCORNER);
	mvaddch(y - 1, x + w, ACS_URCORNER);
	mvaddch(y + h, x - 1, ACS_LLCORNER);
	mvaddch(y + h, x + w, ACS_LRCORNER);

	refresh();
}

static void drawkbd(WINDOW *w, const unsigned shifted) {
	werase(w);

	mvwprintw(w, 0, 1, "L=tab       ");
	wattron(w, COLOR_PAIR(5));
	wprintw(w, "1234567890  ");
	wattroff(w, COLOR_PAIR(5));
	wattron(w, COLOR_PAIR(1));
	wprintw(w, "!\"#$%<");
	wattroff(w, COLOR_PAIR(1));

	mvwprintw(w, 3, 1, "Cl=space");
	mvwprintw(w, 1, 1, "R=shift     ");
	wattron(w, COLOR_PAIR(4));

	if (!shifted) {
		wprintw(w, "qwertyuiop  ");
		wattroff(w, COLOR_PAIR(4));

		wattron(w, COLOR_PAIR(1));
		wprintw(w, "&/()=>");
		wattroff(w, COLOR_PAIR(1));

		mvwprintw(w, 2, 1, "Z,Cr=enter  ");
		wattron(w, COLOR_PAIR(4));
		wprintw(w, "asdfghjkl   ");
		wattroff(w, COLOR_PAIR(4));

		wattron(w, COLOR_PAIR(1));
		wprintw(w, "?{}[]|");
		wattroff(w, COLOR_PAIR(1));

		wattron(w, COLOR_PAIR(4));
		mvwprintw(w, 3, 13, "zxcvbnm     ");
	} else {
		wprintw(w, "QWERTYUIOP  ");
		wattroff(w, COLOR_PAIR(4));

		wattron(w, COLOR_PAIR(1));
		wprintw(w, "&/()=>");
		wattroff(w, COLOR_PAIR(1));

		mvwprintw(w, 2, 1, "Z,Cr=enter  ");
		wattron(w, COLOR_PAIR(4));
		wprintw(w, "ASDFGHJKL   ");
		wattroff(w, COLOR_PAIR(4));

		wattron(w, COLOR_PAIR(1));
		wprintw(w, "?{}[]|");
		wattroff(w, COLOR_PAIR(1));

		wattron(w, COLOR_PAIR(4));
		mvwprintw(w, 3, 13, "ZXCVBNM     ");
	}

	wattroff(w, COLOR_PAIR(4));

	wattron(w, COLOR_PAIR(1));
	wprintw(w, ",.+-`");
	wattroff(w, COLOR_PAIR(1));

	wrefresh(w);
}

int main() {

	WINDOW *top, *bot;
	RoteTerm *rt;
	int fd;
	struct pollfd poller;
	struct input_event ev[64];
	unsigned cur = 0, quit = 0, shifted = 0;

	fd = open(DEV, O_RDONLY);
	if (fd < 0) {
		printf("Couldn't open %s\n", DEV);
		return 1;
	}

	poller.fd = fd;
	poller.events = POLLIN;

	initscr();
	raw();
	noecho();
	start_color();
	keypad(stdscr, TRUE);

	unsigned i, j;
	// rote likes the colors specifically
	for (i = 0; i < 8; i++) for (j = 0; j < 8; j++)
		if (i != 7 || j != 0)
			init_pair(j*8+7-i, i, j);

	refresh();

	#define mywin(win, h, w, y, x) outerbox(h, w, y, x); win = newwin(h, w, y, x)

	const unsigned termh = LINES - BORDER * 2 - 1 - 4;
	const unsigned termw = COLS - BORDER * 2;

	rt = rote_vt_create(termh, termw);
	rote_vt_forkpty(rt, "sh");

	mywin(top, termh, termw, BORDER, BORDER);
	mywin(bot, 4, COLS - BORDER * 2,
		BORDER + (LINES - BORDER * 2 - 1 - 4) + 1,
		BORDER);

	mvaddch(BORDER + (LINES - BORDER * 2 - 1 - 4), BORDER - 1, ACS_LTEE);
	mvaddch(BORDER + (LINES - BORDER * 2 - 1 - 4), COLS - BORDER, ACS_RTEE);

	#undef mywin

	refresh();

	drawkbd(bot, 0);

	#define UPDCURSOR() wmove(bot, ypos[cur], xpos[cur]); wrefresh(bot)

	UPDCURSOR();

	while (!quit) {
		if (poll(&poller, 1, 16) == 1) {
			int rd = read(fd, ev, sizeof(ev));
			if (rd < (int) sizeof(struct input_event)) {
				break; // error
			}

			unsigned i;
			rd /= sizeof(struct input_event);
			for (i = 0; i < (unsigned) rd; i++) {
				if (ev[i].type != EV_KEY)
					continue;
				if (!ev[i].value)
					continue; // Only presses

				const unsigned c = ev[i].code;
				if (c == KEY_Q) {
					quit = 1;
				} else if (c == BTN_DPAD_LEFT || c == KEY_LEFT) {
					if (cur)
						cur--;
					else
						cur = NUMTILES - 1;
					UPDCURSOR();
				} else if (c == BTN_DPAD_RIGHT || c == KEY_RIGHT) {
					cur++;
					if (cur >= NUMTILES)
						cur = 0;
					UPDCURSOR();
				} else if (c == BTN_DPAD_UP || c == KEY_UP) {
					if (ypos[cur]) {
						const unsigned char prevx = xpos[cur];
						cur--;
						while (xpos[cur] != prevx)
							cur--;
						UPDCURSOR();
					}
				} else if (c == BTN_DPAD_DOWN || c == KEY_DOWN) {
					if (ypos[cur] < 3) {
						const unsigned char prevx = xpos[cur];
						const unsigned char prevy = ypos[cur];
						cur++;
						while (ypos[cur] == prevy)
							cur++;
						while (xpos[cur] < prevx && cur < NUMTILES - 1)
							cur++;
						if (xpos[cur] > prevx)
							cur--;
						UPDCURSOR();
					}
				} else if (c == BTN_3 /* r */ || c == KEY_LEFTSHIFT) {
					shifted ^= 1;
					drawkbd(bot, shifted);
					UPDCURSOR();
				} else if (c == BTN_Z || c == BTN_RIGHT /* c-right */ ||
						c == KEY_ENTER) {
					rote_vt_keypress(rt, '\r');
				} else if (c == BTN_2 /* l */ || c == KEY_TAB) {
					rote_vt_keypress(rt, '\t');
				} else if (c == BTN_1 /* b */ || c == KEY_BACKSPACE) {
					rote_vt_keypress(rt, '\b');
				} else if (c == BTN_0 /* a */ || c == KEY_RIGHTSHIFT) {
					char key = tile[cur];
					if (isalpha(key) && shifted)
						key = toupper(key);
					rote_vt_keypress(rt, key);
				} else if (c == BTN_LEFT /* c-left */ ||
						c == KEY_SPACE) {
					rote_vt_keypress(rt, ' ');
				} else if (c == BTN_FORWARD || c == KEY_I) {
					rote_vt_write(rt, "\e[A", strlen("\e[A"));
				} else if (c == BTN_BACK || c == KEY_K) {
					rote_vt_write(rt, "\e[B", strlen("\e[B"));
				}
			}
		}

		rote_vt_draw(rt, top, 0, 0, NULL);
		wrefresh(top);
		UPDCURSOR();
	}

	// For kb testing, flush the input
	nodelay(stdscr, TRUE);
	while (getch() != ERR);

	endwin();
	close(fd);

	return 0;
}
