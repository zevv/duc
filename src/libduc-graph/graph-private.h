
#ifndef graph_private_h
#define graph_private_h

#include "duc-graph.h"

#define FONT_SIZE_LABEL 8
#define FONT_SIZE_TOOLTIP 8
#define FONT_SIZE_CENTER 14

struct label {
	double x, y;
	char *text;
	struct label *next;
};


struct duc_graph_backend {
	void (*free)(duc_graph *g);
	void (*start)(duc_graph *g);
	void (*draw_tooltip)(duc_graph *g, double x, double y, char *text);
	void (*draw_section)(duc_graph *g, double a1, double a2, double r1, double r2, double R, double G, double B, double line);
	void (*draw_bar)(duc_graph *g, double x0, double y0, double x1, double y1, double R, double G, double B);
	void (*draw_text)(duc_graph *g, double x, double y, double size, char *text);
	void (*done)(duc_graph *g);
};


struct duc_graph {

	/* Settings */

	struct duc *duc;
	double size;
	double dpi;
	double font_scale;
	double cx, cy;
	double pos_x, pos_y;
	double width, height;
	double tooltip_a, tooltip_r;
	double tooltip_x, tooltip_y;
	char tooltip_msg[DUC_PATH_MAX+256];
	double r_start;
	double fuzz;
	int max_level;
	enum duc_graph_palette palette;
	size_t max_name_len;
	duc_size_type size_type;
	int bytes;
	int ring_gap;
	int gradient;

	/* format */

	/* Reusable runtime info. Cleared after each graph_draw_* call */

	struct label *label_list;
	double spot_a;
	double spot_r;
	duc_dir *spot_dir;
	struct duc_dirent *spot_ent;

	struct duc_graph_backend *backend;
	void *backend_data;
};


duc_graph *duc_graph_new(duc *duc);
double ang(double a);
void pol2car(duc_graph *g, double a, double r, double *x, double *y);
void car2pol(duc_graph *g, double x, double y, double *a, double *r);

#endif
