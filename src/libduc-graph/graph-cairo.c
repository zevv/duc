
#include "config.h"

#ifdef ENABLE_CAIRO

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
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

#include "private.h"
#include "duc.h"
#include "duc-graph.h"
#include "utlist.h"
#include "graph-private.h"

struct cairo_backend_data {
	cairo_surface_t *cs;
	cairo_t *cr;
	duc_graph_file_format fmt;
	FILE *fout;
};



static cairo_status_t cairo_writer(void *closure, const unsigned char *data, unsigned int length)
{
	FILE *f = closure;
	fwrite(data, length, 1, f);
	return CAIRO_STATUS_SUCCESS;
}



void br_cairo_start(duc_graph *g)
{
	struct cairo_backend_data *bd = g->backend_data;
	cairo_t *cr = bd->cr;
	cairo_save(cr);
	cairo_translate(cr, g->pos_x, g->pos_y);
}


void br_file_start(duc_graph *g)
{
	struct cairo_backend_data *bd = g->backend_data;

	switch(bd->fmt) {

		case DUC_GRAPH_FORMAT_PNG:
			bd->cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)g->size, (int)g->size);
			break;
		case DUC_GRAPH_FORMAT_SVG:
			bd->cs = cairo_svg_surface_create_for_stream(cairo_writer, bd->fout, (int)g->size, (int)g->size);
			break;
		case DUC_GRAPH_FORMAT_PDF:
			bd->cs = cairo_pdf_surface_create_for_stream(cairo_writer, bd->fout, (int)g->size, (int)g->size);
			break;
		default:
			duc_log(g->duc, DUC_LOG_FTL, "Image format not handled by cairo backend");
			exit(1);
	}
			
	bd->cr = cairo_create(bd->cs);
	cairo_translate(bd->cr, g->pos_x, g->pos_y);
}


static void br_cairo_draw_text(duc_graph *g, double x, double y, double size, char *text)
{
	struct cairo_backend_data *bd = g->backend_data;
	cairo_t *cr = bd->cr;

	char font[32];
	snprintf(font, sizeof font, "Arial, Sans, %.0f", size);
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc = pango_font_description_from_string(font);

	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_text(layout, text, -1);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	pango_cairo_update_layout(cr, layout);

	int w,h;
	pango_layout_get_size(layout, &w, &h);

	x -= ((double)w / PANGO_SCALE / 2);
	y -= ((double)h / PANGO_SCALE / 2);

	cairo_move_to(cr, floor(x+0.5), floor(y+0.5));
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


static void br_cairo_draw_tooltip(duc_graph *g, double x, double y, char *text)
{
	struct cairo_backend_data *bd = g->backend_data;
	cairo_t *cr = bd->cr;

	char font[32];
	snprintf(font, sizeof font, "Arial, Sans, %d", FONT_SIZE_TOOLTIP);
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc = pango_font_description_from_string(font);

	pango_layout_set_text(layout, text, -1);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	pango_cairo_update_layout(cr, layout);

	int w,h;
	pango_layout_get_size(layout, &w, &h);

	w /= PANGO_SCALE;
	h /= PANGO_SCALE;

	/* shadow box */

	int i;
	for(i=1; i<3; i++) {
		cairo_rectangle(cr, x - w - 10 + i, y - h - 10 + i, w + 10 + i, h + 10 + i);
		cairo_set_source_rgba(cr, 0, 0, 0, 0.25);
		cairo_fill(cr);
	}

	/* tooltip box */

	cairo_rectangle(cr, x - w - 10 + 0.5, y - h - 10 + 0.5, w + 10, h + 10);
	cairo_set_source_rgba(cr, 1, 1, 1, 1);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	cairo_stroke(cr);
	
	/* black text */

	cairo_move_to(cr, x - w - 5, y - h - 5);
	pango_cairo_layout_path(cr, layout);
	g_object_unref(layout);

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_set_line_width(cr, 1);
	cairo_fill(cr);
}


static void br_cairo_draw_section(duc_graph *g, double a1, double a2, double r1, double r2, double R, double G, double B, double L)
{
	struct cairo_backend_data *bd = g->backend_data;
	cairo_t *cr = bd->cr;

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
		if(g->gradient) {
			cairo_pattern_add_color_stop_rgb(pat, off2, R * 0.6, G * 0.6, B * 0.6);
		}
		cairo_set_source(cr, pat);

		cairo_fill_preserve(cr);
		cairo_pattern_destroy(pat);
	}

	if(L != 0.0) {
		cairo_set_line_width(cr, 0.5);
		cairo_set_source_rgba(cr, L, L, L, 0.9);
		cairo_stroke(cr);
	}
}



void br_cairo_done(duc_graph *g)
{
	struct cairo_backend_data *bd = g->backend_data;
	cairo_t *cr = bd->cr;
	cairo_restore(cr);
}


void br_file_done(duc_graph *g)
{
	struct cairo_backend_data *bd = g->backend_data;

	switch(bd->fmt) {
		case DUC_GRAPH_FORMAT_PNG:
			cairo_surface_write_to_png_stream(bd->cs, cairo_writer, bd->fout);
			break;
		default:
			break;
	}

	cairo_destroy(bd->cr);
	cairo_surface_destroy(bd->cs);
}


static void br_cairo_free(duc_graph *g)
{
	struct cairo_backend_data *bd = g->backend_data;
	free(bd);
}


struct duc_graph_backend duc_graph_backend_cairo = {
	.start = br_cairo_start,
	.draw_text = br_cairo_draw_text,
	.draw_tooltip = br_cairo_draw_tooltip,
	.draw_section = br_cairo_draw_section,
	.done = br_cairo_done,
	.free = br_cairo_free,
};


struct duc_graph_backend duc_graph_backend_file = {
	.start = br_file_start,
	.draw_text = br_cairo_draw_text,
	.draw_tooltip = br_cairo_draw_tooltip,
	.draw_section = br_cairo_draw_section,
	.done = br_file_done,
	.free = br_cairo_free,
};


duc_graph *duc_graph_new_cairo(duc *duc, cairo_t *cr)
{
	duc_graph *g = duc_graph_new(duc);
	g->backend = &duc_graph_backend_cairo;

	struct cairo_backend_data *bd;
	bd = duc_malloc(sizeof *bd);
	g->backend_data = bd;

	bd->cr = cr;

	return g;
}


duc_graph *duc_graph_new_cairo_file(duc *duc, duc_graph_file_format fmt, FILE *fout)
{
	duc_graph *g = duc_graph_new(duc);
	g->backend = &duc_graph_backend_file;

	struct cairo_backend_data *bd;
	bd = duc_malloc(sizeof *bd);
	g->backend_data = bd;
	
	bd->fmt = fmt;
	bd->fout = fout;

	return g;
}

#endif

/*
 * End
 */

