#include "config.h"

#ifdef ENABLE_X11

#include "duc.h"
#include "duc-graph.h"
#include "ducrc.h"
#include "cmd.h"

#include <stdio.h>
#include <ctype.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <cairo.h>
#include <string.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

static int opt_bytes;
static int opt_dark;
static char *opt_database = NULL;
static char *opt_palette = NULL;
static double opt_fuzz = 0.5;
static int opt_levels = 4;
static int opt_apparent = 0;
static int opt_count = 0;
static int opt_ring_gap = 4;
static int opt_gradient = 0;

static Display *dpy;
static Window rootwin;
static int redraw = 1;
static int tooltip_x = 0;
static int tooltip_y = 0;
static int tooltip_moved = 0;
static struct pollfd pfd;
static enum duc_graph_palette palette = 0;
static int win_w = 600;
static int win_h = 600;
static cairo_surface_t *cs;
static cairo_t *cr;
static duc_dir *dir;
static duc_graph *graph;
static double fuzz;

static void draw(void)
{
	if(opt_levels < 1) opt_levels = 1;
	if(opt_levels > 10) opt_levels = 10;

	duc_size_type st = opt_count ? DUC_SIZE_TYPE_COUNT : 
	                   opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;

	duc_graph_set_size(graph, win_w, win_h);
	duc_graph_set_position(graph, 0, 0);
	duc_graph_set_max_level(graph, opt_levels);
	duc_graph_set_fuzz(graph, fuzz);
	duc_graph_set_palette(graph, palette);
	duc_graph_set_max_name_len(graph, 30);
	duc_graph_set_size_type(graph, st);
	duc_graph_set_exact_bytes(graph, opt_bytes);
	duc_graph_set_tooltip(graph, tooltip_x, tooltip_y);
	duc_graph_set_ring_gap(graph, opt_ring_gap);
	duc_graph_set_gradient(graph, opt_gradient);

	cairo_push_group(cr);
	if(opt_dark) {
		cairo_set_source_rgb(cr, 0, 0, 0);
	} else {
		cairo_set_source_rgb(cr, 1, 1, 1);
	}
	cairo_paint(cr);
	duc_graph_draw(graph, dir);
	cairo_pop_group_to_source(cr);
	cairo_paint(cr);
	cairo_surface_flush(cs);
}


static int handle_event(XEvent e)
{
	KeySym k;
	int x, y, b;

	switch(e.type) {

		case ConfigureNotify: 
			win_w = e.xconfigure.width;
			win_h = e.xconfigure.height;
			cairo_xlib_surface_set_size(cs, win_w, win_h);
			redraw = 1;
			break;
			

		case Expose:
			if(e.xexpose.count < 1) redraw = 1;
			break;

		case KeyPress: 

			k = XLookupKeysym(&e.xkey, 0);

			if(k == XK_minus) opt_levels--;
			if(k == XK_equal) opt_levels++;
			if(k == XK_0) opt_levels = 4;
			if(k == XK_Escape) return 1;
			if(k == XK_q) return 1;
			if(k == XK_a) opt_apparent = !opt_apparent;
			if(k == XK_c) opt_count = !opt_count;
			if(k == XK_b) opt_bytes = !opt_bytes;
			if(k == XK_f) fuzz = (fuzz == 0) ? opt_fuzz : 0;
			if(k == XK_g) opt_gradient = !opt_gradient;
			if(k == XK_comma) if(opt_ring_gap > 0) opt_ring_gap --;
			if(k == XK_period) opt_ring_gap ++;
			if(k == XK_p) {
				palette = (palette + 1) % 5;
			}
			if(k == XK_BackSpace) {
				duc_dir *dir2 = duc_dir_openat(dir, "..");
				if(dir2) {
					duc_dir_close(dir);
					dir = dir2;
				}
			}

			redraw = 1;
			break;

		case ButtonPress: 

			x = e.xbutton.x;
			y = e.xbutton.y;
			b = e.xbutton.button;

			if(b == 1) {
				duc_dir *dir2 = duc_graph_find_spot(graph, dir, x, y, NULL);
				if(dir2) {
					duc_dir_close(dir);
					dir = dir2;
				}
			}
			if(b == 3) {
				duc_dir *dir2 = duc_dir_openat(dir, "..");
				if(dir2) {
					duc_dir_close(dir);
					dir = dir2;
				}
			}

			if(b == 4) opt_levels --;
			if(b == 5) opt_levels ++;

			redraw = 1;
			break;

		case MotionNotify: 

			tooltip_x = e.xmotion.x;
			tooltip_y = e.xmotion.y;
			tooltip_moved = 1;
			break;

		default:
			break;
	}

	return 0;
}


static void do_gui(duc *duc, duc_graph *graph, duc_dir *dir)
{

	pfd.fd = ConnectionNumber(dpy);
	pfd.events = POLLIN | POLLERR;
		
	int quit = 0;

	while(!quit) {

		if(redraw) {
			draw();
			XFlush(dpy);
			redraw = 0;
		}

		int r = poll(&pfd, 1, 10);

		if(r == 0) {
			if(tooltip_moved) {
				tooltip_moved = 0;
				redraw = 1;
			}
		}

		while (XEventsQueued(dpy, QueuedAfterReading) > 0) {
			XEvent e;
			XNextEvent(dpy, &e);

			quit = handle_event(e);
		}
	}

	cairo_surface_destroy(cs);
	XCloseDisplay(dpy);
}

	
int gui_main(duc *duc, int argc, char *argv[])
{
	char *path = ".";
	if(argc > 0) path = argv[0];

	fuzz = opt_fuzz;

	if(opt_palette) {
		char c = tolower(opt_palette[0]);
		if(c == 's') palette = DUC_GRAPH_PALETTE_SIZE;
		if(c == 'r') palette = DUC_GRAPH_PALETTE_RAINBOW;
		if(c == 'g') palette = DUC_GRAPH_PALETTE_GREYSCALE;
		if(c == 'm') palette = DUC_GRAPH_PALETTE_MONOCHROME;
		if(c == 'c') palette = DUC_GRAPH_PALETTE_CLASSIC;
	}

	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}
	
	dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}
	
	dpy = XOpenDisplay(NULL);
	if(dpy == NULL) {
		duc_log(duc, DUC_LOG_FTL, "ERROR: Could not open display");
		exit(1);
	}

	int scr = DefaultScreen(dpy);
	rootwin = RootWindow(dpy, scr);

	Window win = XCreateSimpleWindow(
			dpy, 
			rootwin, 
			1, 1, 
			win_w, win_h, 0, 
			BlackPixel(dpy, scr), WhitePixel(dpy, scr));

	XSelectInput(dpy, win, ExposureMask | ButtonPressMask | StructureNotifyMask | KeyPressMask | PointerMotionMask);
	XMapWindow(dpy, win);
	
	cs = cairo_xlib_surface_create(dpy, win, DefaultVisual(dpy, 0), win_w, win_h);
	cr = cairo_create(cs);



	graph = duc_graph_new_cairo(duc, cr);

	do_gui(duc, graph, dir);

	duc_dir_close(dir);

	return 0;
}

static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "show apparent instead of actual file size" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_count,     "count",      0,  DUCRC_TYPE_BOOL,   "show number of files instead of file size" },
	{ &opt_dark,      "dark",       0,  DUCRC_TYPE_BOOL,   "use dark background color" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_fuzz,      "fuzz",       0,  DUCRC_TYPE_DOUBLE, "use radius fuzz factor when drawing graph" },
	{ &opt_gradient,  "gradient",   0,  DUCRC_TYPE_BOOL,   "draw graph with color gradient" },
	{ &opt_levels,    "levels",    'l', DUCRC_TYPE_INT,    "draw up to VAL levels deep [4]" },
	{ &opt_palette,   "palette",    0,  DUCRC_TYPE_STRING, "select palette",
		"available palettes are: size, rainbow, greyscale, monochrome, classic" },
	{ &opt_ring_gap,  "ring-gap",   0,  DUCRC_TYPE_INT,    "leave a gap of VAL pixels between rings" },
	{ NULL }
};


struct cmd cmd_gui = {
	.name = "gui",
	.descr_short = "Interactive X11 graphical interface",
	.usage = "[options] [PATH]",
	.main = gui_main,
	.options = options,
	.descr_long = 
		"The 'gui' subcommand queries the duc database and runs an interactive graphical\n"
		"utility for exploring the disk usage of the given path. If no path is given the\n"
		"current working directory is explored.\n"
		"\n"
		"The following keys can be used to navigate and alter the graph:\n"
		"\n"
		"    +           increase maximum graph depth\n"
		"    -           decrease maximum graph depth\n"
		"    0           Set default graph depth\n"
		"    a           Toggle between apparent and actual disk usage\n"
		"    b           Toggle between exact byte count and abbreviated sizes\n"
		"    c           Toggle between file size and file count\n"
		"    f           toggle graph fuzz\n"
		"    g           toggle graph gradient\n"
		"    p           toggle palettes\n"
		"    backspace   go up one directory\n"

};

#endif

/*
 * End
 */
