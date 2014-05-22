
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

#include "list.h"
#include "duc.h"
#include "duc-graph.h"

#define FONT_SIZE_LABEL 8
#define FONT_SIZE_BACK 12

#define MAX_DEPTH 32

struct label {
	int x, y;
	char *text;
};


struct duc_graph {

	/* Settings */

	struct duc *duc;
	double size;
	double cx, cy;
	double r_start;
	int max_level;

	/* Reusable runtime info. Cleared after each graph_draw_* call */

	struct list *label_list;
	double spot_a;
	double spot_r;
	duc_dir *spot_dir;
};


duc_graph *duc_graph_new(duc *duc)
{
	duc_graph *g;
	
	g = malloc(sizeof *g);
	memset(g, 0, sizeof *g);

	g->duc = duc;
	g->r_start = 100;
	duc_graph_set_max_level(g, 3);
	duc_graph_set_size(g, 400);

	return g;
}


void duc_graph_free(duc_graph *g)
{
	free(g);
}


void duc_graph_set_max_level(duc_graph *g, int max_level)
{
	g->max_level = max_level;
}


void duc_graph_set_size(duc_graph *g, int size)
{
	g->size = size;
	g->cx = size / 2;
	g->cy = size / 2;
	g->r_start = size / 10;
}



static void pol2car(duc_graph *g, double a, double r, int *x, int *y)
{
	*x = cos(a) * r + g->cx;
	*y = sin(a) * r + g->cy;
}


static void car2pol(duc_graph *g, int x, int y, double *a, double *r)
{
	x -= g->cx;
	y -= g->cy;
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


static void draw_section(duc_graph *g, cairo_t *cr, double a1, double a2, int r1, int r_to, double hue, double brightness)
{
	if(cr == NULL) return;

	double R = 0.6, G = 0.6, B = 0.6;

	if(brightness > 0) {
		hsv2rgb(hue, 1.0-brightness, brightness/2+0.5, &R, &G, &B);
	}

	cairo_new_path(cr);
	cairo_arc(cr, g->cx, g->cy, r1, ang(a1), ang(a2));
	cairo_arc_negative(cr, g->cx, g->cy, r_to, ang(a2), ang(a1));
	cairo_close_path(cr);

	cairo_pattern_t *pat;
	pat = cairo_pattern_create_radial(g->cx, g->cy, 0, g->cx, g->cy, g->cx-50);
	cairo_pattern_add_color_stop_rgb(pat, (double)r1 / g->cx, R*0.5, G*0.5, B*0.5);
	cairo_pattern_add_color_stop_rgb(pat, (double)r_to   / g->cx, R*1.5, G*1.5, B*1.5);
	cairo_set_source(cr, pat);

	cairo_fill_preserve(cr);
	cairo_pattern_destroy(pat);

	cairo_set_line_width(cr, 0.5);
	cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.9);
	cairo_stroke(cr);
}


/*
 * This function has two purposes:
 *
 * - If a cairo context is provided, a graph will be drawn on that context
 * - if spot_a and spot_r are defined, it will find the path on the graph
 *   that is below that spot
 */

static int do_dir(duc_graph *g, cairo_t *cr, duc_dir *dir, int level, double r1, double a1_dir, double a2_dir)
{
	double a_range = a2_dir - a1_dir;
	double a1 = a1_dir;
	double a2 = a1_dir;
			
	double ring_width = (g->size/2 - g->r_start - 10) / g->max_level;

	double r_to = r1 + ring_width;

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
		double brightness = 0.8 * r1 / g->cx;

		/* Check if the requested spot lies in this section */

		if(g->spot_r > 0) {
			double a = g->spot_a;
			double r = g->spot_r;

			if(a >= a1 && a < a2 && r >= r1 && r < r_to) {
				g->spot_dir = duc_opendirent(dir, e);
			}
		}

		/* Draw section for this object */

		draw_section(g, cr, a1, a2, r1, r_to, hue, brightness);

		/* Recurse into subdirectories */

		if(e->mode == DUC_MODE_DIR) {
			if(level+1 <= g->max_level) {
				duc_dir *dir_child = duc_opendirent(dir, e);
				if(!dir_child) continue;
				do_dir(g, cr, dir_child, level + 1, r_to, a1, a2);
				duc_closedir(dir_child);
			} else {
				draw_section(g, cr, a1, a2, r_to, r_to+5, hue, 0.5);
			}
		}

		/* Place labels if there is enough room to display */

		if(cr) {
			if(r1 * (a2 - a1) > 5) {
				struct label *label = malloc(sizeof *label);
				pol2car(g, ang((a1+a2)/2), (r1+r_to)/2, &label->x, &label->y);
				char siz[32];
				duc_humanize(e->size, siz, sizeof siz);
				asprintf(&label->text, "%s\n%s", e->name, siz);
				list_push(&g->label_list, label);
			}
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


int duc_graph_draw_file(duc_graph *g, duc_dir *dir, FILE *fout)
{
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, g->size, g->size);
	assert(surface);
	cairo_t *cr = cairo_create(surface);
	assert(cr);

	duc_graph_draw_cairo(g, dir, cr);

	cairo_destroy(cr);
	cairo_surface_write_to_png_stream(surface, cairo_writer, fout);
	cairo_surface_destroy(surface);

	return 0;
}


int duc_graph_draw_cairo(duc_graph *g, duc_dir *dir, cairo_t *cr)
{

	/* Recursively draw graph */
	
	duc_rewinddir(dir);

	do_dir(g, cr, dir, 1, g->r_start, 0, 1);

	/* Draw collected labels */

	struct label *label;

	while((label = list_pop(&g->label_list)) != NULL) {
		draw_text(cr, label->x, label->y, FONT_SIZE_LABEL, label->text);
		free(label->text);
		free(label);
	}

	draw_text(cr, g->cx, g->cy, 16, "cd ../");

	g->label_list = NULL;

	return 0;
}


duc_dir *duc_graph_find_spot(duc_graph *g, duc_dir *dir, int x, int y)
{
	duc_dir *dir2 = NULL;

	car2pol(g, x, y, &g->spot_a, &g->spot_r);
	double r = hypot(x - g->cx, y - g->cy);

	if(r < g->r_start) {
	
		/* If clicked in the center, go up one directory */

		dir2 = duc_opendirat(dir, "..");

	} else {

		/* Find directory at position x,y */

		g->spot_dir = NULL;

		duc_rewinddir(dir);
		do_dir(g, NULL, dir, 1, g->r_start, 0, 1);

		g->spot_a = 0;
		g->spot_r = 0;

		dir2 = g->spot_dir;
	}

	return dir2;
}


/*
 * End
 */

