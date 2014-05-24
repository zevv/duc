
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
#include <cairo-svg.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>

#include "list.h"
#include "duc.h"
#include "duc-graph.h"

#define FONT_SIZE_LABEL 8

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
	double pos_x, pos_y;
	double r_start;
	double fuzz;
	int max_level;
	enum duc_graph_palette palette;

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
	g->fuzz = 0;
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


void duc_graph_set_position(duc_graph *g, int pos_x, int pos_y)
{
	g->pos_x = pos_x;
	g->pos_y = pos_y;
}

void duc_graph_set_palette(duc_graph *g, enum duc_graph_palette p)
{
	g->palette = p;
}


void duc_graph_set_fuzz(duc_graph *g, double fuzz)
{
	g->fuzz = fuzz;
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
	cairo_set_source_rgba(cr, 1, 1, 1, 0.6);
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


static void draw_section(duc_graph *g, cairo_t *cr, double a1, double a2, double r1, double r2, double H, double S, double V, double line)
{
	if(cr == NULL) return;

	double R, G, B;
	hsv2rgb(H, S, V, &R, &G, &B);

	cairo_new_path(cr);
	cairo_arc(cr, g->cx, g->cy, r1, ang(a1), ang(a2));
	cairo_arc_negative(cr, g->cx, g->cy, r2, ang(a2), ang(a1));
	cairo_close_path(cr);

	if(R != 1.0 || G != 1.0 || B != 1.0) {
		cairo_pattern_t *pat;
		pat = cairo_pattern_create_radial(g->cx, g->cy, 0, g->cx, g->cy, g->cx-50);
		double off1 = r2 / g->cx;
		double off2 = r1 / g->cx;
		cairo_pattern_add_color_stop_rgb(pat, off1, R, G, B);
		cairo_pattern_add_color_stop_rgb(pat, off2, R * 0.6, G * 0.6, B * 0.6);
		cairo_set_source(cr, pat);

		cairo_fill_preserve(cr);
		cairo_pattern_destroy(pat);
	}

	cairo_set_line_width(cr, 0.5);
	cairo_set_source_rgba(cr, line, line, line, 0.9);
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
			
	double ring_width = (g->size/2 - g->r_start - 30) / g->max_level;

	/* Calculate max and total size */
	
	off_t size_total = duc_dir_get_size(dir);

	struct duc_dirent *e;

	size_t size_min = size_total;
	size_t size_max = 0;

	while( (e = duc_dir_read(dir)) != NULL) {
		if(e->size < size_min) size_min = e->size;
		if(e->size > size_max) size_max = e->size;
	}

	/* Rewind and iterate the objects to graph */

	duc_dir_rewind(dir);
	while( (e = duc_dir_read(dir)) != NULL) {
		
		/* size_rel is size relative to total, size_nrel is size relative to min and max */

		double size_rel = (double)e->size / size_total;
		double size_nrel = (size_max == size_min) ? 1 : ((double)e->size - size_min) / (size_max - size_min);

		double r2 = r1 + ring_width * (1 - (1 - size_nrel) * g->fuzz);
		a2 += a_range * size_rel;

		/* Skip any segments that would be smaller then one pixel */

		if(r2 * (a2 - a1) * M_PI * 2 < 2) break;
		if(a2 <= a1) break;

		/* Determine section color */

		double H, S, V, L;

		switch(g->palette) {

			case DUC_GRAPH_PALETTE_SIZE:
				H = 0.8 - 0.7 * size_nrel - 0.1 * size_rel;
				S = 1.0 - 0.8 *(double)level / g->max_level;
				V = 1;
				L = 1;
				break;

			case DUC_GRAPH_PALETTE_RAINBOW:
				H = (a1 + a2) / 2;
				S = 1.0 - 0.8 *(double)level / g->max_level;
				V = 1;
				L = 0;
				break;
			
			case DUC_GRAPH_PALETTE_GREYSCALE:
				H = 0;
				S = 0;
				V = 1.0 - 0.5 * size_nrel;
				L = 1;
				break;

			case DUC_GRAPH_PALETTE_MONOCHROME:
				H = 0;
				S = 0;
				V = 1;
				L = 0;
				break;
		}
	

		/* Check if the requested spot lies in this section */

		if(g->spot_r > 0) {
			double a = g->spot_a;
			double r = g->spot_r;

			if(a >= a1 && a < a2 && r >= r1 && r < r2) {
				g->spot_dir = duc_dir_openent(dir, e);
			}
		}

		/* Draw section for this object */

		draw_section(g, cr, a1, a2, r1, r2, H, S, V, L);

		/* Recurse into subdirectories */

		if(e->mode == DUC_MODE_DIR) {
			if(level+1 < g->max_level) {
				duc_dir *dir_child = duc_dir_openent(dir, e);
				if(!dir_child) continue;
				do_dir(g, cr, dir_child, level + 1, r2, a1, a2);
				duc_dir_close(dir_child);
			} else {
				draw_section(g, cr, a1, a2, r2, r2+5, H, S, V/2, L);
			}
		}

		/* Place labels if there is enough room to display */

		if(cr) {
			if(r1 * (a2 - a1) > 5) {
				struct label *label = malloc(sizeof *label);
				pol2car(g, ang((a1+a2)/2), (r1+r2)/2, &label->x, &label->y);
				char *siz = duc_human_size(e->size);
				asprintf(&label->text, "%s\n%s", e->name, siz);
				free(siz);
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


int duc_graph_draw_file(duc_graph *g, duc_dir *dir, enum duc_graph_file_format fmt, FILE *fout)
{
	cairo_t *cr;
	cairo_surface_t *cs;

	switch(fmt) {

		case DUC_GRAPH_FORMAT_PNG:
			cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, g->size, g->size);
			cr = cairo_create(cs);
			duc_graph_draw_cairo(g, dir, cr);
			cairo_surface_write_to_png_stream(cs, cairo_writer, fout);
			break;
		case DUC_GRAPH_FORMAT_SVG:
			cs = cairo_svg_surface_create_for_stream(cairo_writer, fout, g->size, g->size);
			cr = cairo_create(cs);
			duc_graph_draw_cairo(g, dir, cr);
			break;
		case DUC_GRAPH_FORMAT_PDF:
			cs = cairo_pdf_surface_create_for_stream(cairo_writer, fout, g->size, g->size);
			cr = cairo_create(cs);
			duc_graph_draw_cairo(g, dir, cr);
			break;
	}

	cairo_destroy(cr);
	cairo_surface_destroy(cs);

	return 0;
}


int duc_graph_draw_cairo(duc_graph *g, duc_dir *dir, cairo_t *cr)
{
	cairo_save(cr);
	cairo_translate(cr, g->pos_x, g->pos_y);

	/* Recursively draw graph */
	
	duc_dir_rewind(dir);

	do_dir(g, cr, dir, 0, g->r_start, 0, 1);

	/* Draw collected labels */

	struct label *label;

	while((label = list_pop(&g->label_list)) != NULL) {
		draw_text(cr, label->x, label->y, FONT_SIZE_LABEL, label->text);
		free(label->text);
		free(label);
	}
	
	char *p = duc_dir_get_path(dir);
	draw_text(cr, g->cx, 10, FONT_SIZE_LABEL, p);
	free(p);

	char *siz = duc_human_size(duc_dir_get_size(dir));
	draw_text(cr, g->cx, g->cy, 14, siz);
	free(siz);

	g->label_list = NULL;
	cairo_restore(cr);

	return 0;
}


duc_dir *duc_graph_find_spot(duc_graph *g, duc_dir *dir, int x, int y)
{
	duc_dir *dir2 = NULL;

	x -= g->pos_x;
	y -= g->pos_y;

	car2pol(g, x, y, &g->spot_a, &g->spot_r);
	double r = hypot(x - g->cx, y - g->cy);

	if(r < g->r_start) {
	
		/* If clicked in the center, go up one directory */

		dir2 = duc_dir_openat(dir, "..");

	} else {

		/* Find directory at position x,y */

		g->spot_dir = NULL;

		duc_dir_rewind(dir);
		do_dir(g, NULL, dir, 0, g->r_start, 0, 1);

		g->spot_a = 0;
		g->spot_r = 0;

		dir2 = g->spot_dir;
	}

	return dir2;
}


/*
 * End
 */

