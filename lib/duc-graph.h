
#ifndef duc_graph_h
#define duc_graph_h

#include <cairo.h>
#include <duc.h>

/* 
 * Graph drawing
 */

typedef struct duc_graph duc_graph;

enum duc_graph_palette {
	DUC_GRAPH_PALETTE_SIZE,
	DUC_GRAPH_PALETTE_RAINBOW,
	DUC_GRAPH_PALETTE_GREYSCALE,
	DUC_GRAPH_PALETTE_MONOCHROME,
};

enum duc_graph_file_format {
	DUC_GRAPH_FORMAT_PNG,
	DUC_GRAPH_FORMAT_SVG,
	DUC_GRAPH_FORMAT_PDF
};

duc_graph *duc_graph_new(duc *duc);
void duc_graph_free(duc_graph *g);

void duc_graph_set_max_level(duc_graph *g, int max_level);
void duc_graph_set_size(duc_graph *g, int size);
void duc_graph_set_position(duc_graph *g, int x, int y);
void duc_graph_set_palette(duc_graph *g, enum duc_graph_palette p);
void duc_graph_set_fuzz(duc_graph *g, double fuzz);
void duc_graph_set_max_name_len(duc_graph *g, size_t len);

int duc_graph_draw_file(duc_graph *g, duc_dir *dir, enum duc_graph_file_format fmt, FILE *fout);
int duc_graph_draw_cairo(duc_graph *g, duc_dir *dir, cairo_t *cr);
duc_dir *duc_graph_find_spot(duc_graph *g, duc_dir *dir, int x, int y);

#endif
