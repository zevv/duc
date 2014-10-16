
#include "config.h"
#include "duc.h"
#include "duc-graph.h"
#include "cmd.h"

#ifdef HAVE_LIBX11

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <cairo.h>
#include <string.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

static int depth = 4;


int do_gui(duc *duc, duc_graph *graph, duc_dir *dir)
{
	int palette = 0;
	int win_w = 600;
	int win_h = 600;
	double fuzz = 0;

	Display *dpy = XOpenDisplay(NULL);
	if(dpy == NULL) {
		fprintf(stderr, "ERROR: Could not open display\n");
		exit(1);
	}

	int scr = DefaultScreen(dpy);
	Window rootwin = RootWindow(dpy, scr);

	Window win = XCreateSimpleWindow(
			dpy, 
			rootwin, 
			1, 1, 
			win_w, win_h, 0, 
			BlackPixel(dpy, scr), WhitePixel(dpy, scr));

	XSelectInput(dpy, win, ExposureMask | ButtonPressMask | StructureNotifyMask | KeyPressMask);
	XMapWindow(dpy, win);
	
	cairo_surface_t *cs = cairo_xlib_surface_create(dpy, win, DefaultVisual(dpy, 0), win_w, win_h);

	int redraw = 1;

	while(1) {
	
		if(redraw && cs) {
			cairo_t *cr;

			cairo_surface_t *cs2 = cairo_surface_create_similar(
					cs, 
					CAIRO_CONTENT_COLOR,
					cairo_xlib_surface_get_width(cs),
					cairo_xlib_surface_get_height(cs));

			cr = cairo_create(cs2);

			cairo_set_source_rgb(cr, 1, 1, 1);
			cairo_paint(cr);

			int size = win_w < win_h ? win_w : win_h;
			int pos_x = (win_w - size) / 2;
			int pos_y = (win_h - size) / 2;

			if(depth < 2) depth = 2;
			if(depth > 10) depth = 10;

			duc_graph_set_size(graph, size);
			duc_graph_set_position(graph, pos_x, pos_y);
			duc_graph_set_max_level(graph, depth);
			duc_graph_set_fuzz(graph, fuzz);
			duc_graph_set_max_name_len(graph, 30);
			duc_graph_draw_cairo(graph, dir, cr);
			cairo_destroy(cr);

			cr = cairo_create(cs);
			cairo_set_source_surface(cr, cs2, 0, 0);
			cairo_paint(cr);
			cairo_destroy(cr);

			cairo_surface_destroy(cs2);

			XFlush(dpy);
			redraw = 0;
		}

		if(1) {

			XEvent e;
			XNextEvent(dpy, &e);

			switch(e.type) {

				case ConfigureNotify: {
					win_w = e.xconfigure.width;
					win_h = e.xconfigure.height;
					cairo_xlib_surface_set_size(cs, win_w, win_h);
					redraw = 1;
					break;
				}

				case Expose:
					if(e.xexpose.count < 1) redraw = 1;
					break;
				
				case KeyPress: {

					KeySym k = XLookupKeysym(&e.xkey, 0);

					if(k == XK_minus) depth--;
					if(k == XK_equal) depth++;
					if(k == XK_0) depth = 4;
					if(k == XK_Escape) exit(0);
					if(k == XK_q) exit(0);
					if(k == XK_f) fuzz = 0.7 - fuzz;
					if(k == XK_p) {
						palette = (palette + 1) % 4;
						duc_graph_set_palette(graph, palette);
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
				}

				case ButtonPress: {

					int x = e.xbutton.x;
					int y = e.xbutton.y;
					int b = e.xbutton.button;

					if(b == 1) {
						duc_dir *dir2 = duc_graph_find_spot(graph, dir, x, y);
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

					if(b == 4) depth --;
					if(b == 5) depth ++;

					redraw = 1;
					break;
				}
			}
		}
	}

	cairo_surface_destroy(cs);
	XCloseDisplay(dpy);
}

	
static struct option longopts[] = {
	{ "database",       required_argument, NULL, 'd' },
	{ "verbose",        required_argument, NULL, 'v' },
	{ NULL }
};


int gui_main(int argc, char *argv[])
{
	char *path_db = NULL;
	int c;
	duc_log_level loglevel = DUC_LOG_WRN;

	while( ( c = getopt_long(argc, argv, "d:qv", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 'q':
				loglevel = DUC_LOG_FTL;
				break;
			case 'v':
				if(loglevel < DUC_LOG_DMP) loglevel ++;
				break;
			default:
				return -2;
		}
	}
	
	argc -= optind;
	argv += optind;

	char *path = ".";
	if(argc > 0) path = argv[0];

	/* Open duc context */
	
	duc *duc = duc_new();
	if(duc == NULL) {
                fprintf(stderr, "Error creating duc context\n");
                return -1;
        }
	
	duc_set_log_level(duc, loglevel);

	int r = duc_open(duc, path_db, DUC_OPEN_RO);
	if(r != DUC_OK) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}
	
	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}

	duc_graph *graph = duc_graph_new(duc);

	do_gui(duc, graph, dir);

	return 0;
}

#else

int gui_main(int argc, char *argv[])
{
	printf("Not supported on this platform\n");
	return -1;
}

#endif

struct cmd cmd_gui = {
	.name = "gui",
	.description = "Graphical interface",
	.usage = "[options] [PATH]",
	.help = 
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n"
		"  -q, --quiet             quiet mode, do not print any warnings\n"
		"  -v, --verbose           verbose mode, can be passed two times for debugging\n",
	.main = gui_main
};



/*
 * End
 */
