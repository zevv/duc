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

#include "cmd.h"
#include "duc.h"

#ifdef HAVE_LIBNCURSES
#include <ncurses.h>
#endif
#ifdef HAVE_LIBNCURSESW
#include <ncursesw/ncurses.h>
#endif

static int opt_classify = 1;
static int opt_apparent = 0;
static int opt_bytes = 0;
static int opt_graph = 1;
static char *opt_database = NULL;


static int cols = 80;
static int rows = 25;


static duc_dir *do_dir(duc_dir *dir, int depth)
{
	int top = 0;
	int cur = 0;

	for(;;) {
	
		duc_size_type st = opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;

		/* Iterate all dirents to find largest size */

		duc_dir_seek(dir, 0);

		off_t size_max = 1;
		struct duc_dirent *e;
		while( (e = duc_dir_read(dir, st)) != NULL) {
			off_t size = opt_apparent ? e->size.apparent : e->size.actual;
			if(size > size_max) size_max = size;
		}

		int count = duc_dir_get_count(dir);
		int pgsize = rows - 2;
		
		/* Check boundaries */

		if(cur < 0) cur = 0;
		if(cur > count - 1) cur = count - 1;
		if(cur < top) top = cur;
		if(cur > top + pgsize - 1) top = cur - pgsize + 1;
		if(top < 0) top = 0;


		/* Draw header */

		char *path = duc_dir_get_path(dir);
		attrset(A_REVERSE);
		mvhline(0, 0, ' ', cols);
		mvprintw(0, 1, " %s ", path);
		attrset(0);
		free(path);


		/* Draw footer */

		struct duc_size size;
		duc_dir_get_size(dir, &size);
		char siz[16], cnt[16];
		duc_human_size(&size, st, opt_bytes, siz, sizeof siz);
		duc_human_number(count, opt_bytes, cnt, sizeof cnt);
		attrset(A_REVERSE);
		mvhline(rows-1, 0, ' ', cols);
		mvprintw(rows-1, 0, " Total %sB in %s files/directories", siz, cnt);
		attrset(0);

		/* Draw dirents */
	
		duc_dir_seek(dir, top);

		int i;
		int y = 1;

		for(i=top; i<top + pgsize; i++) {
			
			struct duc_dirent *e = duc_dir_read(dir, st);

			attrset(cur == i ? A_REVERSE : 0);
			mvhline(y, 0, ' ', cols);

			if(e) {

				off_t size = opt_apparent ? e->size.apparent : e->size.actual;
		
				size_t max_size_len = opt_bytes ? 12 : 7;

				char class = ' ';
				if(opt_classify)
					class = duc_file_type_char(e->type);

				char siz[16];
				duc_human_size(&e->size, st, opt_bytes, siz, sizeof siz);
				printw("%*s", max_size_len, siz);

				if(cur != i) attron(A_BOLD);
				printw(" ");
				char *p = e->name;
				while(*p) {
					if(*(uint8_t *)p >= 32) {
						printw("%c", *p);
					} else {
						printw("^%c", *p+64);
					}
					p++;
				}
				printw("%c", class);
				attroff(A_BOLD);

				int w = cols - 30;
				if(w > cols / 2) w = cols / 2;
				if(opt_graph && w > 2) {
					int j;
					off_t g = w * size / size_max;
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
			case KEY_UP: cur--; break;
			case KEY_DOWN: cur++; break;
			case KEY_PPAGE: cur -= pgsize; break;
			case KEY_NPAGE: cur += pgsize; break;
			case KEY_RESIZE: getmaxyx(stdscr, rows, cols); break;
			case 'a': opt_apparent ^= 1; break;
			case 'b': opt_bytes ^= 1; break;
			case 'c': opt_classify ^= 1; break;
			case 'g': opt_graph ^= 1; break;

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
				  struct duc_dirent *e = duc_dir_read(dir, st);
				  if(e->type == DUC_FILE_TYPE_DIR) {
					dir2 = duc_dir_openent(dir, e);
					if(dir2) {
						do_dir(dir2, depth + 1);
						duc_dir_close(dir2);
					}
				  }
				  break;
		}

	}

	return NULL;
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
	use_default_colors();
	getmaxyx(stdscr, rows, cols);

	do_dir(dir, 0);

	duc_dir_close(dir);
	duc_close(duc);

	return 0;
}

static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "show apparent instead of actual file size" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ NULL }
};


struct cmd cmd_ui = {
	.name = "ui",
	.descr_short = "Interactive ncurses user interface",
	.usage = "[options] [PATH]",
	.main = ui_main,
	.options = options,
	.descr_long = 
		"The 'gui' subcommand queries the duc database and runs an interactive ncurses\n"
		"utility for exploring the disk usage of the given path. If no path is given the\n"
		"current working directory is explored.\n"
		"\n"
		"The following keys can be used to navigate and alter the file system:\n"
		"\n"
		"    k, up, pgup:     move cursor up\n"
		"    j, down, pgdn:   move cursor down\n"
		"    h, left:         go up to parent directory (..)\n"
		"    l, right, enter: descent into selected directory\n"
		"    a:               toggle between actual and apparent disk usage\n"
		"    b:               toggle between exact and abbreviated sizes\n"
		"    g:               toggle graph\n"
		"    q, escape:       quit\n"

};

#endif

/*
 * End
 */

