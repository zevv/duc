 
#include <getopt.h>
#include <cairo.h>
#include <string.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/poll.h>
#include <libgen.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "duc.h"
#include "duc-graph.h"
#include "cmd.h"

#define SIZEX 600
#define SIZEY 600

static int depth = 4;


int do_gui(duc *duc, duc_graph *graph, char *root)
{
	Display *dpy;
	Window rootwin;
	Window win;
	XEvent e;
	int scr;
	cairo_surface_t *cs;
	cairo_t *cr;
	int size = 400;
	int palette = 0;

	duc_dir *dir = duc_opendir(duc, root);
	if(dir == NULL) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}

	if(!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "ERROR: Could not open display\n");
		exit(1);
	}

	scr = DefaultScreen(dpy);
	rootwin = RootWindow(dpy, scr);

	win = XCreateSimpleWindow(
			dpy, 
			rootwin, 
			1, 1, 
			SIZEX, SIZEY, 0, 
			BlackPixel(dpy, scr), WhitePixel(dpy, scr));

	XSelectInput(dpy, win, ExposureMask | ButtonPressMask | StructureNotifyMask | KeyPressMask);
	XMapWindow(dpy, win);

	cs =  cairo_xlib_surface_create(dpy, win, DefaultVisual(dpy, 0), SIZEX, SIZEY);
	cr = cairo_create(cs);

	int redraw = 0;

	struct pollfd pfd = {
		.fd = ConnectionNumber(dpy),
		.events = POLLIN,
	};

	while(1) {
	
		int r = poll(&pfd, 1, 10);

		if(r == 0) {

			if(redraw) {
				char *path = duc_dirpath(dir);
				XClearWindow(dpy, win);
				cairo_move_to(cr, 20, 20);
				cairo_show_text(cr, path);
				free(path);

				duc_graph_set_size(graph, size);
				duc_graph_set_max_level(graph, depth);
				duc_graph_draw_cairo(graph, dir, cr);
				XFlush(dpy);
				redraw = 0;
			}
		}

		if(r == 1) {

			XNextEvent(dpy, &e);

			switch(e.type) {

				case ConfigureNotify: {
					int w = e.xconfigure.width;
					int h = e.xconfigure.height;
					size = w;
					if(h < w) size = h;
					cairo_xlib_surface_set_size(cs, w, h);
					redraw = 1;
					break;
				}

				case Expose:
					if(e.xexpose.count < 1) redraw = 1;
					break;
				
				case KeyPress: {
					KeySym k = XLookupKeysym(&e.xkey, 0);
					if(k == XK_minus) depth++;
					if(k == XK_equal) depth--;
					if(k == XK_0) depth = 4;
					if(k == XK_Escape) exit(0);
					if(k == XK_q) exit(0);
					if(k == XK_p) {
						palette = (palette + 1) % 4;
						duc_graph_set_palette(graph, palette);
					}
					if(k == XK_BackSpace) {
						duc_dir *dir2 = duc_opendirat(dir, "..");
						if(dir2) {
							duc_closedir(dir);
							dir = dir2;
						}
					}
					if(depth < 2) depth = 2;
					if(depth > 20) depth = 20;
					redraw = 1;
					break;
				}

				case ButtonPress: {

					int x = e.xbutton.x;
					int y = e.xbutton.y;

					duc_dir *dir2 = duc_graph_find_spot(graph, dir, x, y);
					if(dir2) {
						duc_closedir(dir);
						dir = dir2;
						redraw = 1;
					}

					break;
				}
			}
		}
	}

	cairo_surface_destroy(cs);
	XCloseDisplay(dpy);
}


int gui_main(int argc, char *argv[])
{
	char *path_db = NULL;
	int c;

	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "bd:Fn:", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
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

	int r = duc_open(duc, path_db, DUC_OPEN_RO);
	if(r != DUC_OK) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}

	duc_graph *graph = duc_graph_new(duc);

	do_gui(duc, graph, path);

	return 0;
}


struct cmd cmd_gui = {
	.name = "gui",
	.description = "Graphical interface",
	.usage = "[options] [PATH]",
	.help = 
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n",
	.main = gui_main
};


/*
 * End
 */
