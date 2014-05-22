
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>
#include <libgen.h>

#include <cairo.h>
#include <pango/pangocairo.h>

#include "duc.h"
#include "duc-graph.h"

#define FONT_SIZE_LABEL 8
#define FONT_SIZE_BACK 12

#define MAX_DEPTH 32

struct label {
	int x, y;
	char *text;
	struct label *next;
};


struct graph {
	struct duc *duc;
	int cx, cy;
	int ring_width;
	int max_level;
	struct label *label_list;
	cairo_t *cr;

	double spot_a;
	double spot_r;
	char *spot_part[MAX_DEPTH];
};


static void pol2car(struct graph *graph, double a, double r, int *x, int *y)
{
	*x = cos(a) * r + graph->cx;
	*y = sin(a) * r + graph->cy;
}


static void car2pol(struct graph *graph, int x, int y, double *a, double *r)
{
	x -= graph->cx;
	y -= graph->cy;
	*r = hypot(y, x);
	*a = atan2(x, -y) / (M_PI*2);
	if(*a < 0) *a += 1;
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


static void draw_text(cairo_t *cr, int x, int y, int size, char *text)
{
	char font[32];
	snprintf(font, sizeof font, "Arial, Sans, %d", size);
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc = pango_font_description_from_string(font);

	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_text(layout, text, -1);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	pango_cairo_update_layout(cr, layout);

	int w,h;
	pango_layout_get_size(layout, &w, &h);

	x -= (w/PANGO_SCALE/2);
	y -= (h/PANGO_SCALE/2);

	cairo_move_to(cr, x, y);
	pango_cairo_layout_path(cr, layout);
	g_object_unref(layout);
	
	/* light grey background */

	cairo_set_line_join (cr, CAIRO_LINE_JOIN_BEVEL);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.4);
	cairo_set_line_width(cr, 3);
	cairo_stroke_preserve(cr);

	/* black text */

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_set_line_width(cr, 1);
	cairo_fill(cr);
}


double ang(double a)
{
	return -M_PI * 0.5 + M_PI * 2 * a;
}


static int draw_section(struct graph *graph, double a1, double a2, int r_from, int r_to, double hue, double brightness)
{

	cairo_t *cr = graph->cr;
	
	if(cr) {

		double r = 0.6, g = 0.6, b = 0.6;

		if(brightness > 0) {
			hsv2rgb(hue, 1.0-brightness, brightness/2+0.5, &r, &g, &b);
		}
			
		cairo_new_path(cr);
		cairo_arc(cr, graph->cx, graph->cy, r_from, ang(a1), ang(a2));
		cairo_arc_negative(cr, graph->cx, graph->cy, r_to, ang(a2), ang(a1));
		cairo_close_path(cr);

		cairo_pattern_t *pat;
		pat = cairo_pattern_create_radial(graph->cx, graph->cy, 0, graph->cx, graph->cy, graph->cx-50);
		cairo_pattern_add_color_stop_rgb(pat, (double)r_from / graph->cx, r*0.5, g*0.5, b*0.5);
		cairo_pattern_add_color_stop_rgb(pat, (double)r_to   / graph->cx, r*1.5, g*1.5, b*1.5);
		cairo_set_source(cr, pat);

		cairo_fill_preserve(cr);
		cairo_pattern_destroy(pat);

		cairo_set_line_width(cr, 0.5);
		cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.9);
		cairo_stroke(cr);
	}

	return 0;
}


/*
 * This function has two purposes:
 *
 * - If a cairo context is provided, a graph will be drawn on that context
 * - if spot_a and spot_r are defined, it will find the path on the graph
 *   that is below that spot
 */

static int do_dir(struct graph *graph, duc_dir *dir, int level, double a_min, double a_max)
{
	int spot_found = 0;

	double a_range = a_max - a_min;
	double a1 = a_min;
	double a2 = a_min;
			
	double r_from = (level+1) * graph->ring_width;
	double r_to = r_from + graph->ring_width;

	/* Calculate max and total size */
	
	off_t size_total = duc_dirsize(dir);

	struct duc_dirent *e;

	size_t size_min = size_total;
	size_t size_max = 0;

	while( (e = duc_readdir(dir)) != NULL) {
		if(e->size < size_min) size_min = e->size;
		if(e->size > size_max) size_max = e->size;
	}

	/* Rewind and iterate the objects to graph */

	duc_rewinddir(dir);
	while( (e = duc_readdir(dir)) != NULL) {
		
		/* size_rel is size relative to total, size_nrel is size relative to min and max */

		double size_rel = (double)e->size / size_total;
		double size_nrel = (size_max == size_min) ? 0 : ((double)e->size - size_min) / (size_max - size_min);

		a2 += a_range * size_rel;

		/* Skip any segments that would be smaller then one pixel */

		if(r_to * (a2 - a1) * M_PI * 2 < 2) break;
		if(a2 <= a1) break;

		/* Color of the segment depends on it's relative size to other
		 * objects in the same directory */

		double hue = 0.8 - 0.8 * size_nrel;
		double brightness = 0.8 * r_from / graph->cx;

		/* Check if the requested spot lies in this section */

		if(graph->spot_r > 0) {
			double a = graph->spot_a;
			double r = graph->spot_r;

			if(a >= a1 && a < a2 && r >= r_from && r < r_to) {
				spot_found = 1;
			}
		}

		/* Draw section for this object */

		int found = draw_section(graph, a1, a2, r_from, r_to, hue, brightness);
		if(found) {
			graph->spot_part[level] = strdup(e->name);
			return 1;
		}

		/* Recurse into subdirectories */

		if(e->mode == DUC_MODE_DIR) {
			if(level+1 < graph->max_level) {
				duc_dir *dir_child = duc_opendirat(dir, e);
				if(!dir_child) continue;
				int r = do_dir(graph, dir_child, level + 1, a1, a2);
				duc_closedir(dir_child);
				if(r) spot_found = 1;
			} else {
				draw_section(graph, a1, a2, r_to, r_to+5, hue, 0.5);
			}
		}

		/* Place labels if there is enough room to display */

		if(r_from * (a2 - a1) > 5) {
			struct label *label = malloc(sizeof *label);
			pol2car(graph, ang((a1+a2)/2), (r_from+r_to)/2, &label->x, &label->y);
			char siz[32];
			duc_humanize(e->size, siz, sizeof siz);
			int r = asprintf(&label->text, "%s\n%s", e->name, siz);
			if(r > 0) {
				label->next = graph->label_list;
				graph->label_list = label;
			} else {
				free(label);
			}
		}

		if(spot_found) {
			graph->spot_part[level] = strdup(e->name);
			return 1;
		}
		
		a1 = a2;
	}

	return 0;
}


static cairo_status_t cairo_writer(void *closure, const unsigned char *data, unsigned int length)
{
	FILE *f = closure;
	fwrite(data, length, 1, f);
	return CAIRO_STATUS_SUCCESS;
}


int duc_graph(duc *duc, duc_dir *dir, int size, int max_level, FILE *fout)
{
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
	assert(surface);
	cairo_t *cr = cairo_create(surface);
	assert(cr);

	duc_graph_cairo(duc, dir, size, max_level, cr);

	cairo_destroy(cr);
	cairo_surface_write_to_png_stream(surface, cairo_writer, fout);
	cairo_surface_destroy(surface);

	return 0;
}


int duc_graph_cairo(duc *duc, duc_dir *dir, int size, int max_level, cairo_t *cr)
{
	struct graph graph;
	memset(&graph, 0, sizeof graph);

	graph.duc = duc;
	graph.max_level = max_level;
	graph.ring_width = ((size-30) / 2) / (graph.max_level + 1);
	graph.label_list = NULL;
	graph.cx = size / 2;
	graph.cy = size / 2;
	graph.cr = cr;

	cairo_save(cr);

	/* Recursively draw graph */
	
	duc_rewinddir(dir);
	do_dir(&graph, dir, 1, 0, 1);

	/* Draw collected labels */

	struct label *label = graph.label_list;
	while(label) {
		draw_text(cr, label->x, label->y, FONT_SIZE_LABEL, label->text);
		free(label->text);
		struct label *next = label->next;
		free(label);
		label = next;
	}

	draw_text(cr, graph.cx, graph.cy, 16, "cd ../");
	
	cairo_restore(cr);

	return 0;
}


int duc_graph_xy_to_path(duc *duc, duc_dir *dir, int size, int max_level, int x, int y, char *path, size_t path_len)
{

	struct graph graph;
	memset(&graph, 0, sizeof graph);

	graph.duc = duc;
	graph.max_level = max_level;
	graph.ring_width = ((size-30) / 2) / (graph.max_level + 1);
	graph.label_list = NULL;
	graph.cx = size / 2;
	graph.cy = size / 2;

	car2pol(&graph, x, y, &graph.spot_a, &graph.spot_r);

	/* If clicked in the center, go up one directory */

	double r = hypot(x - size/2, y - size/2) / graph.ring_width;

	if(r < 2) {
		snprintf(path, path_len, "%s", duc_dirpath(dir));
		dirname(path);
		return 1;
	}

	duc_rewinddir(dir);
	int found = do_dir(&graph, dir, 1, 0, 1);
		
	strncpy(path, duc_dirpath(dir), path_len);

	if(found) {

		int i;
		for(i=0; i<graph.max_level; i++) {
			if(graph.spot_part[i]) {
				strcat(path, "/");
				strcat(path, graph.spot_part[i]);
				free(graph.spot_part[i]);
			}
		}
	}

	return found;
}


/*
 * End
 */

