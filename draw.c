
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

#include "philesight.h"
#include "db.h"

struct label {
	int x, y;
	char *text;
	struct label *next;
};


struct graph {
	int w, h;
	int cx, cy;
	int ring_width;
	int depth;
	struct label *label_list;
	cairo_t *cr;
	struct db *db;
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
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_set_line_width(cr, 1.5);
	cairo_text_path(cr, text);
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_move_to(cr, x - w/2, y + h/2);
	cairo_show_text(cr, text);
}


static void draw_section(struct graph *graph, double a_from, double a_to, int r_from, int r_to, double brightness)
{
	cairo_t *cr = graph->cr;
			
	double r, g, b;
	hsv2rgb((a_from+a_to)*0.5 / (2*M_PI), 1.0-brightness, brightness/2+0.5, &r, &g, &b);

	cairo_pattern_t *pat;
	pat = cairo_pattern_create_radial(graph->cx, graph->cy, 0, graph->cx, graph->cy, graph->cx-50);
	cairo_pattern_add_color_stop_rgb(pat, (double)r_from / graph->cx, r*0.8, g*0.8, b*0.8);
	cairo_pattern_add_color_stop_rgb(pat, (double)r_to / graph->cx, r*1.5, g*1.5, b*1.5);
	cairo_set_source(cr, pat);

	cairo_new_path(cr);
	cairo_arc(cr, graph->cx, graph->cy, r_from, a_from, a_to);
	cairo_arc_negative(cr, graph->cx, graph->cy, r_to, a_to, a_from);
	cairo_close_path(cr);

	cairo_fill_preserve(cr);

	cairo_set_line_width(cr, 0.3);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
	cairo_stroke(cr);
	cairo_pattern_destroy(pat);
}


static void draw_ring(struct graph *graph, uint32_t id, int level, double a_min, double a_max)
{
	struct db *db = graph->db;
	size_t vallen = 0;
	double a_range = a_max - a_min;

	void *buf = db_get(db, &id, sizeof id, &vallen);
	if(!buf) return;
	void *p = buf;
 			
	// +--------+---------+------+ ( +-----+-----+-----+ )
 	// |  size  | namelen | name | ( | cid | cid | ... | )
	// +--------+---------+------+ ( +-----+-----+-----+ )

	off_t size = *(off_t *)p; p += sizeof(off_t);
	uint8_t namelen = *(uint8_t *)p; p += sizeof(uint8_t);
	p += namelen;

	int nchildren = (vallen - (p - buf)) / sizeof(uint32_t);

#ifdef DEBUG
	printf("> %d %s %d\n", id, name, nchildren);
#endif
	
	double a_from = a_min;
	double a_to = a_min;

	uint32_t i;
	for(i=0; i<nchildren; i++) {
		uint32_t cid = *(uint32_t *)p;
		p += sizeof(uint32_t);
#ifdef DEBUG
		printf(">  %d\n", cid);
#endif
		size_t cvallen = 0;
		void *bufc = db_get(db, &cid, sizeof cid, &cvallen);
		if(!bufc) continue;
		
		void *pc = bufc;

		off_t csize = *(off_t *)pc; pc += sizeof(off_t);
		uint8_t cnamelen = *(uint8_t *)pc; pc += sizeof(uint8_t);
		char *cname = pc; pc += cnamelen;
		int cnchildren = (cvallen - (pc - bufc)) / sizeof(uint32_t);


		a_to += a_range * csize / size;

		if(a_to > a_from) {
			double r_from = (level+1) * graph->ring_width;
			double r_to = r_from + graph->ring_width;
		
			double brightness = r_from / graph->cx;

			draw_section(graph, a_from, a_to, r_from, r_to, brightness);

			if(cnchildren > 0) {
				if(level+1 < graph->depth) {
					draw_ring(graph, cid, level + 1, a_from, a_to);
				} else {
					draw_section(graph, a_from, a_to, r_to, r_to+5, 0.5);
				}
			}

			if(r_from * (a_to - a_from) > 40) {
				struct label *label = malloc(sizeof *label);
				pol2car(graph, (a_from+a_to)/2, (r_from+r_to)/2, &label->x, &label->y);
				label->text = malloc(cnamelen + 1);
				memcpy(label->text, cname, cnamelen);
				label->text[cnamelen] = '\0';
				label->next = graph->label_list;
				graph->label_list = label;
			}
		}
		
		db_free_val(bufc);

		a_from = a_to;
	}

	db_free_val(buf);
}


uint32_t find_id(struct db *db, char *fname)
{
	return 0;
}


int cmd_draw(struct db *db, int argc, char **argv)
{
	int i;
	struct graph graph;

	if(argc < 2) {
		fprintf(stderr, "Missing argument PATH\n");
		usage();
		return 1;
	}

	graph.w = 900;
	graph.h = 900;
	graph.depth = 6;
	graph.ring_width = 50;
	graph.label_list = NULL;

	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, graph.w, graph.h);
	cairo_t *cr = cairo_create(surface);

	graph.cx = graph.w / 2;
	graph.cy = graph.h / 2;
	graph.cr = cr;
	graph.db = db;

	uint32_t id = find_id(graph.db, "/home/ico");

	draw_ring(&graph, id, 0, 0, M_PI * 2);

	cairo_set_line_width(cr, 0.3);
	cairo_set_source_rgba(graph.cr, 0, 0, 0, 0.7);
	cairo_stroke(cr);

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
		label = label->next;
	}

	cairo_destroy(graph.cr);
	cairo_surface_write_to_png(surface, "out.png");
	cairo_surface_destroy(surface);

	return 0;
}

/*
 * End
 */

