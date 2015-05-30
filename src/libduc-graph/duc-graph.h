#include "config.h"

#ifndef duc_graph_h
#define duc_graph_h

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

typedef enum {
	DUC_GRAPH_FORMAT_PNG,
	DUC_GRAPH_FORMAT_SVG,
	DUC_GRAPH_FORMAT_PDF
} duc_graph_file_format;

#ifdef ENABLE_CAIRO
#include <cairo.h>
duc_graph *duc_graph_new_cairo(duc *duc, cairo_t *cr);
duc_graph *duc_graph_new_file(duc *duc, duc_graph_file_format fmt, FILE *fout);
#endif
#ifdef ENABLE_OPENGL
duc_graph *duc_graph_new_opengl(duc *duc);
#endif

void duc_graph_free(duc_graph *g);

void duc_graph_set_max_level(duc_graph *g, int max_level);
void duc_graph_set_geometry(duc_graph *g, int x, int y, int w, int h);
void duc_graph_set_size(duc_graph *g, int w, int h);
void duc_graph_set_position(duc_graph *g, int x, int y);
void duc_graph_set_tooltip(duc_graph *g, int x, int y);
void duc_graph_set_palette(duc_graph *g, enum duc_graph_palette p);
void duc_graph_set_fuzz(duc_graph *g, double fuzz);
void duc_graph_set_max_name_len(duc_graph *g, size_t len);
void duc_graph_set_size_type(duc_graph *g, duc_size_type st);
void duc_graph_set_exact_bytes(duc_graph *g, int exact);
void duc_graph_set_ring_gap(duc_graph *g, int gap);

int duc_graph_draw(duc_graph *g, duc_dir *dir);
duc_dir *duc_graph_find_spot(duc_graph *g, duc_dir *dir, int x, int y, struct duc_dirent *ent);

#endif
