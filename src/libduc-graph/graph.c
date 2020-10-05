#include "config.h"

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

#include "private.h"
#include "duc.h"
#include "duc-graph.h"
#include "utlist.h"
#include "graph-private.h"


duc_graph *duc_graph_new(duc *duc)
{
	duc_graph *g;
	
	g = duc_malloc0(sizeof *g);

	g->duc = duc;
	g->r_start = 100;
	g->fuzz = 0;
	duc_graph_set_dpi(g, 96);
	duc_graph_set_max_level(g, 3);
	duc_graph_set_size(g, 400, 400);

	return g;
}


void duc_graph_free(duc_graph *g)
{
	if(g->backend)
		g->backend->free(g);
	free(g);
}


void duc_graph_set_max_level(duc_graph *g, int max_level)
{
	g->max_level = max_level;
}


void duc_graph_set_max_name_len(duc_graph *g, size_t len)
{
	g->max_name_len = len;
}


void duc_graph_set_size(duc_graph *g, int w, int h)
{
	g->width = w;
	g->height = h;
}


void duc_graph_set_dpi(duc_graph *g, double dpi)
{
	g->dpi = dpi;
	g->font_scale = dpi / 96.0;
}


void duc_graph_set_position(duc_graph *g, double pos_x, double pos_y)
{
	g->pos_x = pos_x;
	g->pos_y = pos_y;
}


void duc_graph_set_tooltip(duc_graph *g, double x, double y)
{
	g->tooltip_x = x;
	g->tooltip_y = y;
}


void duc_graph_set_palette(duc_graph *g, enum duc_graph_palette p)
{
	g->palette = p;
}


void duc_graph_set_fuzz(duc_graph *g, double fuzz)
{
	g->fuzz = fuzz;
}


void duc_graph_set_size_type(duc_graph *g, duc_size_type st)
{
	g->size_type = st;
}


void duc_graph_set_exact_bytes(duc_graph *g, int exact)
{
	g->bytes = exact;
}


void duc_graph_set_ring_gap(duc_graph *g, int gap)
{
	g->ring_gap = gap;
}


void duc_graph_set_gradient(duc_graph *g, int onoff)
{
	g->gradient = onoff;
}


void pol2car(duc_graph *g, double a, double r, double *x, double *y)
{
	*x = cos(a) * r + g->cx;
	*y = sin(a) * r + g->cy;
}


void car2pol(duc_graph *g, double x, double y, double *a, double *r)
{
	x -= g->cx;
	y -= g->cy;
	*r = hypot(y, x);
	*a = atan2(x, -y) / (M_PI*2);
	if(*a < 0) *a += 1;
}


static void shorten_name(char *label, size_t maxlen)
{
	if(maxlen == 0) return;

	size_t n = strlen(label);
	if(n < maxlen) return;

	size_t cut1 = maxlen/2;
	size_t cut2 = n - maxlen/2;

	for(; cut1>5; cut1--) if(!isalnum(label[cut1])) break;
	for(; cut2<n-5; cut2++) if(!isalnum(label[cut2])) break;

	if(cut1 == 5) cut1 = maxlen/3;
	if(cut2 == n-5) cut2 = n - maxlen/3;

	if(cut2 > cut1 && cut1 + n - cut2 + 3 <= n) {
		label[cut1++] = '.';
		label[cut1++] = '.';
		label[cut1++] = '.';
		memmove(label+cut1, label+cut2+1, n-cut2);
	}
}


static void hsv2rgb(double h, double s, double v, double *r, double *g, double *b)
{	
	double f, m, n;
	int i;
	
	h *= 6.0;
	i = (int)floor(h);
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



double ang(double a)
{
	return -M_PI * 0.5 + M_PI * 2 * a;
}


static void gen_tooltip(duc_graph *g, struct duc_size *size, const char *name, duc_file_type type)
{
	char siz_app[32], siz_act[32], siz_cnt[32];
	duc_human_size(size, DUC_SIZE_TYPE_APPARENT, g->bytes, siz_app, sizeof siz_app);
	duc_human_size(size, DUC_SIZE_TYPE_ACTUAL, g->bytes, siz_act, sizeof siz_act);
	duc_human_size(size, DUC_SIZE_TYPE_COUNT, g->bytes, siz_cnt, sizeof siz_cnt);
	char *typ = duc_file_type_name(type);
	char *p = g->tooltip_msg;
	int l = sizeof(g->tooltip_msg);
	if(name) {
		p += snprintf(p, l, "name: %s\n", name);
	}
	p += snprintf(p, l, 
			"type: %s\n"
			"actual size: %s\n"
			"apparent size: %s\n"
			"file count: %s",
			typ, siz_act, siz_app, siz_cnt);
}


/*
 * This function has two purposes:
 *
 * - If a backend is provided, a graph will be drawn on that context
 * - if spot_a and spot_r are defined, it will find the path on the graph
 *   that is below that spot
 */

static int do_dir(duc_graph *g, duc_dir *dir, int level, double r1, double a1_dir, double a2_dir, struct duc_size *total)
{
	double a_range = a2_dir - a1_dir;
	double a1 = a1_dir;
	double a2 = a1_dir;

	double ring_width = (g->size/2 - g->r_start - 30) / g->max_level;

	/* Calculate max and total size */

	struct duc_size tmp;
	off_t size_total;
	if(total) {
		tmp = *total;
	} else {
		duc_dir_get_size(dir, &tmp);
	}
	size_total = duc_get_size(&tmp, g->size_type);
	if(size_total == 0) return 0;

	struct duc_dirent *e;

	off_t size_min = size_total;
	off_t size_max = 0;

	while( (e = duc_dir_read(dir, g->size_type, DUC_SORT_SIZE)) != NULL) {
		off_t size = duc_get_size(&e->size, g->size_type);
		if(size < size_min) size_min = size;
		if(size > size_max) size_max = size;
	}

	/* Rewind and iterate the objects to graph */

	duc_dir_rewind(dir);
	while( (e = duc_dir_read(dir, g->size_type, DUC_SORT_SIZE)) != NULL) {

		/* size_rel is size relative to total, size_nrel is size relative to min and max */

		off_t size = duc_get_size(&e->size, g->size_type);

		double size_rel = (double)size / size_total;
		double size_nrel = (size_max - size_min) ? ((double)size - size_min) / (size_max - size_min) : 1;

		double r2 = r1 + ring_width * (1 - (1 - size_nrel) * g->fuzz);
		a2 += a_range * size_rel;

		/* Skip any segments that would be smaller then one pixel */

		if(r2 * (a2 - a1) * M_PI * 2 < 1) break;
		if(a2 <= a1) break;

		/* Determine section color */

		double H = 0;
		double S = 0;
		double V = 0;
		double L = 0;

		switch(g->palette) {

			case DUC_GRAPH_PALETTE_SIZE:
				H = 0.8 - 0.7 * size_nrel - 0.1 * size_rel;
				S = 1.0 - 0.8 *(double)level / g->max_level;
				V = 1;
				L = 0;
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
				L = 0;
				break;

			case DUC_GRAPH_PALETTE_MONOCHROME:
				H = 0;
				S = 0;
				V = 1;
				L = 0.01;
				break;

			case DUC_GRAPH_PALETTE_CLASSIC:
				H = (a1 + a2) / 2 + 0.75; if(H > 1.0) H -= 1.0;
				S = 1.0 - 0.8 *(double)level / g->max_level;
				V = 1;
				L = 0.8;
				break;
		}

		double R, G, B;
		hsv2rgb(H, S, V, &R, &G, &B);
		
		if(e->type != DUC_FILE_TYPE_DIR) S *= 0.5;

		/* Check if the tooltip lies within this section */

		{
			double a = g->tooltip_a;
			double r = g->tooltip_r;

			if(a >= a1 && a < a2 && r >= r1 && r < r2) {
				V -= 0.7;
				gen_tooltip(g, &e->size, e->name, e->type);
			}
		}


		/* Check if the requested spot lies in this section */

		if(g->spot_r > 0) {
			double a = g->spot_a;
			double r = g->spot_r;

			if(a >= a1 && a < a2 && r >= r1 && r < r2) {
				g->spot_ent = duc_malloc(sizeof *g->spot_ent);
				*g->spot_ent = *e;
				g->spot_ent->name = duc_strdup(e->name);
				if(e->type == DUC_FILE_TYPE_DIR)
					g->spot_dir = duc_dir_openent(dir, e);
			}
		}


		if(e->type == DUC_FILE_TYPE_DIR) {

			/* Recurse into subdirectories */

			if(level+1 < g->max_level) {
				duc_dir *dir_child = duc_dir_openent(dir, e);
				if(!dir_child) continue;
				do_dir(g, dir_child, level + 1, r2, a1, a2, &e->size);
				duc_dir_close(dir_child);
			} else {
				if(g->backend) 
					g->backend->draw_section(g, a1, a2, r2+2, r2+8, R*0.5, G*0.5, B*0.5, L);
			}
		}


		/* Place labels if there is enough room to display */

		if(g->backend) {
			double area = (r2 - r1) * (a2 - a1);
			if(area > 1.5) {
				struct label *label = duc_malloc(sizeof *label);
			
				char siz[16];
				duc_human_size(&e->size, g->size_type, g->bytes, siz, sizeof siz);
				char *name = duc_strdup(e->name);
				shorten_name(name, g->max_name_len);

				pol2car(g, ang((a1+a2)/2), (r1+r2)/2, &label->x, &label->y);
				int r = asprintf(&label->text, "%s\n%s", name, siz);
				if (r < 0) {
				    label->text = "\0";
				}
				LL_APPEND(g->label_list, label);

				free(name);
			}
		}
		
		/* Draw section for this object */

		if(g->backend) {
			g->backend->draw_section(g, a1, a2, r1, r2 - g->ring_gap, R, G, B, L);
		}

		a1 = a2;
	}

	return 0;
}



int duc_graph_draw(duc_graph *g, duc_dir *dir)
{

	double n = g->width < g->height ? g->width : g->height;
	g->size = n;
	g->cx = g->width / 2;
	g->cy = g->height / 2;
	g->r_start = g->size / 10;

	/* Convert tooltip xy to polar coords */

	double tooltip_x = g->tooltip_x - g->pos_x;
	double tooltip_y = g->tooltip_y - g->pos_y;
	g->tooltip_msg[0] = '\0';

	car2pol(g, tooltip_x, tooltip_y, &g->tooltip_a, &g->tooltip_r);

	/* Recursively draw graph */
	
	duc_dir_rewind(dir);

	if(g->backend)
		g->backend->start(g);
	do_dir(g, dir, 0, g->r_start, 0, 1, NULL);

	/* Draw collected labels */

	struct label *l, *ln;

	LL_FOREACH_SAFE(g->label_list, l, ln) {
		if(g->backend)
			g->backend->draw_text(g, (int)l->x, (int)l->y, FONT_SIZE_LABEL * g->font_scale, l->text);
		free(l->text);
		free(l);
	}
	
	char *p = duc_dir_get_path(dir);
	if(g->backend)
		g->backend->draw_text(g, (int)g->cx, 10, FONT_SIZE_LABEL * g->font_scale, p);
	free(p);

	struct duc_size size;
	duc_dir_get_size(dir, &size);
	char siz[16];
	duc_human_size(&size, g->size_type, g->bytes, siz, sizeof siz);
	if(g->backend)
		g->backend->draw_text(g, (int)g->cx, (int)g->cy, FONT_SIZE_CENTER * g->font_scale, siz);

	if(g->tooltip_r < g->r_start) {
		gen_tooltip(g, &size, NULL, DUC_FILE_TYPE_DIR);
	}
		
	/* Draw tooltip */

	if(g->tooltip_msg[0]) {
		g->backend->draw_tooltip(g, (int)tooltip_x, (int)tooltip_y, g->tooltip_msg);
	}

	g->label_list = NULL;
	if(g->backend)
		g->backend->done(g);

	return 0;
}


duc_dir *duc_graph_find_spot(duc_graph *g, duc_dir *dir, double x, double y, struct duc_dirent **ent)
{
	duc_dir *dir2 = NULL;

	double n = g->width < g->height ? g->width : g->height;
	g->size = n;
	g->cx = g->width / 2;
	g->cy = g->height / 2;
	g->r_start = g->size / 10;

	x -= (int)g->pos_x;
	y -= (int)g->pos_y;

	car2pol(g, x, y, &g->spot_a, &g->spot_r);

	if(g->spot_r < g->r_start) {
	
		/* If clicked in the center, go up one directory */

		dir2 = duc_dir_openat(dir, "..");

	} else {

		/* Find directory at position x,y */

		g->spot_dir = NULL;

		duc_dir_rewind(dir);
		struct duc_graph_backend *be = g->backend;
		g->backend = NULL;
		do_dir(g, dir, 0, g->r_start, 0, 1, NULL);
		g->backend = be;

		g->spot_a = 0;
		g->spot_r = 0;
		if(ent) *ent = g->spot_ent;
		dir2 = g->spot_dir;
	}

	return dir2;
}

/*
 * End
 */

