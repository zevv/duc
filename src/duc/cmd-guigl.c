#include "config.h"

#ifdef ENABLE_GUI

#include "duc.h"
#include "duc-graph.h"
#include "ducrc.h"
#include "cmd.h"

#ifdef HAVE_LIBGLUT

#include <stdio.h>
#include <ctype.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <string.h>

#include <GL/gl.h>
#include <GL/glut.h>

static int opt_bytes;
static int opt_dark;
static char *opt_database = NULL;
static char *opt_palette = NULL;
static double opt_fuzz = 0.5;
static int opt_levels = 4;
static int opt_apparent = 0;
static int opt_ring_gap = 0;

//static int redraw = 1;
static int tooltip_x = 0;
static int tooltip_y = 0;
//static int tooltip_moved = 0;
static enum duc_graph_palette palette = 0;
static int win_w = 600;
static int win_h = 600;
static duc_dir *dir;
static duc_graph *graph;
static double fuzz;


static void cb_reshape(int w, int h)
{
	win_w = w;
	win_h = h;
	glViewport(0, 0, win_w, win_h);
}


static void cb_draw(void)
{
	if(opt_levels < 1) opt_levels = 1;
	if(opt_levels > 10) opt_levels = 10;

	duc_graph_set_size(graph, win_w, win_h);
	duc_graph_set_max_level(graph, opt_levels);
	duc_graph_set_fuzz(graph, fuzz);
	duc_graph_set_palette(graph, palette);
	duc_graph_set_max_name_len(graph, 30);
	duc_graph_set_size_type(graph, opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL);
	duc_graph_set_exact_bytes(graph, opt_bytes);
	duc_graph_set_tooltip(graph, tooltip_x, tooltip_y);
	duc_graph_set_ring_gap(graph, opt_ring_gap);

	duc_graph_draw(graph, dir);

	glutSwapBuffers();
}
	

void cb_keyboard(unsigned char k, int x, int y)
{
	if(k == '-') opt_levels--;
	if(k == '=') opt_levels++;
	if(k == '0') opt_levels = 4;
	if(k == 27) exit(0);
	if(k == 'q') exit(0);
	if(k == 'a') opt_apparent = !opt_apparent;
	if(k == 'b') opt_bytes = !opt_bytes;
	if(k == 'f') fuzz = (fuzz == 0) ? opt_fuzz : 0;
	if(k == ',') if(opt_ring_gap > 0) opt_ring_gap --;
	if(k == '.') opt_ring_gap ++;
	if(k == 'p') {
		palette = (palette + 1) % 4;
	}
	if(k == 8) {
		duc_dir *dir2 = duc_dir_openat(dir, "..");
		if(dir2) {
			duc_dir_close(dir);
			dir = dir2;
		}
	}

	cb_draw();
}


void cb_mouse_button(int b, int state, int x, int y)
{
	if(state != GLUT_DOWN) return;

	if(b == 0) {
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

	cb_draw();
}


void cb_mouse_motion(int x, int y)
{
	tooltip_x = x;
	tooltip_y = y;
	cb_draw();
}


int guigl_main(duc *duc, int argc, char *argv[])
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
	
	glutInit(&argc, argv);
	glutInitWindowSize(win_w, win_h);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutCreateWindow("duc");

	graph = duc_graph_new_opengl(duc);

	glutDisplayFunc(cb_draw);
	glutReshapeFunc(cb_reshape);
	glutKeyboardFunc(cb_keyboard);
	glutMouseFunc(cb_mouse_button);
	glutPassiveMotionFunc(cb_mouse_motion);
	//glutIdleFunc(cb_idle);
	glutIgnoreKeyRepeat(1);
	glutMainLoop();

	return 0;
}

static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "show apparent instead of actual file size" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_dark,      "dark",       0,  DUCRC_TYPE_BOOL,   "use dark background color" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_fuzz,      "fuzz",       0,  DUCRC_TYPE_DOUBLE, "use radius fuzz factor when drawing graph" },
	{ &opt_levels,    "levels",    'l', DUCRC_TYPE_INT,    "draw up to VAL levels deep [4]" },
	{ &opt_palette,   "palette",    0,  DUCRC_TYPE_STRING, "select palette <size|rainbow|greyscale|monochrome>" },
	{ &opt_ring_gap,  "ring-gap",   0,  DUCRC_TYPE_INT,    "leave a gap of VAL pixels between rings" },
	{ NULL }
};


#else

static int guigl_main(int argc, char *argv[])
{
	duc_log(NULL, DUC_LOG_FTL, "'duc gui' is not supported on this platform");
	return -1;
}

static struct ducrc_option options[] = {
	{ NULL }
};

#endif

struct cmd cmd_guigl = {
	.name = "guigl",
	.descr_short = "Interactive OpenGL graphical interface",
	.usage = "[options] [PATH]",
	.main = guigl_main,
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
		"    p           toggle palettes\n"
		"    f           toggle graph fuzz\n"
		"    backspace   go up one directory\n"

};

#endif

/*
 * End
 */
