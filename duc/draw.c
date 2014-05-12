
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>

#include <cairo.h>

#include "duc.h"
#include "cmd.h"

struct label {
	int x, y;
	char *text;
	struct label *next;
};


struct graph {
	struct duc *duc;
	int cx, cy;
	int ring_width;
	int depth;
	struct label *label_list;
	cairo_t *cr;
};


static void pol2car(struct graph *graph, double a, double r, int *x, int *y)
{
	*x = cos(a) * r + graph->cx;
	*y = sin(a) * r + graph->cy;
}


static void hsv2rgb(double h, double s, double v, double *r, double *g, double *b)
{	
	double f, m, n;
	int i;
	
	h *= 6.0;
	i = floor(h);
	f = (h) - i;
	if (!(i & 1)) f = 1 - f;
	m = v * (1 - s);
	n = v * (1 - s * f);
	if(i<0) i=0;
	if(i>6) i=6;
	switch (i) {
		case 6:
		case 0: *r=v; *g=n, *b=m; break;
		case 1: *r=n; *g=v, *b=m; break;
		case 2: *r=m; *g=v, *b=n; break;
		case 3: *r=m; *g=n, *b=v; break;
		case 4: *r=n; *g=m, *b=v; break;
		case 5: *r=v; *g=m, *b=n; break;
		default: *r=*g=*b=1;
	}
}


static void draw_text(cairo_t *cr, int x, int y, char *text)
{
	char *p = text;
	int lines = 1;
	int size = 11;
	while(*p) if(*p++ == '\n') lines++;

	y -= (lines-1) * (size+2) / 2.0;

	cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, size);

	cairo_text_extents_t ext;
	cairo_text_extents(cr, text, &ext);

	int w = ext.width;
	int h = ext.height;
	cairo_move_to(cr, x - w/2, y + h/2);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.8);
	cairo_set_line_width(cr, 1.5);
	cairo_text_path(cr, text);
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to(cr, x - w/2, y + h/2);
	cairo_show_text(cr, text);
}


double ang(double a)
{
	return -M_PI * 0.5 + M_PI * 2 * a;
}


static void draw_section(struct graph *graph, double a_from, double a_to, int r_from, int r_to, double brightness)
{
	cairo_t *cr = graph->cr;
			
	double r, g, b;
	hsv2rgb((a_from+a_to)*0.5, 1.0-brightness, brightness/2+0.5, &r, &g, &b);

	cairo_pattern_t *pat;
	pat = cairo_pattern_create_radial(graph->cx, graph->cy, 0, graph->cx, graph->cy, graph->cx-50);
	cairo_pattern_add_color_stop_rgb(pat, (double)r_from / graph->cx, r*0.8, g*0.8, b*0.8);
	cairo_pattern_add_color_stop_rgb(pat, (double)r_to / graph->cx, r*1.5, g*1.5, b*1.5);
	cairo_set_source(cr, pat);

	cairo_new_path(cr);
	cairo_arc(cr, graph->cx, graph->cy, r_from, ang(a_from), ang(a_to));
	cairo_arc_negative(cr, graph->cx, graph->cy, r_to, ang(a_to), ang(a_from));
	cairo_close_path(cr);

	cairo_fill_preserve(cr);

	cairo_set_line_width(cr, 0.3);
	cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.3);
	cairo_stroke(cr);
	cairo_pattern_destroy(pat);
}


static void draw_ring(struct graph *graph, ducdir *dir, int level, double a_min, double a_max)
{
	double a_range = a_max - a_min;
	double a_from = a_min;
	double a_to = a_min;

	/* Calculate max and total size */
	
	off_t size_total = 0;

	struct ducent *e;
	while( (e = duc_readdir(dir)) != NULL) {
		size_total += e->size;
	}

	duc_rewinddir(dir);
	while( (e = duc_readdir(dir)) != NULL) {

		a_to += a_range * e->size / size_total;

		if(a_to > a_from) {
			double r_from = (level+1) * graph->ring_width;
			double r_to = r_from + graph->ring_width;
		
			double brightness = 0.8 * r_from / graph->cx;

			draw_section(graph, a_from, a_to, r_from, r_to, brightness);

			if(S_ISDIR(e->mode)) {
				if(level+1 < graph->depth) {
					ducdir *dir_child = duc_opendirat(graph->duc, e->dev, e->ino);
					draw_ring(graph, dir_child, level + 1, a_from, a_to);
					duc_closedir(dir_child);
				} else {
					draw_section(graph, a_from, a_to, r_to, r_to+5, 0.5);
				}
			}

			if(r_from * (a_to - a_from) > 5) {
				struct label *label = malloc(sizeof *label);
				pol2car(graph, ang((a_from+a_to)/2), (r_from+r_to)/2, &label->x, &label->y);
				label->text = strdup(e->name);
				label->next = graph->label_list;
				graph->label_list = label;
			}
		}
		
		a_from = a_to;
	}
}



static int draw_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;
	struct graph graph;
	int size = 800;
	char *path_out = "duc.png";
	
	graph.depth = 4;

	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ "levels",         required_argument, NULL, 'l' },
		{ "output",         required_argument, NULL, 'o' },
		{ "size",           required_argument, NULL, 's' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d:l:o:s:", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 'l':
				graph.depth = atoi(optarg);
				break;
			case 'o':
				path_out = optarg;
				break;
			case 's':
				size = atoi(optarg);
				break;
			default:
				return -2;
		}
	}

	argc -= optind;
	argv += optind;
	
	char *path = ".";
	if(argc > 0) path = argv[0];

	graph.ring_width = ((size-30) / 2) / (graph.depth + 1);
	graph.label_list = NULL;

	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
	cairo_t *cr = cairo_create(surface);

	graph.cx = size / 2;
	graph.cy = size / 2;
	graph.cr = cr;

	/* Open duc context */

	graph.duc = duc_open(path_db, DUC_OPEN_RO);

	ducdir *dir = duc_opendir(graph.duc, path);
	if(dir == NULL) {
		fprintf(stderr, "Path not found in database\n");
		return -1;
	}

	/* Recursively draw graph */

	draw_ring(&graph, dir, 0, 0, 1);

	/* Draw collected labels */

	cairo_set_line_width(cr, 0.3);
	cairo_set_source_rgba(graph.cr, 0, 0, 0, 0.7);
	cairo_stroke(cr);

	int i;
	for(i=1; i<=graph.depth+1; i++) {
		cairo_new_path(cr);
		cairo_arc(graph.cr, graph.cx, graph.cy, i * graph.ring_width, 0, 2*M_PI);
		cairo_stroke(cr);
	}
	cairo_stroke(graph.cr);

	struct label *label = graph.label_list;
	while(label) {
		draw_text(cr, label->x, label->y, label->text);
		free(label->text);
		struct label *next = label->next;
		free(label);
		label = next;
	}

	cairo_destroy(graph.cr);
	cairo_surface_write_to_png(surface, path_out);
	cairo_surface_destroy(surface);

	duc_closedir(dir);
	duc_close(graph.duc);

	return 0;
}


	
struct cmd cmd_draw = {
	.name = "draw",
	.description = "Draw graph",
	.usage = "[options] [PATH]",
	.help = 
		"Valid options:\n"
		"\n"
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n"
	        "  -l, --levels=ARG        draw up to ARG levels deep [4]\n"
		"  -o, --output=ARG        output file name [duc.png]\n"
	        "  -s, --size=ARG          image size [800]\n",
	.main = draw_main
};

/*
 * End
 */

