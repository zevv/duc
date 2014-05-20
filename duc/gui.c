
#include <getopt.h>
#include <cairo.h>
#include <string.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>
#include <sys/poll.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "duc.h"
#include "cmd.h"

#define SIZEX 600
#define SIZEY 600
#define DEPTH 4



int do_gui(duc *duc, char *root)
{
	Display *dpy;
	Window rootwin;
	Window win;
	XEvent e;
	int scr;
	cairo_surface_t *cs;
	cairo_t *cr;
	static int size = 400;
	char path[PATH_MAX];

	strncpy(path, root, sizeof path);

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

	XSelectInput(dpy, win, ExposureMask | ButtonPressMask | StructureNotifyMask);
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
				printf("redraw\n");
				XClearWindow(dpy, win);
				cairo_move_to(cr, 20, 20);
				cairo_show_text(cr, path);
				duc_graph_cairo(dir, size, DEPTH, cr);
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

				case ButtonPress: {

					int x = e.xbutton.x;
					int y = e.xbutton.y;
					char newpath[PATH_MAX];
					int r = duc_graph_xy_to_path(dir, size, 4, x, y, newpath, sizeof newpath);
					if(r) {
						char oldpath[PATH_MAX];
						snprintf(oldpath, sizeof oldpath, "%s", path);
						snprintf(path, sizeof path, "%s%s", oldpath, newpath);
					} else {
						dirname(path);
					}

					printf("%s\n", path);
					duc_dir *dir_new = duc_opendir(duc, path);
					if(dir_new) {
						duc_closedir(dir);
						dir = dir_new;
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
	

	do_gui(duc, path);

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
