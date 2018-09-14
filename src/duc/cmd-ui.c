#include "config.h"

#ifdef ENABLE_UI

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "cmd.h"
#include "duc.h"

#ifdef HAVE_LIBNCURSES
#ifdef HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif
#endif
#ifdef HAVE_LIBNCURSESW
#include <ncursesw/ncurses.h>
#endif

enum {
	PAIR_SIZE = 1,
	PAIR_NAME,
	PAIR_CLASS,
	PAIR_GRAPH,
	PAIR_BAR,
	PAIR_CURSOR,
};

static void help(void);

static bool opt_apparent = false;
static bool opt_count = false;
static bool opt_bytes = false;
static bool opt_graph = true;
static bool opt_nocolor = false;
static bool opt_name_sort = false;
static char *opt_database = NULL;


static int cols = 80;
static int rows = 25;


static duc_dir *do_dir(duc_dir *dir, int depth)
{
	int top = 0;
	int cur = 0;

	for(;;) {

		int attr_size, attr_name, attr_class, attr_graph, attr_bar, attr_cursor;

		if(!opt_nocolor) {
			attr_size = COLOR_PAIR(PAIR_SIZE);
			attr_name = COLOR_PAIR(PAIR_NAME);
			attr_bar = COLOR_PAIR(PAIR_BAR);
			attr_cursor = COLOR_PAIR(PAIR_CURSOR);
			attr_graph = COLOR_PAIR(PAIR_GRAPH);
			attr_class = COLOR_PAIR(PAIR_CLASS);
		} else {
			attr_size = 0;
			attr_name = A_BOLD;
			attr_class = 0;
			attr_graph = 0;
			attr_bar = A_REVERSE;
			attr_cursor = A_REVERSE;
			use_default_colors();
		}
	
		duc_size_type st = opt_count ? DUC_SIZE_TYPE_COUNT : 
				   opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;

		/* Iterate all dirents to find largest size */

		duc_dir_seek(dir, 0);

		off_t size_max = 1;
		int dir_count = 0;
		int file_count = 0;
		struct duc_dirent *e;
		duc_sort sort = opt_name_sort ? DUC_SORT_NAME : DUC_SORT_SIZE;
		while( (e = duc_dir_read(dir, st, sort)) != NULL) {
			if(e->type == DUC_FILE_TYPE_DIR) {
				dir_count ++;
			} else {
				file_count ++;
			}
			off_t size = duc_get_size(&e->size, st);
			if(size > size_max) size_max = size;
		}

		int count = dir_count + file_count;
		int pgsize = rows - 2;
		
		/* Check boundaries */

		if(cur < 0) cur = 0;
		if(cur > count - 1) cur = count - 1;
		if(cur < top) top = cur;
		if(cur > top + pgsize - 1) top = cur - pgsize + 1;
		if(top < 0) top = 0;


		/* Draw header */

		char *path = duc_dir_get_path(dir);
		attrset(attr_bar);
		mvhline(0, 0, ' ', cols);
		mvprintw(0, 1, " %s ", path);
		attrset(0);
		free(path);


		/* Draw footer */

		struct duc_size size;
		duc_dir_get_size(dir, &size);
		char siz[32], fcnt[32], dcnt[32];
		duc_human_size(&size, st, opt_bytes, siz, sizeof siz);
		duc_human_number(file_count, opt_bytes, fcnt, sizeof fcnt);
		duc_human_number(dir_count, opt_bytes, dcnt, sizeof dcnt);
		attrset(attr_bar);
		mvhline(rows-1, 0, ' ', cols);
		mvprintw(rows-1, 0, " Total %sB in %s files and %s directories (", siz, fcnt, dcnt);
		switch(st) {
			case DUC_SIZE_TYPE_APPARENT: printw("apparent size"); break;
			case DUC_SIZE_TYPE_ACTUAL: printw("actual size"); break;
			case DUC_SIZE_TYPE_COUNT: printw("file count"); break;
		}
		printw(")");
		attrset(0);

		/* Draw dirents */
	
		duc_dir_seek(dir, top);

		int i;
		int y = 1;

		for(i=top; i<top + pgsize; i++) {
			
			struct duc_dirent *e = duc_dir_read(dir, st, sort);

			attrset(cur == i ? attr_cursor : 0);
			mvhline(y, 0, ' ', cols);

			if(e) {

				off_t size = duc_get_size(&e->size, st);;
		
				size_t max_size_len = opt_bytes ? 12 : 7;

				char class = duc_file_type_char(e->type);

				char siz[32];
				duc_human_size(&e->size, st, opt_bytes, siz, sizeof siz);
				if(cur != i) attrset(attr_size);
				printw("%*s", max_size_len, siz);

				printw(" ");
				char *p = e->name;
				if(cur != i) attrset(attr_name);
				while(*p) {
					if(*(uint8_t *)p >= 32) {
						printw("%c", *p);
					} else {
						printw("^%c", *p+64);
					}
					p++;
				}
				if(cur != i) attrset(attr_class);
				printw("%c", class);

				int w = cols - 30;
				if(w > cols / 2) w = cols / 2;
				if(opt_graph && w > 2) {
					int j;
					off_t g = w * size / size_max;
					if(cur != i) attrset(attr_graph);
					mvprintw(y, cols - w - 4, " [");
					for(j=0; j<w; j++) printw("%s", j < g ? "=" : " ");
					printw("] ");
				}

			} else {

				attrset(A_DIM);
				mvprintw(y, 0, "~");
			}

			y++;
		}

		duc_dir *dir2 = NULL;

		/* Handle key */

		int c = getch();

		switch(c) {
			case 'k':
			case KEY_UP: cur--; break;
			case 'j':
			case KEY_DOWN: cur++; break;
			case 21: cur -= pgsize/2; break;
			case KEY_PPAGE: cur -= pgsize; break;
			case 4: cur += pgsize/2; break;
			case KEY_NPAGE: cur += pgsize; break;
			case KEY_RESIZE: getmaxyx(stdscr, rows, cols); break;
			case '0': cur = 0; break;
			case KEY_HOME: cur = 0; break;
			case '$': cur = count-1; break;
			case KEY_END: cur = count-1; break;
			case 'a': opt_apparent ^= 1; break;
			case 'b': opt_bytes ^= 1; break;
			case 'C': opt_nocolor ^= 1; break;
			case 'c': opt_count ^= 1; break;
			case 'g': opt_graph ^= 1; break;
			case 'h': help(); break;
			case '?': help(); break;
			case 'n': opt_name_sort ^= 1; break;

			case 27:
			case 'q': 
				  exit(0); break;

			case KEY_BACKSPACE:
			case KEY_LEFT:
				  if(depth > 0) {
					  return NULL;
				  } else {
					  dir2 = duc_dir_openat(dir, "..");
					  if(dir2) {
						  do_dir(dir2, 0);
						  duc_dir_close(dir2);
					  }
				  }
				  break;

			case KEY_RIGHT:
			case '\r':
			case '\n': 
				  duc_dir_seek(dir, cur);
				  e = duc_dir_read(dir, st, sort);
				  if(e && e->type == DUC_FILE_TYPE_DIR) {
					dir2 = duc_dir_openent(dir, e);
					if(dir2) {
						do_dir(dir2, depth + 1);
						duc_dir_close(dir2);
					}
				  }
				  break;
			case 'o':
				  duc_dir_seek(dir, cur);
				  e = duc_dir_read(dir, st, sort);
				  if(e && e->type == DUC_FILE_TYPE_REG) {
					  char cmd[DUC_PATH_MAX] = "true";
					  char *path = duc_dir_get_path(dir);
					  if(path) {
						  snprintf(cmd, sizeof(cmd), "xdg-open \"%s/%s\"", path, e->name);
						  free(path);
					  }
					  endwin();
					  system(cmd);
					  doupdate();
				  }
				  break;
			default:
				  break;
		}

	}
}


static void bye(void)
{
	endwin();
}


static int ui_main(duc *duc, int argc, char **argv)
{
	char *path = ".";
	if(argc > 0) path = argv[0];


	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	setlocale(LC_CTYPE, "");
	initscr();
	atexit(bye);
	cbreak();
	noecho();
	nonl();
	keypad(stdscr, 1);
	//halfdelay(1);
	curs_set(0);
	start_color();
	getmaxyx(stdscr, rows, cols);

	init_pair(PAIR_SIZE, COLOR_WHITE, COLOR_BLACK);
	init_pair(PAIR_NAME, COLOR_GREEN, COLOR_BLACK);
	init_pair(PAIR_CLASS, COLOR_YELLOW, COLOR_BLACK);
	init_pair(PAIR_GRAPH, COLOR_CYAN, COLOR_BLACK);
	init_pair(PAIR_BAR, COLOR_WHITE, COLOR_BLUE);
	init_pair(PAIR_CURSOR, COLOR_BLACK, COLOR_CYAN);

	do_dir(dir, 0);

	duc_dir_close(dir);
	duc_close(duc);

	return 0;
}

static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "show apparent instead of actual file size" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_count,     "count",      0,  DUCRC_TYPE_BOOL,   "show number of files instead of file size" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_name_sort, "name-sort", 'n', DUCRC_TYPE_BOOL,   "sort output by name instead of by size" },
	{ &opt_nocolor,   "no-color",   0 , DUCRC_TYPE_BOOL,   "do not use colors on terminal output" },
	{ NULL }
};


struct cmd cmd_ui = {
	.name = "ui",
	.descr_short = "Interactive ncurses user interface",
	.usage = "[options] [PATH]",
	.main = ui_main,
	.options = options,
	.descr_long = 
		"The 'ui' subcommand queries the duc database and runs an interactive ncurses\n"
		"utility for exploring the disk usage of the given path. If no path is given the\n"
		"current working directory is explored.\n"
		"\n"
		"The following keys can be used to navigate and alter the file system:\n"
		"\n"
		"    up, pgup, j:     move cursor up\n"
		"    down, pgdn, k:   move cursor down\n"
		"    home, 0:         move cursor to top\n"
		"    end, $:          move cursor to bottom\n"
		"    left, backspace: go up to parent directory (..)\n"
		"    right, enter:    descent into selected directory\n"
		"    a:               toggle between actual and apparent disk usage\n"
		"    b:               toggle between exact and abbreviated sizes\n"
		"    c:               Toggle between file size and file count\n"
		"    h:               show help. press 'q' to return to the main screen\n"
		"    n:               toggle sort order between 'size' and 'name'\n"
		"    o:               try to open the file using xdg-open\n"
		"    q, escape:       quit\n"

};


static void help(void)
{
	attrset(0);
	refresh();

	FILE *f = popen("clear; less -", "w");
	fputs(cmd_ui.descr_long, f);
	fclose(f);

	erase();
	redrawwin(stdscr);
	touchwin(stdscr);
	refresh();
}

#endif

/*
 * End
 */

