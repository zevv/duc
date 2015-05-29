
#ifndef graph_private_h
#define graph_private_h

#include "duc-graph.h"

#define FONT_SIZE_LABEL 8
#define FONT_SIZE_TOOLTIP 8

#define MAX_DEPTH 32

struct label {
	int x, y;
	char *text;
	struct label *next;
};


struct duc_graph_backend {
	void (*free)(duc_graph *g);
	void (*start)(duc_graph *g);
	void (*draw_tooltip)(duc_graph *g, int x, int y, char *text);
	void (*draw_section)(duc_graph *g, double a1, double a2, double r1, double r2, double H, double S, double V, double line);
	void (*draw_text)(duc_graph *g, int x, int y, int size, char *text);
	void (*done)(duc_graph *g);
};


struct duc_graph {

	/* Settings */

	struct duc *duc;
	double size;
	double cx, cy;
	double pos_x, pos_y;
	double width, height;
	double tooltip_a, tooltip_r;
	double tooltip_x, tooltip_y;
	char tooltip_msg[256];
	double r_start;
	double fuzz;
	int max_level;
	enum duc_graph_palette palette;
	size_t max_name_len;
	duc_size_type size_type;
	int bytes;
	int ring_gap;

	/* format */

	/* Reusable runtime info. Cleared after each graph_draw_* call */

	struct label *label_list;
	double spot_a;
	double spot_r;
	duc_dir *spot_dir;
	struct duc_dirent spot_ent;

	struct duc_graph_backend *backend;
	void *backend_data;
};


duc_graph *duc_graph_new(duc *duc);
double ang(double a);
void pol2car(duc_graph *g, double a, double r, int *x, int *y);
void car2pol(duc_graph *g, int x, int y, double *a, double *r);
void shorten_name(char *label, int maxlen);
void hsv2rgb(double h, double s, double v, double *r, double *g, double *b);

#endif
